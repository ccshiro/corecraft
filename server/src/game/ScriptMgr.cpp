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

#include "ScriptMgr.h"
#include "ConditionMgr.h"
#include "CreatureAI.h"
#include "GameObjectAI.h"
#include "logging.h"
#include "ObjectMgr.h"
#include "ProgressBar.h"
#include "SpecialVisCreature.h"
#include "Totem.h"
#include "TemporarySummon.h"
#include "World.h"
#include "maps/checks.h"
#include "maps/visitors.h"
#include "movement/IdleMovementGenerator.h"
#include "movement/PointMovementGenerator.h"
#include "movement/RandomMovementGenerator.h"
#include "movement/WaypointManager.h"
#include "movement/WaypointMovementGenerator.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "Policies/Singleton.h"
#include <stdexcept>

ScriptMapMapName sQuestEndScripts;
ScriptMapMapName sQuestStartScripts;
ScriptMapMapName sSpellScripts;
ScriptMapMapName sGameObjectScripts;
ScriptMapMapName sEventScripts;
ScriptMapMapName sGossipScripts;
ScriptMapMapName sCreatureMovementScripts;

ScriptMgr::ScriptMgr()
  : script_library_(nullptr), m_scheduledScripts(0),
    m_pOnInitScriptLibrary(nullptr), m_pOnFreeScriptLibrary(nullptr),
    m_pGetCreatureAI(nullptr), m_pCreateInstanceData(nullptr),
    m_pOnGossipHello(nullptr), m_pOnGOGossipHello(nullptr),
    m_pOnGossipSelect(nullptr), m_pOnGOGossipSelect(nullptr),
    m_pOnGossipSelectWithCode(nullptr), m_pOnGOGossipSelectWithCode(nullptr),
    m_pOnQuestAccept(nullptr), m_pOnGOQuestAccept(nullptr),
    m_pOnItemQuestAccept(nullptr), m_pOnQuestRewarded(nullptr),
    m_pOnGOQuestRewarded(nullptr), m_pGetNPCDialogStatus(nullptr),
    m_pGetGODialogStatus(nullptr), m_pOnGOUse(nullptr), m_pOnItemUse(nullptr),
    m_pOnAreaTrigger(nullptr), m_pOnProcessEvent(nullptr),
    m_pOnEffectDummyCreature(nullptr), m_pOnEffectDummyGO(nullptr),
    m_pOnEffectDummyItem(nullptr), m_pOnAuraDummy(nullptr),
    m_pOnConditionCheck(nullptr)
{
}

ScriptMgr::~ScriptMgr()
{
    UnloadScriptLibrary();
}

// /////////////////////////////////////////////////////////
//              DB SCRIPTS (loaders of static data)
// /////////////////////////////////////////////////////////

void ScriptMgr::LoadScripts(ScriptMapMapName& scripts, const char* tablename)
{
    if (IsScriptScheduled()) // function don't must be called in time scripts
                             // use.
        return;

    logging.info("Loading script table `%s`:", tablename);

    scripts.first = tablename;
    scripts.second.clear(); // need for reload support

    std::unique_ptr<QueryResult> result(WorldDatabase.PQuery(
        "SELECT id, delay, command, datalong, datalong2, buddy_entry, "
        "search_radius, data_flags, dataint, dataint2, dataint3, dataint4, x, "
        "y, z, o FROM %s",
        tablename));

    uint32 count = 0;

    if (!result)
    {
        logging.info("Loaded %u script definitions\n", count);
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        bar.step();

        Field* fields = result->Fetch();

        ScriptInfo tmp;
        tmp.id = fields[0].GetUInt32();
        tmp.delay = fields[1].GetUInt32();
        tmp.command = fields[2].GetUInt32();
        tmp.raw.data[0] = fields[3].GetUInt32();
        tmp.raw.data[1] = fields[4].GetUInt32();
        tmp.buddyEntry = fields[5].GetUInt32();
        tmp.searchRadius = fields[6].GetUInt32();
        tmp.data_flags = fields[7].GetUInt8();
        tmp.textId[0] = fields[8].GetInt32();
        tmp.textId[1] = fields[9].GetInt32();
        tmp.textId[2] = fields[10].GetInt32();
        tmp.textId[3] = fields[11].GetInt32();
        tmp.x = fields[12].GetFloat();
        tmp.y = fields[13].GetFloat();
        tmp.z = fields[14].GetFloat();
        tmp.o = fields[15].GetFloat();

        // generic command args check
        if (tmp.buddyEntry) // Check Buddy args
        {
            if (tmp.IsCreatureBuddy() &&
                !ObjectMgr::GetCreatureTemplate(tmp.buddyEntry))
            {
                logging.error(
                    "Table `%s` has buddyEntry = %u in command %u for script "
                    "id %u, but this creature_template does not exist, "
                    "skipping.",
                    tablename, tmp.buddyEntry, tmp.command, tmp.id);
                continue;
            }
            else if (!tmp.IsCreatureBuddy() &&
                     !ObjectMgr::GetGameObjectInfo(tmp.buddyEntry))
            {
                logging.error(
                    "Table `%s` has buddyEntry = %u in command %u for script "
                    "id %u, but this gameobject_template does not exist, "
                    "skipping.",
                    tablename, tmp.buddyEntry, tmp.command, tmp.id);
                continue;
            }
            if (!tmp.searchRadius)
            {
                logging.error(
                    "Table `%s` has searchRadius = 0 in command %u for script "
                    "id %u for buddy %u, skipping.",
                    tablename, tmp.command, tmp.id, tmp.buddyEntry);
                continue;
            }
        }

        if (tmp.data_flags) // Check flags
        {
            if (tmp.data_flags & ~(SCRIPT_FLAG_COMMAND_ADDITIONAL * 2 - 1))
            {
                logging.error(
                    "Table `%s` has invalid data_flags %u in command %u for "
                    "script id %u, skipping.",
                    tablename, tmp.data_flags, tmp.command, tmp.id);
                continue;
            }
            if (!tmp.HasAdditionalScriptFlag() &&
                tmp.data_flags & SCRIPT_FLAG_COMMAND_ADDITIONAL)
            {
                logging.error(
                    "Table `%s` has invalid data_flags %u in command %u for "
                    "script id %u, skipping.",
                    tablename, tmp.data_flags, tmp.command, tmp.id);
                continue;
            }
            if (tmp.data_flags & SCRIPT_FLAG_BUDDY_AS_TARGET && !tmp.buddyEntry)
            {
                logging.error(
                    "Table `%s` has buddy required in data_flags %u in command "
                    "%u for script id %u, but no buddy defined, skipping.",
                    tablename, tmp.data_flags, tmp.command, tmp.id);
                continue;
            }
        }

        switch (tmp.command)
        {
        case SCRIPT_COMMAND_TALK:
        {
            if (tmp.talk.chatType > CHAT_TYPE_ZONE_YELL)
            {
                logging.error(
                    "Table `%s` has invalid CHAT_TYPE_ (datalong = %u) in "
                    "SCRIPT_COMMAND_TALK for script id %u",
                    tablename, tmp.talk.chatType, tmp.id);
                continue;
            }

            if (!GetLanguageDescByID(tmp.talk.language))
            {
                logging.error(
                    "Table `%s` has datalong2 = %u in SCRIPT_COMMAND_TALK for "
                    "script id %u, but this language does not exist.",
                    tablename, tmp.talk.language, tmp.id);
                continue;
            }

            if (tmp.textId[0] == 0)
            {
                logging.error(
                    "Table `%s` has invalid talk text id (dataint = %i) in "
                    "SCRIPT_COMMAND_TALK for script id %u",
                    tablename, tmp.textId[0], tmp.id);
                continue;
            }

            for (int i = 0; i < MAX_TEXT_ID; ++i)
            {
                if (tmp.textId[i] &&
                    (tmp.textId[i] < MIN_DB_SCRIPT_STRING_ID ||
                        tmp.textId[i] >= MAX_DB_SCRIPT_STRING_ID))
                {
                    logging.error(
                        "Table `%s` has out of range text id (dataint = %i "
                        "expected %u-%u) in SCRIPT_COMMAND_TALK for script id "
                        "%u",
                        tablename, tmp.textId[i], MIN_DB_SCRIPT_STRING_ID,
                        MAX_DB_SCRIPT_STRING_ID, tmp.id);
                    continue;
                }
            }

            // if (!GetMangosStringLocale(tmp.dataint)) will be checked after
            // db_script_string loading
            break;
        }
        case SCRIPT_COMMAND_EMOTE:
        {
            if (!sEmotesStore.LookupEntry(tmp.emote.emoteId))
            {
                logging.error(
                    "Table `%s` has invalid emote id (datalong = %u) in "
                    "SCRIPT_COMMAND_EMOTE for script id %u",
                    tablename, tmp.emote.emoteId, tmp.id);
                continue;
            }
            break;
        }
        case SCRIPT_COMMAND_TELEPORT_TO:
        {
            if (!sMapStore.LookupEntry(tmp.teleportTo.mapId))
            {
                logging.error(
                    "Table `%s` has invalid map (Id: %u) in "
                    "SCRIPT_COMMAND_TELEPORT_TO for script id %u",
                    tablename, tmp.teleportTo.mapId, tmp.id);
                continue;
            }

            if (!maps::verify_coords(tmp.x, tmp.y))
            {
                logging.error(
                    "Table `%s` has invalid coordinates (X: %f Y: %f) in "
                    "SCRIPT_COMMAND_TELEPORT_TO for script id %u",
                    tablename, tmp.x, tmp.y, tmp.id);
                continue;
            }
            break;
        }
        case SCRIPT_COMMAND_QUEST_EXPLORED:
        {
            Quest const* quest = sObjectMgr::Instance()->GetQuestTemplate(
                tmp.questExplored.questId);
            if (!quest)
            {
                logging.error(
                    "Table `%s` has invalid quest (ID: %u) in "
                    "SCRIPT_COMMAND_QUEST_EXPLORED in `datalong` for script id "
                    "%u",
                    tablename, tmp.questExplored.questId, tmp.id);
                continue;
            }

            if (!quest->HasSpecialFlag(QUEST_SPECIAL_FLAG_EXPLORATION_OR_EVENT))
            {
                logging.error(
                    "Table `%s` has quest (ID: %u) in "
                    "SCRIPT_COMMAND_QUEST_EXPLORED in `datalong` for script id "
                    "%u, but quest not have flag "
                    "QUEST_SPECIAL_FLAG_EXPLORATION_OR_EVENT in quest flags. "
                    "Script command or quest flags wrong. Quest modified to "
                    "require objective.",
                    tablename, tmp.questExplored.questId, tmp.id);

                // this will prevent quest completing without objective
                const_cast<Quest*>(quest)->SetSpecialFlag(
                    QUEST_SPECIAL_FLAG_EXPLORATION_OR_EVENT);

                // continue; - quest objective requirement set and command can
                // be allowed
            }

            if (float(tmp.questExplored.distance) > DEFAULT_VISIBILITY_DISTANCE)
            {
                logging.error(
                    "Table `%s` has too large distance (%u) for exploring "
                    "objective complete in `datalong2` in "
                    "SCRIPT_COMMAND_QUEST_EXPLORED in `datalong` for script id "
                    "%u",
                    tablename, tmp.questExplored.distance, tmp.id);
                continue;
            }

            if (tmp.questExplored.distance &&
                float(tmp.questExplored.distance) > DEFAULT_VISIBILITY_DISTANCE)
            {
                logging.error(
                    "Table `%s` has too large distance (%u) for exploring "
                    "objective complete in `datalong2` in "
                    "SCRIPT_COMMAND_QUEST_EXPLORED in `datalong` for script id "
                    "%u, max distance is %f or 0 for disable distance check",
                    tablename, tmp.questExplored.distance, tmp.id,
                    DEFAULT_VISIBILITY_DISTANCE);
                continue;
            }

            if (tmp.questExplored.distance &&
                float(tmp.questExplored.distance) < INTERACTION_DISTANCE)
            {
                logging.error(
                    "Table `%s` has too small distance (%u) for exploring "
                    "objective complete in `datalong2` in "
                    "SCRIPT_COMMAND_QUEST_EXPLORED in `datalong` for script id "
                    "%u, min distance is %f or 0 for disable distance check",
                    tablename, tmp.questExplored.distance, tmp.id,
                    INTERACTION_DISTANCE);
                continue;
            }

            break;
        }
        case SCRIPT_COMMAND_KILL_CREDIT:
        {
            if (!ObjectMgr::GetCreatureTemplate(tmp.killCredit.creatureEntry))
            {
                logging.error(
                    "Table `%s` has invalid creature (Entry: %u) in "
                    "SCRIPT_COMMAND_KILL_CREDIT for script id %u",
                    tablename, tmp.killCredit.creatureEntry, tmp.id);
                continue;
            }
            break;
        }
        case SCRIPT_COMMAND_RESPAWN_GAMEOBJECT:
        {
            uint32 goEntry = 0;

            if (!tmp.GetGOGuid())
            {
                if (!tmp.buddyEntry)
                {
                    logging.error(
                        "Table `%s` has no gameobject nor buddy defined in "
                        "SCRIPT_COMMAND_RESPAWN_GAMEOBJECT for script id %u",
                        tablename, tmp.id);
                    continue;
                }
                goEntry = tmp.buddyEntry;
            }
            else
            {
                GameObjectData const* data =
                    sObjectMgr::Instance()->GetGOData(tmp.GetGOGuid());
                if (!data)
                {
                    logging.error(
                        "Table `%s` has invalid gameobject (GUID: %u) in "
                        "SCRIPT_COMMAND_RESPAWN_GAMEOBJECT for script id %u",
                        tablename, tmp.GetGOGuid(), tmp.id);
                    continue;
                }
                goEntry = data->id;
            }

            GameObjectInfo const* info = ObjectMgr::GetGameObjectInfo(goEntry);
            if (!info)
            {
                logging.error(
                    "Table `%s` has gameobject with invalid entry (GUID: %u "
                    "Entry: %u) in SCRIPT_COMMAND_RESPAWN_GAMEOBJECT for "
                    "script id %u",
                    tablename, tmp.GetGOGuid(), goEntry, tmp.id);
                continue;
            }

            if (info->type == GAMEOBJECT_TYPE_FISHINGNODE ||
                info->type == GAMEOBJECT_TYPE_FISHINGHOLE ||
                info->type == GAMEOBJECT_TYPE_DOOR ||
                info->type == GAMEOBJECT_TYPE_BUTTON ||
                info->type == GAMEOBJECT_TYPE_TRAP)
            {
                logging.error(
                    "Table `%s` have gameobject type (%u) unsupported by "
                    "command SCRIPT_COMMAND_RESPAWN_GAMEOBJECT for script id "
                    "%u",
                    tablename, info->id, tmp.id);
                continue;
            }
            break;
        }
        case SCRIPT_COMMAND_TEMP_SUMMON_CREATURE:
        {
            if (!maps::verify_coords(tmp.x, tmp.y))
            {
                logging.error(
                    "Table `%s` has invalid coordinates (X: %f Y: %f) in "
                    "SCRIPT_COMMAND_TEMP_SUMMON_CREATURE for script id %u",
                    tablename, tmp.x, tmp.y, tmp.id);
                continue;
            }

            if (!ObjectMgr::GetCreatureTemplate(
                    tmp.summonCreature.creatureEntry))
            {
                logging.error(
                    "Table `%s` has invalid creature (Entry: %u) in "
                    "SCRIPT_COMMAND_TEMP_SUMMON_CREATURE for script id %u",
                    tablename, tmp.summonCreature.creatureEntry, tmp.id);
                continue;
            }
            break;
        }
        case SCRIPT_COMMAND_OPEN_DOOR:
        case SCRIPT_COMMAND_CLOSE_DOOR:
        {
            uint32 goEntry = 0;

            if (!tmp.GetGOGuid())
            {
                if (!tmp.buddyEntry)
                {
                    logging.error(
                        "Table `%s` has no gameobject nor buddy defined in %s "
                        "for script id %u",
                        tablename, (tmp.command == SCRIPT_COMMAND_OPEN_DOOR ?
                                           "SCRIPT_COMMAND_OPEN_DOOR" :
                                           "SCRIPT_COMMAND_CLOSE_DOOR"),
                        tmp.id);
                    continue;
                }
                goEntry = tmp.buddyEntry;
            }
            else
            {
                GameObjectData const* data =
                    sObjectMgr::Instance()->GetGOData(tmp.GetGOGuid());
                if (!data)
                {
                    logging.error(
                        "Table `%s` has invalid gameobject (GUID: %u) in %s "
                        "for script id %u",
                        tablename, tmp.GetGOGuid(),
                        (tmp.command == SCRIPT_COMMAND_OPEN_DOOR ?
                                "SCRIPT_COMMAND_OPEN_DOOR" :
                                "SCRIPT_COMMAND_CLOSE_DOOR"),
                        tmp.id);
                    continue;
                }
                goEntry = data->id;
            }

            GameObjectInfo const* info = ObjectMgr::GetGameObjectInfo(goEntry);
            if (!info)
            {
                logging.error(
                    "Table `%s` has gameobject with invalid entry (GUID: %u "
                    "Entry: %u) in %s for script id %u",
                    tablename, tmp.GetGOGuid(), goEntry,
                    (tmp.command == SCRIPT_COMMAND_OPEN_DOOR ?
                            "SCRIPT_COMMAND_OPEN_DOOR" :
                            "SCRIPT_COMMAND_CLOSE_DOOR"),
                    tmp.id);
                continue;
            }

            if (info->type != GAMEOBJECT_TYPE_DOOR)
            {
                logging.error(
                    "Table `%s` has gameobject type (%u) non supported by "
                    "command %s for script id %u",
                    tablename, info->id,
                    (tmp.command == SCRIPT_COMMAND_OPEN_DOOR ?
                            "SCRIPT_COMMAND_OPEN_DOOR" :
                            "SCRIPT_COMMAND_CLOSE_DOOR"),
                    tmp.id);
                continue;
            }

            break;
        }
        case SCRIPT_COMMAND_REMOVE_AURA:
        {
            if (!sSpellStore.LookupEntry(tmp.removeAura.spellId))
            {
                logging.error(
                    "Table `%s` using nonexistent spell (id: %u) in "
                    "SCRIPT_COMMAND_REMOVE_AURA or SCRIPT_COMMAND_CAST_SPELL "
                    "for script id %u",
                    tablename, tmp.removeAura.spellId, tmp.id);
                continue;
            }
            break;
        }
        case SCRIPT_COMMAND_CAST_SPELL:
        {
            if (!sSpellStore.LookupEntry(tmp.castSpell.spellId))
            {
                logging.error(
                    "Table `%s` using nonexistent spell (id: %u) in "
                    "SCRIPT_COMMAND_REMOVE_AURA or SCRIPT_COMMAND_CAST_SPELL "
                    "for script id %u",
                    tablename, tmp.castSpell.spellId, tmp.id);
                continue;
            }
            break;
        }
        case SCRIPT_COMMAND_CREATE_ITEM:
        {
            if (!ObjectMgr::GetItemPrototype(tmp.createItem.itemEntry))
            {
                logging.error(
                    "Table `%s` has nonexistent item (entry: %u) in "
                    "SCRIPT_COMMAND_CREATE_ITEM for script id %u",
                    tablename, tmp.createItem.itemEntry, tmp.id);
                continue;
            }
            if (!tmp.createItem.amount)
            {
                logging.error(
                    "Table `%s` SCRIPT_COMMAND_CREATE_ITEM but amount is %u "
                    "for script id %u",
                    tablename, tmp.createItem.amount, tmp.id);
                continue;
            }
            break;
        }
        case SCRIPT_COMMAND_DESPAWN_SELF:
        {
            // for later, we might consider despawn by database guid, and define
            // in datalong2 as option to despawn self.
            break;
        }
        case SCRIPT_COMMAND_PLAY_MOVIE:
        {
            logging.error(
                "Table `%s` use unsupported SCRIPT_COMMAND_PLAY_MOVIE for "
                "script id %u",
                tablename, tmp.id);
            continue;
        }
        case SCRIPT_COMMAND_MOVEMENT:
        {
            if (tmp.movement.movementType > (int)movement::gen::random_waterair)
            {
                logging.error(
                    "Table `%s` SCRIPT_COMMAND_MOVEMENT has invalid "
                    "MovementType %u for script id %u",
                    tablename, tmp.movement.movementType, tmp.id);
                continue;
            }

            break;
        }
        case SCRIPT_COMMAND_SET_ACTIVEOBJECT:
        {
            break;
        }
        case SCRIPT_COMMAND_SET_FACTION:
        {
            if (tmp.faction.factionId &&
                !sFactionTemplateStore.LookupEntry(tmp.faction.factionId))
            {
                logging.error(
                    "Table `%s` has datalong = %u in "
                    "SCRIPT_COMMAND_SET_FACTION for script id %u, but this "
                    "faction-template does not exist.",
                    tablename, tmp.faction.factionId, tmp.id);
                continue;
            }

            break;
        }
        case SCRIPT_COMMAND_MORPH_TO_ENTRY_OR_MODEL:
        {
            if (tmp.data_flags & SCRIPT_FLAG_COMMAND_ADDITIONAL)
            {
                if (tmp.morph.creatureOrModelEntry &&
                    !sCreatureDisplayInfoStore.LookupEntry(
                        tmp.morph.creatureOrModelEntry))
                {
                    logging.error(
                        "Table `%s` has datalong2 = %u in "
                        "SCRIPT_COMMAND_MORPH_TO_ENTRY_OR_MODEL for script id "
                        "%u, but this model does not exist.",
                        tablename, tmp.morph.creatureOrModelEntry, tmp.id);
                    continue;
                }
            }
            else
            {
                if (tmp.morph.creatureOrModelEntry &&
                    !ObjectMgr::GetCreatureTemplate(
                        tmp.morph.creatureOrModelEntry))
                {
                    logging.error(
                        "Table `%s` has datalong2 = %u in "
                        "SCRIPT_COMMAND_MORPH_TO_ENTRY_OR_MODEL for script id "
                        "%u, but this creature_template does not exist.",
                        tablename, tmp.morph.creatureOrModelEntry, tmp.id);
                    continue;
                }
            }

            break;
        }
        case SCRIPT_COMMAND_MOUNT_TO_ENTRY_OR_MODEL:
        {
            if (tmp.data_flags & SCRIPT_FLAG_COMMAND_ADDITIONAL)
            {
                if (tmp.mount.creatureOrModelEntry &&
                    !sCreatureDisplayInfoStore.LookupEntry(
                        tmp.mount.creatureOrModelEntry))
                {
                    logging.error(
                        "Table `%s` has datalong2 = %u in "
                        "SCRIPT_COMMAND_MOUNT_TO_ENTRY_OR_MODEL for script id "
                        "%u, but this model does not exist.",
                        tablename, tmp.mount.creatureOrModelEntry, tmp.id);
                    continue;
                }
            }
            else
            {
                if (tmp.mount.creatureOrModelEntry &&
                    !ObjectMgr::GetCreatureTemplate(
                        tmp.mount.creatureOrModelEntry))
                {
                    logging.error(
                        "Table `%s` has datalong2 = %u in "
                        "SCRIPT_COMMAND_MOUNT_TO_ENTRY_OR_MODEL for script id "
                        "%u, but this creature_template does not exist.",
                        tablename, tmp.mount.creatureOrModelEntry, tmp.id);
                    continue;
                }
            }

            break;
        }
        case SCRIPT_COMMAND_SET_RUN:
        {
            break;
        }
        case SCRIPT_COMMAND_ATTACK_START:
        {
            break;
        }
        case SCRIPT_COMMAND_GO_LOCK_STATE:
        {
            if ( // lock(0x01) and unlock(0x02) together
                ((tmp.goLockState.lockState & 0x01) &&
                    (tmp.goLockState.lockState & 0x02)) ||
                // non-interact (0x4) and interact (0x08) together
                ((tmp.goLockState.lockState & 0x04) &&
                    (tmp.goLockState.lockState & 0x08)) ||
                // no setting
                !tmp.goLockState.lockState ||
                // invalid number
                tmp.goLockState.lockState >= 0x10)
            {
                logging.error(
                    "Table `%s` has invalid lock state (datalong = %u) in "
                    "SCRIPT_COMMAND_GO_LOCK_STATE for script id %u.",
                    tablename, tmp.goLockState.lockState, tmp.id);
                continue;
            }
            break;
        }
        case SCRIPT_COMMAND_STAND_STATE:
        {
            if (tmp.standState.stand_state >= MAX_UNIT_STAND_STATE)
            {
                logging.error(
                    "Table `%s` has invalid stand state (datalong = %u) in "
                    "SCRIPT_COMMAND_STAND_STATE for script id %u",
                    tablename, tmp.standState.stand_state, tmp.id);
                continue;
            }
            break;
        }
        case SCRIPT_COMMAND_MODIFY_NPC_FLAGS:
        {
            break;
        }
        }

        if (scripts.second.find(tmp.id) == scripts.second.end())
        {
            ScriptMap emptyMap;
            scripts.second[tmp.id] = emptyMap;
        }
        scripts.second[tmp.id].insert(ScriptMap::value_type(tmp.delay, tmp));

        ++count;
    } while (result->NextRow());

    logging.info("Loaded %u script definitions\n", count);
}

void ScriptMgr::LoadGameObjectScripts()
{
    LoadScripts(sGameObjectScripts, "gameobject_scripts");

    // check ids
    for (ScriptMapMap::const_iterator itr = sGameObjectScripts.second.begin();
         itr != sGameObjectScripts.second.end(); ++itr)
    {
        if (!sObjectMgr::Instance()->GetGOData(itr->first))
            logging.error(
                "Table `gameobject_scripts` has not existing gameobject (GUID: "
                "%u) as script id",
                itr->first);
    }
}

void ScriptMgr::LoadQuestEndScripts()
{
    LoadScripts(sQuestEndScripts, "quest_end_scripts");

    // check ids
    for (ScriptMapMap::const_iterator itr = sQuestEndScripts.second.begin();
         itr != sQuestEndScripts.second.end(); ++itr)
    {
        if (!sObjectMgr::Instance()->GetQuestTemplate(itr->first))
            logging.error(
                "Table `quest_end_scripts` has not existing quest (Id: %u) as "
                "script id",
                itr->first);
    }
}

void ScriptMgr::LoadQuestStartScripts()
{
    LoadScripts(sQuestStartScripts, "quest_start_scripts");

    // check ids
    for (ScriptMapMap::const_iterator itr = sQuestStartScripts.second.begin();
         itr != sQuestStartScripts.second.end(); ++itr)
    {
        if (!sObjectMgr::Instance()->GetQuestTemplate(itr->first))
            logging.error(
                "Table `quest_start_scripts` has not existing quest (Id: %u) "
                "as script id",
                itr->first);
    }
}

void ScriptMgr::LoadSpellScripts()
{
    LoadScripts(sSpellScripts, "spell_scripts");

    // check ids
    for (ScriptMapMap::const_iterator itr = sSpellScripts.second.begin();
         itr != sSpellScripts.second.end(); ++itr)
    {
        SpellEntry const* spellInfo = sSpellStore.LookupEntry(itr->first);

        if (!spellInfo)
        {
            logging.error(
                "Table `spell_scripts` has not existing spell (Id: %u) as "
                "script id",
                itr->first);
            continue;
        }

        // check for correct spellEffect
        bool found = false;
        for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
        {
            // skip empty effects
            if (!spellInfo->Effect[i])
                continue;

            if (spellInfo->Effect[i] == SPELL_EFFECT_SCRIPT_EFFECT)
            {
                found = true;
                break;
            }
        }

        if (!found)
            logging.error(
                "Table `spell_scripts` has unsupported spell (Id: %u) without "
                "SPELL_EFFECT_SCRIPT_EFFECT (%u) spell effect",
                itr->first, SPELL_EFFECT_SCRIPT_EFFECT);
    }
}

void ScriptMgr::LoadEventScripts()
{
    LoadScripts(sEventScripts, "event_scripts");

    std::set<uint32> evt_scripts;

    // Load all possible script entries from gameobjects
    for (uint32 i = 1; i < sGOStorage.MaxEntry; ++i)
    {
        if (GameObjectInfo const* goInfo =
                sGOStorage.LookupEntry<GameObjectInfo>(i))
        {
            if (uint32 eventId = goInfo->GetEventScriptId())
                evt_scripts.insert(eventId);

            if (goInfo->type == GAMEOBJECT_TYPE_CAPTURE_POINT)
            {
                evt_scripts.insert(goInfo->capturePoint.neutralEventID1);
                evt_scripts.insert(goInfo->capturePoint.neutralEventID2);
                evt_scripts.insert(goInfo->capturePoint.contestedEventID1);
                evt_scripts.insert(goInfo->capturePoint.contestedEventID2);
                evt_scripts.insert(goInfo->capturePoint.progressEventID1);
                evt_scripts.insert(goInfo->capturePoint.progressEventID2);
                evt_scripts.insert(goInfo->capturePoint.winEventID1);
                evt_scripts.insert(goInfo->capturePoint.winEventID2);
            }
        }
    }

    // Load all possible script entries from spells
    for (uint32 i = 1; i < sSpellStore.GetNumRows(); ++i)
    {
        SpellEntry const* spell = sSpellStore.LookupEntry(i);
        if (spell)
        {
            for (int j = 0; j < MAX_EFFECT_INDEX; ++j)
            {
                if (spell->Effect[j] == SPELL_EFFECT_SEND_EVENT)
                {
                    if (spell->EffectMiscValue[j])
                        evt_scripts.insert(spell->EffectMiscValue[j]);
                }
            }
        }
    }

    for (auto& elem : sTaxiPathNodesByPath)
    {
        for (size_t node_idx = 0; node_idx < elem.size(); ++node_idx)
        {
            TaxiPathNodeEntry const& node = elem[node_idx];

            if (node.arrivalEventID)
                evt_scripts.insert(node.arrivalEventID);

            if (node.departureEventID)
                evt_scripts.insert(node.departureEventID);
        }
    }

    // Then check if all scripts are in above list of possible script entries
    for (ScriptMapMap::const_iterator itr = sEventScripts.second.begin();
         itr != sEventScripts.second.end(); ++itr)
    {
        auto itr2 = evt_scripts.find(itr->first);
        if (itr2 == evt_scripts.end())
            logging.error(
                "Table `event_scripts` has script (Id: %u) not referring to "
                "any gameobject_template type 10 data2 field, type 3 data6 "
                "field, type 13 data 2 field, type 29 or any spell effect %u "
                "or path taxi node data",
                itr->first, SPELL_EFFECT_SEND_EVENT);
    }
}

void ScriptMgr::LoadGossipScripts()
{
    LoadScripts(sGossipScripts, "gossip_scripts");

    // checks are done in LoadGossipMenuItems and LoadGossipMenu
}

void ScriptMgr::LoadCreatureMovementScripts()
{
    LoadScripts(sCreatureMovementScripts, "creature_movement_scripts");

    // checks are done in WaypointManager::Load
}

void ScriptMgr::LoadDbScriptStrings()
{
    sObjectMgr::Instance()->LoadMangosStrings(WorldDatabase, "db_script_string",
        MIN_DB_SCRIPT_STRING_ID, MAX_DB_SCRIPT_STRING_ID);

    std::set<int32> ids;

    for (int32 i = MIN_DB_SCRIPT_STRING_ID; i < MAX_DB_SCRIPT_STRING_ID; ++i)
        if (sObjectMgr::Instance()->GetMangosStringLocale(i))
            ids.insert(i);

    CheckScriptTexts(sQuestEndScripts, ids);
    CheckScriptTexts(sQuestStartScripts, ids);
    CheckScriptTexts(sSpellScripts, ids);
    CheckScriptTexts(sGameObjectScripts, ids);
    CheckScriptTexts(sEventScripts, ids);
    CheckScriptTexts(sGossipScripts, ids);
    CheckScriptTexts(sCreatureMovementScripts, ids);

    sWaypointMgr::Instance()->CheckTextsExistance(ids);

    for (const auto& id : ids)
        logging.error("Table `db_script_string` has unused string id %u", id);
}

void ScriptMgr::CheckScriptTexts(
    ScriptMapMapName const& scripts, std::set<int32>& ids)
{
    for (auto itrMM = scripts.second.begin(); itrMM != scripts.second.end();
         ++itrMM)
    {
        for (auto itrM = itrMM->second.begin(); itrM != itrMM->second.end();
             ++itrM)
        {
            if (itrM->second.command == SCRIPT_COMMAND_TALK)
            {
                for (int i = 0; i < MAX_TEXT_ID; ++i)
                {
                    if (itrM->second.textId[i] &&
                        !sObjectMgr::Instance()->GetMangosStringLocale(
                            itrM->second.textId[i]))
                        logging.error(
                            "Table `db_script_string` is missing string id %u, "
                            "used in database script table %s id %u.",
                            itrM->second.textId[i], scripts.first,
                            itrMM->first);

                    if (ids.find(itrM->second.textId[i]) != ids.end())
                        ids.erase(itrM->second.textId[i]);
                }
            }
        }
    }
}

// /////////////////////////////////////////////////////////
//              DB SCRIPT ENGINE
// /////////////////////////////////////////////////////////

/// Helper function to get Object source or target for Script-Command
/// returns false iff an error happened
bool ScriptAction::GetScriptCommandObject(
    const ObjectGuid guid, bool includeItem, Object*& resultObject)
{
    resultObject = nullptr;

    if (!guid)
        return true;

    switch (guid.GetHigh())
    {
    case HIGHGUID_UNIT:
        resultObject = m_map->GetCreature(guid);
        break;
    case HIGHGUID_PET:
        resultObject = m_map->GetPet(guid);
        break;
    case HIGHGUID_PLAYER:
        resultObject = ObjectAccessor::FindPlayer(guid, false);
        break;
    case HIGHGUID_GAMEOBJECT:
        resultObject = m_map->GetGameObject(guid);
        break;
    case HIGHGUID_CORPSE:
        resultObject = HashMapHolder<Corpse>::Find(guid);
        break;
    case HIGHGUID_ITEM:
        // case HIGHGUID_CONTAINER: ==HIGHGUID_ITEM
        {
            if (includeItem)
            {
                if (Player* player =
                        ObjectAccessor::FindPlayer(m_ownerGuid, false))
                    resultObject = player->GetItemByGuid(guid);
                break;
            }
            // else no break, but display error message
        }
    default:
        logging.error(
            " DB-SCRIPTS: Process table `%s` id %u, command %u with "
            "unsupported guid %s, skipping",
            m_table, m_script->id, m_script->command, guid.GetString().c_str());
        return false;
    }

    if (resultObject && !resultObject->IsInWorld())
        resultObject = nullptr;

    return true;
}

/// Select source and target for a script command
/// Returns false iff an error happened
bool ScriptAction::GetScriptProcessTargets(WorldObject* pOrigSource,
    WorldObject* pOrigTarget, WorldObject*& pFinalSource,
    WorldObject*& pFinalTarget)
{
    WorldObject* pBuddy = nullptr;

    if (m_script->buddyEntry)
    {
        if (!pOrigSource && !pOrigTarget)
        {
            logging.error(
                " DB-SCRIPTS: Process table `%s` id %u, command %u called "
                "without buddy %u, but no source for search available, "
                "skipping.",
                m_table, m_script->id, m_script->command, m_script->buddyEntry);
            return false;
        }

        // Prefer non-players as searcher
        WorldObject* pSearcher = pOrigSource ? pOrigSource : pOrigTarget;
        if (pSearcher->GetTypeId() == TYPEID_PLAYER && pOrigTarget &&
            pOrigTarget->GetTypeId() != TYPEID_PLAYER)
            pSearcher = pOrigTarget;

        if (m_script->IsCreatureBuddy())
        {
            pBuddy = maps::visitors::yield_best_match<Creature>{}(pSearcher,
                m_script->searchRadius,
                maps::checks::entry_guid{
                    m_script->buddyEntry, 0, pSearcher, true});
        }
        else
        {
            pBuddy = maps::visitors::yield_best_match<GameObject>{}(pSearcher,
                m_script->searchRadius,
                maps::checks::entry_guid{m_script->buddyEntry, 0});
        }

        if (!pBuddy)
        {
            logging.error(
                " DB-SCRIPTS: Process table `%s` id %u, command %u has buddy "
                "%u not found in range %u of searcher %s (data-flags %u), "
                "skipping.",
                m_table, m_script->id, m_script->command, m_script->buddyEntry,
                m_script->searchRadius, pSearcher->GetGuidStr().c_str(),
                m_script->data_flags);
            return false;
        }
    }

    if (m_script->data_flags & SCRIPT_FLAG_BUDDY_AS_TARGET)
    {
        pFinalSource = pOrigSource;
        pFinalTarget = pBuddy;
    }
    else
    {
        pFinalSource = pBuddy ? pBuddy : pOrigSource;
        pFinalTarget = pOrigTarget;
    }

    if (m_script->data_flags & SCRIPT_FLAG_REVERSE_DIRECTION)
        std::swap(pFinalSource, pFinalTarget);

    if (m_script->data_flags & SCRIPT_FLAG_SOURCE_TARGETS_SELF)
        pFinalTarget = pFinalSource;

    return true;
}

/// Helper to log error information
bool ScriptAction::LogIfNotCreature(WorldObject* pWorldObject)
{
    if (!pWorldObject || pWorldObject->GetTypeId() != TYPEID_UNIT)
    {
        logging.error(
            " DB-SCRIPTS: Process table `%s` id %u, command %u call for "
            "non-creature, skipping.",
            m_table, m_script->id, m_script->command);
        return true;
    }
    return false;
}
bool ScriptAction::LogIfNotUnit(WorldObject* pWorldObject)
{
    if (!pWorldObject || !pWorldObject->isType(TYPEMASK_UNIT))
    {
        logging.error(
            " DB-SCRIPTS: Process table `%s` id %u, command %u call for "
            "non-unit, skipping.",
            m_table, m_script->id, m_script->command);
        return true;
    }
    return false;
}
bool ScriptAction::LogIfNotGameObject(WorldObject* pWorldObject)
{
    if (!pWorldObject || pWorldObject->GetTypeId() != TYPEID_GAMEOBJECT)
    {
        logging.error(
            " DB-SCRIPTS: Process table `%s` id %u, command %u call for "
            "non-gameobject, skipping.",
            m_table, m_script->id, m_script->command);
        return true;
    }
    return false;
}

/// Helper to get a player if possible (target preferred)
Player* ScriptAction::GetPlayerTargetOrSourceAndLog(
    WorldObject* pSource, WorldObject* pTarget)
{
    if ((!pTarget || pTarget->GetTypeId() != TYPEID_PLAYER) &&
        (!pSource || pSource->GetTypeId() != TYPEID_PLAYER))
    {
        logging.error(
            " DB-SCRIPTS: Process table `%s` id %u, command %u call for non "
            "player, skipping.",
            m_table, m_script->id, m_script->command);
        return nullptr;
    }

    return pTarget && pTarget->GetTypeId() == TYPEID_PLAYER ? (Player*)pTarget :
                                                              (Player*)pSource;
}

/// Handle one Script Step
void ScriptAction::HandleScriptStep()
{
    Object* source = nullptr;
    Object* target = nullptr;
    if (!GetScriptCommandObject(m_sourceGuid, true, source))
        return;
    if (!GetScriptCommandObject(m_targetGuid, false, target))
        return;

    // Give some debug log output for easier use
    LOG_DEBUG(logging,
        "DB-SCRIPTS: Process table `%s` id %u, command %u for source %s (%sin "
        "world), target %s (%sin world)",
        m_table, m_script->id, m_script->command,
        m_sourceGuid.GetString().c_str(), source ? "" : "not ",
        m_targetGuid.GetString().c_str(), target ? "" : "not ");

    // Get expected source and target (if defined with buddy)
    WorldObject* pSource = source && source->isType(TYPEMASK_WORLDOBJECT) ?
                               (WorldObject*)source :
                               nullptr;
    WorldObject* pTarget = target && target->isType(TYPEMASK_WORLDOBJECT) ?
                               (WorldObject*)target :
                               nullptr;
    if (!GetScriptProcessTargets(pSource, pTarget, pSource, pTarget))
        return;

    switch (m_script->command)
    {
    case SCRIPT_COMMAND_TALK:
    {
        if (!pSource)
        {
            logging.error(
                " DB-SCRIPTS: Process table `%s` id %u, command %u found no "
                "worldobject as source, skipping.",
                m_table, m_script->id, m_script->command);
            break;
        }

        Unit* unitTarget = pTarget && pTarget->isType(TYPEMASK_UNIT) ?
                               static_cast<Unit*>(pTarget) :
                               nullptr;
        int32 textId = m_script->textId[0];

        // May have text for random
        if (m_script->textId[1])
        {
            int i = 2;
            for (; i < MAX_TEXT_ID; ++i)
            {
                if (!m_script->textId[i])
                    break;
            }

            // Use one random
            textId = m_script->textId[urand(0, i - 1)];
        }

        switch (m_script->talk.chatType)
        {
        case CHAT_TYPE_SAY:
            pSource->MonsterSay(textId, m_script->talk.language, unitTarget);
            break;
        case CHAT_TYPE_YELL:
            pSource->MonsterYell(textId, m_script->talk.language, unitTarget);
            break;
        case CHAT_TYPE_TEXT_EMOTE:
            pSource->MonsterTextEmote(textId, unitTarget);
            break;
        case CHAT_TYPE_BOSS_EMOTE:
            pSource->MonsterTextEmote(textId, unitTarget, true);
            break;
        case CHAT_TYPE_WHISPER:
            if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
            {
                logging.error(
                    " DB-SCRIPTS: Process table `%s` id %u, command %u attempt "
                    "to whisper (%u) to %s, skipping.",
                    m_table, m_script->id, m_script->command,
                    m_script->talk.chatType,
                    unitTarget ? unitTarget->GetGuidStr().c_str() :
                                 "<no target>");
                break;
            }
            pSource->MonsterWhisper(textId, unitTarget);
            break;
        case CHAT_TYPE_BOSS_WHISPER:
            if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
            {
                logging.error(
                    " DB-SCRIPTS: Process table `%s` id %u, command %u attempt "
                    "to whisper (%u) to %s, skipping.",
                    m_table, m_script->id, m_script->command,
                    m_script->talk.chatType,
                    unitTarget ? unitTarget->GetGuidStr().c_str() :
                                 "<no target>");
                break;
            }
            pSource->MonsterWhisper(textId, unitTarget, true);
            break;
        case CHAT_TYPE_ZONE_YELL:
            pSource->MonsterYellToZone(
                textId, m_script->talk.language, unitTarget);
            break;
        default:
            break; // must be already checked at load
        }
        break;
    }
    case SCRIPT_COMMAND_EMOTE:
    {
        if (LogIfNotUnit(pSource))
            break;

        ((Unit*)pSource)->HandleEmote(m_script->emote.emoteId);
        break;
    }
    case SCRIPT_COMMAND_FIELD_SET:
        // TODO
        if (!source)
        {
            logging.error(
                " DB-SCRIPTS: Process table `%s` id %u, command %u call for "
                "NULL object.",
                m_table, m_script->id, m_script->command);
            break;
        }

        if (m_script->setField.fieldId <= OBJECT_FIELD_ENTRY ||
            m_script->setField.fieldId >= source->GetValuesCount())
        {
            logging.error(
                " DB-SCRIPTS: Process table `%s` id %u, command %u call for "
                "wrong field %u (max count: %u) in object (TypeId: %u).",
                m_table, m_script->id, m_script->command,
                m_script->setField.fieldId, source->GetValuesCount(),
                source->GetTypeId());
            break;
        }

        source->SetUInt32Value(
            m_script->setField.fieldId, m_script->setField.fieldValue);
        break;
    case SCRIPT_COMMAND_MOVE_TO:
    {
        if (LogIfNotUnit(pSource))
            break;

        // Just turn around
        if ((m_script->x == 0.0f && m_script->y == 0.0f &&
                m_script->z == 0.0f) ||
            // Check point-to-point distance, hence revert effect of bounding
            // radius
            ((Unit*)pSource)
                ->IsWithinDist3d(m_script->x, m_script->y, m_script->z,
                    0.01f - ((Unit*)pSource)->GetObjectBoundingRadius()))
        {
            ((Unit*)pSource)->SetFacingTo(m_script->o);
            break;
        }

        // For command additional teleport the unit
        if (m_script->data_flags & SCRIPT_FLAG_COMMAND_ADDITIONAL)
        {
            ((Unit*)pSource)
                ->NearTeleportTo(m_script->x, m_script->y, m_script->z,
                    m_script->o != 0.0f ? m_script->o :
                                          ((Unit*)pSource)->GetO());
            break;
        }

        // Normal Movement
        // TODO: m_script->moveTo.travelSpeed ignored; should not be needed for
        // anything anyways
        ((Unit*)pSource)
            ->movement_gens.push(new movement::PointMovementGenerator(0,
                m_script->x, m_script->y, m_script->z, false,
                ((Unit*)pSource)->isInCombat()));
        break;
    }
    case SCRIPT_COMMAND_FLAG_SET:
        // TODO
        if (!source)
        {
            logging.error(
                "SCRIPT_COMMAND_FLAG_SET (script id %u) call for NULL object.",
                m_script->id);
            break;
        }
        if (m_script->setFlag.fieldId <= OBJECT_FIELD_ENTRY ||
            m_script->setFlag.fieldId >= source->GetValuesCount())
        {
            logging.error(
                "SCRIPT_COMMAND_FLAG_SET (script id %u) call for wrong field "
                "%u (max count: %u) in object (TypeId: %u).",
                m_script->id, m_script->setFlag.fieldId,
                source->GetValuesCount(), source->GetTypeId());
            break;
        }

        source->SetFlag(
            m_script->setFlag.fieldId, m_script->setFlag.fieldValue);
        break;
    case SCRIPT_COMMAND_FLAG_REMOVE:
        // TODO
        if (!source)
        {
            logging.error(
                "SCRIPT_COMMAND_FLAG_REMOVE (script id %u) call for NULL "
                "object.",
                m_script->id);
            break;
        }
        if (m_script->removeFlag.fieldId <= OBJECT_FIELD_ENTRY ||
            m_script->removeFlag.fieldId >= source->GetValuesCount())
        {
            logging.error(
                "SCRIPT_COMMAND_FLAG_REMOVE (script id %u) call for wrong "
                "field %u (max count: %u) in object (TypeId: %u).",
                m_script->id, m_script->removeFlag.fieldId,
                source->GetValuesCount(), source->GetTypeId());
            break;
        }

        source->RemoveFlag(
            m_script->removeFlag.fieldId, m_script->removeFlag.fieldValue);
        break;
    case SCRIPT_COMMAND_TELEPORT_TO:
    {
        Player* pPlayer = GetPlayerTargetOrSourceAndLog(pSource, pTarget);
        if (!pPlayer)
            break;

        pPlayer->TeleportTo(m_script->teleportTo.mapId, m_script->x,
            m_script->y, m_script->z, m_script->o);
        break;
    }
    case SCRIPT_COMMAND_QUEST_EXPLORED:
    {
        Player* pPlayer = GetPlayerTargetOrSourceAndLog(pSource, pTarget);
        if (!pPlayer)
            break;

        WorldObject* pWorldObject = nullptr;
        if (pSource && pSource->isType(TYPEMASK_CREATURE_OR_GAMEOBJECT))
            pWorldObject = pSource;
        else if (pTarget && pTarget->isType(TYPEMASK_CREATURE_OR_GAMEOBJECT))
            pWorldObject = pTarget;

        // if we have a distance, we must have a worldobject
        if (m_script->questExplored.distance != 0 && !pWorldObject)
        {
            logging.error(
                " DB-SCRIPTS: Process table `%s` id %u, command %u called "
                "without source worldobject, skipping.",
                m_table, m_script->id, m_script->command);
            break;
        }

        bool failQuest = false;
        // Creature must be alive for giving credit
        if (pWorldObject && pWorldObject->GetTypeId() == TYPEID_UNIT &&
            !((Creature*)pWorldObject)->isAlive())
            failQuest = true;
        else if (m_script->questExplored.distance != 0 &&
                 !pWorldObject->IsWithinDistInMap(
                     pPlayer, float(m_script->questExplored.distance)))
            failQuest = true;

        // quest id and flags checked at script loading
        if (!failQuest)
            pPlayer->AreaExploredOrEventHappens(
                m_script->questExplored.questId);
        else
            pPlayer->FailQuest(m_script->questExplored.questId);

        break;
    }
    case SCRIPT_COMMAND_KILL_CREDIT:
    {
        Player* pPlayer = GetPlayerTargetOrSourceAndLog(pSource, pTarget);
        if (!pPlayer)
            break;

        if (m_script->killCredit.isGroupCredit)
            pPlayer->RewardPlayerAndGroupAtEvent(
                m_script->killCredit.creatureEntry, pSource);
        else
            pPlayer->KilledMonsterCredit(m_script->killCredit.creatureEntry);

        break;
    }
    case SCRIPT_COMMAND_RESPAWN_GAMEOBJECT:
    {
        GameObject* pGo = nullptr;
        uint32 time_to_despawn = m_script->respawnGo.despawnDelay < 5 ?
                                     5 :
                                     m_script->respawnGo.despawnDelay;

        if (m_script->respawnGo.goGuid)
        {
            GameObjectData const* goData =
                sObjectMgr::Instance()->GetGOData(m_script->respawnGo.goGuid);
            if (!goData)
                break; // checked at load

            // TODO - This was a change, was before current map of source
            pGo = m_map->GetGameObject(ObjectGuid(
                HIGHGUID_GAMEOBJECT, goData->id, m_script->respawnGo.goGuid));
        }
        else
        {
            if (LogIfNotGameObject(pSource))
                break;

            pGo = (GameObject*)pSource;
        }

        if (!pGo)
        {
            logging.error(
                " DB-SCRIPTS: Process table `%s` id %u, command %u failed for "
                "gameobject(guid: %u, buddyEntry: %u).",
                m_table, m_script->id, m_script->command,
                m_script->respawnGo.goGuid, m_script->buddyEntry);
            break;
        }

        if (pGo->GetGoType() == GAMEOBJECT_TYPE_FISHINGNODE ||
            pGo->GetGoType() == GAMEOBJECT_TYPE_DOOR ||
            pGo->GetGoType() == GAMEOBJECT_TYPE_BUTTON ||
            pGo->GetGoType() == GAMEOBJECT_TYPE_TRAP)
        {
            logging.error(
                " DB-SCRIPTS: Process table `%s` id %u, command %u can not be "
                "used with gameobject of type %u (guid: %u, buddyEntry: %u).",
                m_table, m_script->id, m_script->command,
                uint32(pGo->GetGoType()), m_script->respawnGo.goGuid,
                m_script->buddyEntry);
            break;
        }

        if (pGo->isSpawned())
            break; // gameobject already spawned

        pGo->SetLootState(GO_READY);
        pGo->SetRespawnTime(time_to_despawn); // despawn object in ? seconds
        pGo->Refresh();
        break;
    }
    case SCRIPT_COMMAND_TEMP_SUMMON_CREATURE:
    {
        if (!pSource)
        {
            logging.error(
                " DB-SCRIPTS: Process table `%s` id %u, command %u found no "
                "worldobject as source, skipping.",
                m_table, m_script->id, m_script->command);
            break;
        }

        float x = m_script->x;
        float y = m_script->y;
        float z = m_script->z;
        float o = m_script->o;

        Creature* pCreature =
            pSource->SummonCreature(m_script->summonCreature.creatureEntry, x,
                y, z, o, TEMPSUMMON_TIMED_OR_DEAD_DESPAWN,
                m_script->summonCreature.despawnDelay,
                (m_script->data_flags & SCRIPT_FLAG_COMMAND_ADDITIONAL) ?
                    SUMMON_OPT_ACTIVE :
                    0);
        if (!pCreature)
        {
            logging.error(
                " DB-SCRIPTS: Process table `%s` id %u, command %u failed for "
                "creature (entry: %u).",
                m_table, m_script->id, m_script->command,
                m_script->summonCreature.creatureEntry);
            break;
        }

        break;
    }
    case SCRIPT_COMMAND_OPEN_DOOR:
    case SCRIPT_COMMAND_CLOSE_DOOR:
    {
        GameObject* pDoor;
        uint32 time_to_reset = m_script->changeDoor.resetDelay < 15 ?
                                   15 :
                                   m_script->changeDoor.resetDelay;

        if (m_script->changeDoor.goGuid)
        {
            GameObjectData const* goData =
                sObjectMgr::Instance()->GetGOData(m_script->changeDoor.goGuid);
            if (!goData) // checked at load
                break;

            // TODO - Was a change, before random map
            pDoor = m_map->GetGameObject(ObjectGuid(
                HIGHGUID_GAMEOBJECT, goData->id, m_script->changeDoor.goGuid));
        }
        else
        {
            if (LogIfNotGameObject(pSource))
                break;

            pDoor = (GameObject*)pSource;
        }

        if (!pDoor)
        {
            logging.error(
                " DB-SCRIPTS: Process table `%s` id %u, command %u failed for "
                "gameobject(guid: %u, buddyEntry: %u).",
                m_table, m_script->id, m_script->command,
                m_script->changeDoor.goGuid, m_script->buddyEntry);
            break;
        }

        if (pDoor->GetGoType() != GAMEOBJECT_TYPE_DOOR)
        {
            logging.error(
                " DB-SCRIPTS: Process table `%s` id %u, command %u failed for "
                "non-door(GoType: %u).",
                m_table, m_script->id, m_script->command, pDoor->GetGoType());
            break;
        }

        if ((m_script->command == SCRIPT_COMMAND_OPEN_DOOR &&
                pDoor->GetGoState() != GO_STATE_READY) ||
            (m_script->command == SCRIPT_COMMAND_CLOSE_DOOR &&
                pDoor->GetGoState() == GO_STATE_READY))
            break; // to be opened door already open, or to be closed door
                   // already closed

        pDoor->UseDoorOrButton(time_to_reset);

        if (pTarget && pTarget->isType(TYPEMASK_GAMEOBJECT) &&
            ((GameObject*)pTarget)->GetGoType() == GAMEOBJECT_TYPE_BUTTON)
            ((GameObject*)target)->UseDoorOrButton(time_to_reset);

        break;
    }
    case SCRIPT_COMMAND_ACTIVATE_OBJECT:
    {
        if (LogIfNotUnit(pSource))
            break;
        if (LogIfNotGameObject(pTarget))
            break;

        ((GameObject*)pTarget)
            ->Use((Unit*)pSource, true, m_script->activateObject.misc_value);
        break;
    }
    case SCRIPT_COMMAND_REMOVE_AURA:
    {
        if (LogIfNotUnit(pSource))
            break;

        ((Unit*)pSource)->remove_auras(m_script->removeAura.spellId);
        break;
    }
    case SCRIPT_COMMAND_CAST_SPELL:
    {
        if (LogIfNotUnit(pSource))
            break;
        if (LogIfNotUnit(pTarget))
            break;

        // TODO: when GO cast implemented, code below must be updated
        // accordingly to also allow GO spell cast
        ((Unit*)pSource)
            ->CastSpell(((Unit*)pTarget), m_script->castSpell.spellId,
                (m_script->data_flags & SCRIPT_FLAG_COMMAND_ADDITIONAL) != 0);

        break;
    }
    case SCRIPT_COMMAND_PLAY_SOUND: // TODO
    {
        if (!pSource)
        {
            logging.error(
                " DB-SCRIPTS: Process table `%s` id %u, command %u could not "
                "find proper source",
                m_table, m_script->id, m_script->command);
            break;
        }

        // bitmask: 0/1=anyone/target, 0/2=with distance dependent
        Player* pTarget = nullptr;

        if (m_script->playSound.flags & 1)
        {
            if (!target)
            {
                logging.error(
                    " DB-SCRIPTS: Process table `%s` id %u, command %u in "
                    "targeted mode call for NULL target.",
                    m_table, m_script->id, m_script->command);
                break;
            }

            if (target->GetTypeId() != TYPEID_PLAYER)
            {
                logging.error(
                    " DB-SCRIPTS: Process table `%s` id %u, command %u in "
                    "targeted mode call for non-player (TypeId: %u), skipping.",
                    m_table, m_script->id, m_script->command,
                    target->GetTypeId());
                break;
            }

            pTarget = (Player*)target;
        }

        // bitmask: 0/1=anyone/target, 0/2=with distance dependent
        if (m_script->playSound.flags & 2)
            pSource->PlayDistanceSound(m_script->playSound.soundId, pTarget);
        else
            pSource->PlayDirectSound(m_script->playSound.soundId, pTarget);

        break;
    }
    case SCRIPT_COMMAND_CREATE_ITEM:
    {
        // XXX:
        Player* pPlayer = GetPlayerTargetOrSourceAndLog(pSource, pTarget);
        if (!pPlayer)
            break;

        inventory::transaction trans;
        trans.add(m_script->createItem.itemEntry, m_script->createItem.amount);
        pPlayer->storage().finalize(trans);
        // XXX: pPlayer->SendNewItem(pItem, m_script->createItem.amount, true,
        // false);

        break;
    }
    case SCRIPT_COMMAND_DESPAWN_SELF:
    {
        // TODO - Remove this check after a while
        if (pTarget && pTarget->GetTypeId() != TYPEID_UNIT && pSource &&
            pSource->GetTypeId() == TYPEID_UNIT)
        {
            logging.error(
                "DB-SCRIPTS: Process table `%s` id %u, command %u target must "
                "be creature, but (only) source is, use data_flags to fix",
                m_table, m_script->id, m_script->command);
            pTarget = pSource;
        }

        if (LogIfNotCreature(pTarget))
            break;

        ((Creature*)pTarget)->ForcedDespawn(m_script->despawn.despawnDelay);

        break;
    }
    case SCRIPT_COMMAND_PLAY_MOVIE:
    {
        break; // must be skipped at loading
    }
    case SCRIPT_COMMAND_MOVEMENT:
    {
        if (LogIfNotCreature(pSource))
            break;

        // Consider add additional checks for cases where creature should not
        // change movementType
        // (pet? in combat? already using same MMgen as script try to apply?)

        switch (m_script->movement.movementType)
        {
        case (int)movement::gen::idle:
            ((Creature*)pSource)
                ->movement_gens.push(new movement::IdleMovementGenerator());
            break;
        case (int)movement::gen::random:
            ((Creature*)pSource)
                ->movement_gens.push(new movement::RandomMovementGenerator());
            break;
        case (int)movement::gen::waypoint:
            ((Creature*)pSource)
                ->movement_gens.push(new movement::WaypointMovementGenerator());
            break;
        }

        break;
    }
    case SCRIPT_COMMAND_SET_ACTIVEOBJECT:
    {
        if (LogIfNotCreature(pSource))
            break;

        ((Creature*)pSource)
            ->SetActiveObjectState(m_script->activeObject.activate);
        break;
    }
    case SCRIPT_COMMAND_SET_FACTION:
    {
        if (LogIfNotCreature(pSource))
            break;

        if (m_script->faction.factionId)
            ((Creature*)pSource)
                ->SetFactionTemporary(
                    m_script->faction.factionId, m_script->faction.flags);
        else
            ((Creature*)pSource)->ClearTemporaryFaction();

        break;
    }
    case SCRIPT_COMMAND_MORPH_TO_ENTRY_OR_MODEL:
    {
        if (LogIfNotCreature(pSource))
            break;

        if (!m_script->morph.creatureOrModelEntry)
            ((Creature*)pSource)->DeMorph();
        else if (m_script->data_flags & SCRIPT_FLAG_COMMAND_ADDITIONAL)
            ((Creature*)pSource)
                ->SetDisplayId(m_script->morph.creatureOrModelEntry);
        else
        {
            CreatureInfo const* ci = ObjectMgr::GetCreatureTemplate(
                m_script->morph.creatureOrModelEntry);
            uint32 display_id = Creature::ChooseDisplayId(ci);

            ((Creature*)pSource)->SetDisplayId(display_id);
        }

        break;
    }
    case SCRIPT_COMMAND_MOUNT_TO_ENTRY_OR_MODEL:
    {
        if (LogIfNotCreature(pSource))
            break;

        if (!m_script->mount.creatureOrModelEntry)
            ((Creature*)pSource)->Unmount();
        else if (m_script->data_flags & SCRIPT_FLAG_COMMAND_ADDITIONAL)
            ((Creature*)pSource)->Mount(m_script->mount.creatureOrModelEntry);
        else
        {
            CreatureInfo const* ci = ObjectMgr::GetCreatureTemplate(
                m_script->mount.creatureOrModelEntry);
            uint32 display_id = Creature::ChooseDisplayId(ci);

            ((Creature*)pSource)->Mount(display_id);
        }

        break;
    }
    case SCRIPT_COMMAND_SET_RUN:
    {
        if (LogIfNotCreature(pSource))
            break;

        ((Creature*)pSource)->SetWalk(!m_script->run.run);

        break;
    }
    case SCRIPT_COMMAND_ATTACK_START:
    {
        if (LogIfNotCreature(pSource))
            break;
        if (LogIfNotUnit(pTarget))
            break;

        Creature* pAttacker = static_cast<Creature*>(pSource);
        Unit* unitTarget = static_cast<Unit*>(target);

        if (pAttacker->IsFriendlyTo(unitTarget))
        {
            logging.error(
                " DB-SCRIPTS: Process table `%s` id %u, command %u attacker is "
                "friendly to target, can not attack (Attacker: %s, Target: %s)",
                m_table, m_script->id, m_script->command,
                pAttacker->GetGuidStr().c_str(),
                unitTarget->GetGuidStr().c_str());
            break;
        }

        pAttacker->AI()->AttackStart(unitTarget);

        break;
    }
    case SCRIPT_COMMAND_GO_LOCK_STATE:
    {
        if (LogIfNotGameObject(pSource))
            break;

        GameObject* pGo = static_cast<GameObject*>(pSource);

        /* flag lockState
         * go_lock          0x01
         * go_unlock        0x02
         * go_nonInteract   0x04
         * go_Interact      0x08
         */

        // Lock or Unlock
        if (m_script->goLockState.lockState & 0x01)
            pGo->SetFlag(GAMEOBJECT_FLAGS, GO_FLAG_LOCKED);
        else if (m_script->goLockState.lockState & 0x02)
            pGo->RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_LOCKED);
        // Set Non Interactable or Set Interactable
        if (m_script->goLockState.lockState & 0x04)
            pGo->SetFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT);
        else if (m_script->goLockState.lockState & 0x08)
            pGo->RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT);
    }
    case SCRIPT_COMMAND_STAND_STATE:
    {
        if (LogIfNotCreature(pSource))
            break;

        // Must be safe cast to Unit* here
        ((Unit*)pSource)->SetStandState(m_script->standState.stand_state);
        break;
    }
    case SCRIPT_COMMAND_MODIFY_NPC_FLAGS:
    {
        if (LogIfNotCreature(pSource))
            break;

        // Add Flags
        if (m_script->npcFlag.change_flag & 0x01)
            pSource->SetFlag(UNIT_NPC_FLAGS, m_script->npcFlag.flag);
        // Remove Flags
        else if (m_script->npcFlag.change_flag & 0x02)
            pSource->RemoveFlag(UNIT_NPC_FLAGS, m_script->npcFlag.flag);
        // Toggle Flags
        else
        {
            if (pSource->HasFlag(UNIT_NPC_FLAGS, m_script->npcFlag.flag))
                pSource->RemoveFlag(UNIT_NPC_FLAGS, m_script->npcFlag.flag);
            else
                pSource->SetFlag(UNIT_NPC_FLAGS, m_script->npcFlag.flag);
        }

        break;
    }
    default:
        logging.error(
            " DB-SCRIPTS: Process table `%s` id %u, command %u unknown command "
            "used.",
            m_table, m_script->id, m_script->command);
        break;
    }
}

// /////////////////////////////////////////////////////////
//              Scripting Library Hooks
// /////////////////////////////////////////////////////////

void ScriptMgr::LoadAreaTriggerScripts()
{
    m_AreaTriggerScripts.clear(); // need for reload case
    QueryResult* result = WorldDatabase.Query(
        "SELECT entry, ScriptName FROM scripted_areatrigger");

    uint32 count = 0;

    if (!result)
    {
        logging.info("Loaded %u scripted areatrigger\n", count);
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        ++count;
        bar.step();

        Field* fields = result->Fetch();

        uint32 triggerId = fields[0].GetUInt32();
        const char* scriptName = fields[1].GetString();

        if (!sAreaTriggerStore.LookupEntry(triggerId))
        {
            logging.error(
                "Table `scripted_areatrigger` has area trigger (ID: %u) not "
                "listed in `AreaTrigger.dbc`.",
                triggerId);
            continue;
        }

        m_AreaTriggerScripts[triggerId] = GetScriptId(scriptName);
    } while (result->NextRow());

    delete result;

    logging.info("Loaded %u areatrigger scripts\n", count);
}

void ScriptMgr::LoadEventIdScripts()
{
    m_EventIdScripts.clear(); // need for reload case
    QueryResult* result =
        WorldDatabase.Query("SELECT id, ScriptName FROM scripted_event_id");

    uint32 count = 0;

    if (!result)
    {
        logging.info("Loaded %u scripted event id\n", count);
        return;
    }

    BarGoLink bar(result->GetRowCount());

    // TODO: remove duplicate code below, same way to collect event id's used in
    // LoadEventScripts()
    std::set<uint32> evt_scripts;

    // Load all possible event entries from gameobjects
    for (uint32 i = 1; i < sGOStorage.MaxEntry; ++i)
    {
        if (GameObjectInfo const* goInfo =
                sGOStorage.LookupEntry<GameObjectInfo>(i))
        {
            if (uint32 eventId = goInfo->GetEventScriptId())
                evt_scripts.insert(eventId);

            if (goInfo->type == GAMEOBJECT_TYPE_CAPTURE_POINT)
            {
                evt_scripts.insert(goInfo->capturePoint.neutralEventID1);
                evt_scripts.insert(goInfo->capturePoint.neutralEventID2);
                evt_scripts.insert(goInfo->capturePoint.contestedEventID1);
                evt_scripts.insert(goInfo->capturePoint.contestedEventID2);
                evt_scripts.insert(goInfo->capturePoint.progressEventID1);
                evt_scripts.insert(goInfo->capturePoint.progressEventID2);
                evt_scripts.insert(goInfo->capturePoint.winEventID1);
                evt_scripts.insert(goInfo->capturePoint.winEventID2);
            }
        }
    }

    // Load all possible event entries from spells
    for (uint32 i = 1; i < sSpellStore.GetNumRows(); ++i)
    {
        SpellEntry const* spell = sSpellStore.LookupEntry(i);
        if (spell)
        {
            for (int j = 0; j < MAX_EFFECT_INDEX; ++j)
            {
                if (spell->Effect[j] == SPELL_EFFECT_SEND_EVENT)
                {
                    if (spell->EffectMiscValue[j])
                        evt_scripts.insert(spell->EffectMiscValue[j]);
                }
            }
        }
    }

    // Load all possible event entries from taxi path nodes
    for (auto& elem : sTaxiPathNodesByPath)
    {
        for (size_t node_idx = 0; node_idx < elem.size(); ++node_idx)
        {
            TaxiPathNodeEntry const& node = elem[node_idx];

            if (node.arrivalEventID)
                evt_scripts.insert(node.arrivalEventID);

            if (node.departureEventID)
                evt_scripts.insert(node.departureEventID);
        }
    }

    do
    {
        ++count;
        bar.step();

        Field* fields = result->Fetch();

        uint32 eventId = fields[0].GetUInt32();
        const char* scriptName = fields[1].GetString();

        auto itr = evt_scripts.find(eventId);
        if (itr == evt_scripts.end())
            logging.error(
                "Table `scripted_event_id` has id %u not referring to any "
                "gameobject_template type 10 data2 field, type 3 data6 field, "
                "type 13 data 2 field, type 29 or any spell effect %u or path "
                "taxi node data",
                eventId, SPELL_EFFECT_SEND_EVENT);

        m_EventIdScripts[eventId] = GetScriptId(scriptName);
    } while (result->NextRow());

    delete result;

    logging.info("Loaded %u scripted event id\n", count);
}

void ScriptMgr::LoadScriptNames()
{
    m_scriptNames.push_back("");
    QueryResult* result = WorldDatabase.Query(
        "SELECT DISTINCT(ScriptName) FROM creature_template WHERE ScriptName "
        "<> '' "
        "UNION "
        "SELECT DISTINCT(ScriptName) FROM gameobject_template WHERE ScriptName "
        "<> '' "
        "UNION "
        "SELECT DISTINCT(ScriptName) FROM item_template WHERE ScriptName <> '' "
        "UNION "
        "SELECT DISTINCT(ScriptName) FROM scripted_areatrigger WHERE "
        "ScriptName <> '' "
        "UNION "
        "SELECT DISTINCT(ScriptName) FROM scripted_event_id WHERE ScriptName "
        "<> '' "
        "UNION "
        "SELECT DISTINCT(ScriptName) FROM instance_template WHERE ScriptName "
        "<> '' "
        "UNION "
        "SELECT DISTINCT(ScriptName) FROM world_template WHERE ScriptName <> "
        "''");

    if (!result)
    {
        logging.error("Loaded empty set of Script Names!\n");
        return;
    }

    BarGoLink bar(result->GetRowCount());
    uint32 count = 0;

    do
    {
        bar.step();
        m_scriptNames.push_back((*result)[0].GetString());
        ++count;
    } while (result->NextRow());
    delete result;

    std::sort(m_scriptNames.begin(), m_scriptNames.end());
    logging.info("Loaded %d Script Names\n", count);
}

uint32 ScriptMgr::GetScriptId(const char* name) const
{
    // use binary search to find the script name in the sorted vector
    // assume "" is the first element
    if (!name)
        return 0;

    auto itr =
        std::lower_bound(m_scriptNames.begin(), m_scriptNames.end(), name);

    if (itr == m_scriptNames.end() || *itr != name)
        return 0;

    return uint32(itr - m_scriptNames.begin());
}

uint32 ScriptMgr::GetAreaTriggerScriptId(uint32 triggerId) const
{
    auto itr = m_AreaTriggerScripts.find(triggerId);
    if (itr != m_AreaTriggerScripts.end())
        return itr->second;

    return 0;
}

uint32 ScriptMgr::GetEventIdScriptId(uint32 eventId) const
{
    auto itr = m_EventIdScripts.find(eventId);
    if (itr != m_EventIdScripts.end())
        return itr->second;

    return 0;
}

CreatureAI* ScriptMgr::GetCreatureAI(Creature* pCreature)
{
    if (!m_pGetCreatureAI)
        return nullptr;

    return m_pGetCreatureAI(pCreature);
}

InstanceData* ScriptMgr::CreateInstanceData(Map* pMap)
{
    if (!m_pCreateInstanceData)
        return nullptr;

    return m_pCreateInstanceData(pMap);
}

bool ScriptMgr::OnGossipHello(Player* pPlayer, Creature* pCreature)
{
    if (pCreature->AI() && pCreature->AI()->OnGossipHello(pPlayer))
        return true;

    return m_pOnGossipHello != nullptr && m_pOnGossipHello(pPlayer, pCreature);
}

bool ScriptMgr::OnGossipHello(Player* pPlayer, GameObject* pGameObject)
{
    if (pGameObject->AI() && pGameObject->AI()->OnGossipHello(pPlayer))
        return true;

    return m_pOnGOGossipHello != nullptr &&
           m_pOnGOGossipHello(pPlayer, pGameObject);
}

bool ScriptMgr::OnGossipSelect(Player* pPlayer, Creature* pCreature,
    uint32 sender, uint32 action, const char* code)
{
    if (code)
        return m_pOnGossipSelectWithCode != nullptr &&
               m_pOnGossipSelectWithCode(
                   pPlayer, pCreature, sender, action, code);
    else
        return m_pOnGossipSelect != nullptr &&
               m_pOnGossipSelect(pPlayer, pCreature, sender, action);
}

bool ScriptMgr::OnGossipSelect(Player* pPlayer, GameObject* pGameObject,
    uint32 sender, uint32 action, const char* code)
{
    if (code)
        return m_pOnGOGossipSelectWithCode != nullptr &&
               m_pOnGOGossipSelectWithCode(
                   pPlayer, pGameObject, sender, action, code);
    else
        return m_pOnGOGossipSelect != nullptr &&
               m_pOnGOGossipSelect(pPlayer, pGameObject, sender, action);
}

bool ScriptMgr::OnQuestAccept(
    Player* pPlayer, Creature* pCreature, Quest const* pQuest)
{
    if (pCreature->AI())
        pCreature->AI()->OnQuestAccept(pPlayer, pQuest);

    return m_pOnQuestAccept != nullptr &&
           m_pOnQuestAccept(pPlayer, pCreature, pQuest);
}

bool ScriptMgr::OnQuestAccept(
    Player* pPlayer, GameObject* pGameObject, Quest const* pQuest)
{
    if (pGameObject->AI())
        pGameObject->AI()->OnQuestAccept(pPlayer, pQuest);

    return m_pOnGOQuestAccept != nullptr &&
           m_pOnGOQuestAccept(pPlayer, pGameObject, pQuest);
}

bool ScriptMgr::OnQuestAccept(Player* pPlayer, Item* pItem, Quest const* pQuest)
{
    return m_pOnItemQuestAccept != nullptr &&
           m_pOnItemQuestAccept(pPlayer, pItem, pQuest);
}

bool ScriptMgr::OnQuestRewarded(
    Player* pPlayer, Creature* pCreature, Quest const* pQuest)
{
    if (pCreature->AI())
        pCreature->AI()->OnQuestReward(pPlayer, pQuest);

    return m_pOnQuestRewarded != nullptr &&
           m_pOnQuestRewarded(pPlayer, pCreature, pQuest);
}

bool ScriptMgr::OnQuestRewarded(
    Player* pPlayer, GameObject* pGameObject, Quest const* pQuest)
{
    if (pGameObject->AI())
        pGameObject->AI()->OnQuestReward(pPlayer, pQuest);

    return m_pOnGOQuestRewarded != nullptr &&
           m_pOnGOQuestRewarded(pPlayer, pGameObject, pQuest);
}

uint32 ScriptMgr::GetDialogStatus(Player* pPlayer, Creature* pCreature)
{
    if (!m_pGetNPCDialogStatus)
        return 100;

    return m_pGetNPCDialogStatus(pPlayer, pCreature);
}

uint32 ScriptMgr::GetDialogStatus(Player* pPlayer, GameObject* pGameObject)
{
    if (pGameObject->AI())
    {
        uint32 status = pGameObject->AI()->GetDialogStatus(pPlayer);
        if (status != 100)
            return status;
    }

    if (!m_pGetGODialogStatus)
        return 100;

    return m_pGetGODialogStatus(pPlayer, pGameObject);
}

bool ScriptMgr::OnGameObjectUse(Player* pPlayer, GameObject* pGameObject)
{
    return m_pOnGOUse != nullptr && m_pOnGOUse(pPlayer, pGameObject);
}

bool ScriptMgr::OnItemUse(
    Player* pPlayer, Item* pItem, SpellCastTargets const& targets)
{
    return m_pOnItemUse != nullptr && m_pOnItemUse(pPlayer, pItem, targets);
}

bool ScriptMgr::OnAreaTrigger(Player* pPlayer, AreaTriggerEntry const* atEntry)
{
    return m_pOnAreaTrigger != nullptr && m_pOnAreaTrigger(pPlayer, atEntry);
}

bool ScriptMgr::OnProcessEvent(
    uint32 eventId, Object* pSource, Object* pTarget, bool isStart)
{
    if (pSource->GetTypeId() == TYPEID_UNIT)
    {
        Creature* c = (Creature*)pSource;
        if (c->AI())
            c->AI()->OnGameEvent(isStart, eventId);
    }
    else if (pSource->GetTypeId() == TYPEID_GAMEOBJECT)
    {
        GameObject* go = (GameObject*)pSource;
        if (go->AI())
            go->AI()->OnGameEvent(isStart, eventId);
    }

    return m_pOnProcessEvent != nullptr &&
           m_pOnProcessEvent(eventId, pSource, pTarget, isStart);
}

bool ScriptMgr::OnEffectDummy(
    Unit* pCaster, uint32 spellId, SpellEffectIndex effIndex, Creature* pTarget)
{
    // CreatureAI
    if (pTarget->AI() &&
        pTarget->AI()->OnDummyEffect(pCaster, spellId, effIndex))
        return true;

    return m_pOnEffectDummyCreature != nullptr &&
           m_pOnEffectDummyCreature(pCaster, spellId, effIndex, pTarget);
}

bool ScriptMgr::OnEffectDummy(Unit* pCaster, uint32 spellId,
    SpellEffectIndex effIndex, GameObject* pTarget)
{
    return m_pOnEffectDummyGO != nullptr &&
           m_pOnEffectDummyGO(pCaster, spellId, effIndex, pTarget);
}

bool ScriptMgr::OnEffectDummy(
    Unit* pCaster, uint32 spellId, SpellEffectIndex effIndex, Item* pTarget)
{
    return m_pOnEffectDummyItem != nullptr &&
           m_pOnEffectDummyItem(pCaster, spellId, effIndex, pTarget);
}

bool ScriptMgr::OnAuraDummy(Aura const* pAura, bool apply)
{
    return m_pOnAuraDummy != nullptr && m_pOnAuraDummy(pAura, apply);
}

bool ScriptMgr::OnConditionCheck(
    const Condition* condition, ConditionSourceInfo& sourceInfo)
{
    assert(condition);

    // This function is assumed to return true by default in condition mgr, yet
    // this is the opposite:
    // return m_pOnConditionCheck != NULL && m_pOnConditionCheck(condition,
    // sourceInfo);

    if (!m_pOnConditionCheck)
        return true;

    return m_pOnConditionCheck(condition, sourceInfo);
}

ScriptLoadResult ScriptMgr::LoadScriptLibrary(const char* libName)
{
    UnloadScriptLibrary();

    try
    {
        script_library_ = new MaNGOS::library(libName);
    }
    catch (std::exception& e)
    {
        logging.error("%s", e.what());
        return SCRIPT_LOAD_ERR_NOT_FOUND;
    }

    try
    {
        // Get function pointers to the other functions in the library

        typedef void(MANGOS_IMPORT * void_func)();

        m_pOnInitScriptLibrary =
            script_library_->symbol<void_func>("InitScriptLibrary");
        m_pOnFreeScriptLibrary =
            script_library_->symbol<void_func>("FreeScriptLibrary");

        m_pGetCreatureAI =
            script_library_->symbol<CreatureAI*(MANGOS_IMPORT*)(Creature*)>(
                "GetCreatureAI");
        m_pCreateInstanceData =
            script_library_->symbol<InstanceData*(MANGOS_IMPORT*)(Map*)>(
                "CreateInstanceData");

        m_pOnGossipHello =
            script_library_->symbol<bool(MANGOS_IMPORT*)(Player*, Creature*)>(
                "GossipHello");
        m_pOnGOGossipHello =
            script_library_->symbol<bool(MANGOS_IMPORT*)(Player*, GameObject*)>(
                "GOGossipHello");
        m_pOnGossipSelect =
            script_library_->symbol<bool(MANGOS_IMPORT*)(Player*, Creature*,
                uint32, uint32)>("GossipSelect");
        m_pOnGOGossipSelect =
            script_library_->symbol<bool(MANGOS_IMPORT*)(Player*, GameObject*,
                uint32, uint32)>("GOGossipSelect");
        m_pOnGossipSelectWithCode =
            script_library_->symbol<bool(MANGOS_IMPORT*)(Player*, Creature*,
                uint32, uint32, const char*)>("GossipSelectWithCode");
        m_pOnGOGossipSelectWithCode =
            script_library_->symbol<bool(MANGOS_IMPORT*)(Player*, GameObject*,
                uint32, uint32, const char*)>("GOGossipSelectWithCode");

        m_pOnQuestAccept = script_library_->symbol<bool(
            MANGOS_IMPORT*)(Player*, Creature*, const Quest*)>("QuestAccept");
        m_pOnGOQuestAccept =
            script_library_->symbol<bool(MANGOS_IMPORT*)(Player*, GameObject*,
                const Quest*)>("GOQuestAccept");
        m_pOnItemQuestAccept = script_library_->symbol<bool(
            MANGOS_IMPORT*)(Player*, Item*, const Quest*)>("ItemQuestAccept");
        m_pOnQuestRewarded = script_library_->symbol<bool(
            MANGOS_IMPORT*)(Player*, Creature*, const Quest*)>("QuestRewarded");
        m_pOnGOQuestRewarded =
            script_library_->symbol<bool(MANGOS_IMPORT*)(Player*, GameObject*,
                const Quest*)>("GOQuestRewarded");
        m_pGetNPCDialogStatus =
            script_library_->symbol<uint32(MANGOS_IMPORT*)(Player*, Creature*)>(
                "GetNPCDialogStatus");
        m_pGetGODialogStatus = script_library_->symbol<uint32(
            MANGOS_IMPORT*)(Player*, GameObject*)>("GetGODialogStatus");

        m_pOnGOUse =
            script_library_->symbol<bool(MANGOS_IMPORT*)(Player*, GameObject*)>(
                "GOUse");
        m_pOnItemUse = script_library_->symbol<bool(MANGOS_IMPORT*)(Player*,
            Item*, const SpellCastTargets&)>("ItemUse");

        m_pOnAreaTrigger = script_library_->symbol<bool(
            MANGOS_IMPORT*)(Player*, const AreaTriggerEntry*)>("AreaTrigger");
        m_pOnProcessEvent = script_library_->symbol<bool(
            MANGOS_IMPORT*)(uint32, Object*, Object*, bool)>("ProcessEvent");

        m_pOnEffectDummyCreature =
            script_library_->symbol<bool(MANGOS_IMPORT*)(Unit*, uint32,
                SpellEffectIndex, Creature*)>("EffectDummyCreature");
        m_pOnEffectDummyGO = script_library_->symbol<bool(MANGOS_IMPORT*)(Unit*,
            uint32, SpellEffectIndex, GameObject*)>("EffectDummyGameObject");
        m_pOnEffectDummyItem =
            script_library_->symbol<bool(MANGOS_IMPORT*)(Unit*, uint32,
                SpellEffectIndex, Item*)>("EffectDummyItem");
        m_pOnAuraDummy =
            script_library_->symbol<bool(MANGOS_IMPORT*)(const Aura*, bool)>(
                "AuraDummy");
    }
    catch (std::exception& e)
    {
        m_pOnFreeScriptLibrary = nullptr; // prevent call before init
        UnloadScriptLibrary();
        return SCRIPT_LOAD_ERR_OUTDATED;
    }

    m_pOnInitScriptLibrary();
    return SCRIPT_LOAD_OK;
}

void ScriptMgr::UnloadScriptLibrary()
{
    if (!script_library_)
        return;

    if (m_pOnFreeScriptLibrary)
        m_pOnFreeScriptLibrary();

    delete script_library_;
    script_library_ = nullptr;

    m_pOnInitScriptLibrary = nullptr;
    m_pOnFreeScriptLibrary = nullptr;

    m_pGetCreatureAI = nullptr;
    m_pCreateInstanceData = nullptr;

    m_pOnGossipHello = nullptr;
    m_pOnGOGossipHello = nullptr;
    m_pOnGossipSelect = nullptr;
    m_pOnGOGossipSelect = nullptr;
    m_pOnGossipSelectWithCode = nullptr;
    m_pOnGOGossipSelectWithCode = nullptr;
    m_pOnQuestAccept = nullptr;
    m_pOnGOQuestAccept = nullptr;
    m_pOnItemQuestAccept = nullptr;
    m_pOnQuestRewarded = nullptr;
    m_pOnGOQuestRewarded = nullptr;
    m_pGetNPCDialogStatus = nullptr;
    m_pGetGODialogStatus = nullptr;
    m_pOnGOUse = nullptr;
    m_pOnItemUse = nullptr;
    m_pOnAreaTrigger = nullptr;
    m_pOnProcessEvent = nullptr;
    m_pOnEffectDummyCreature = nullptr;
    m_pOnEffectDummyGO = nullptr;
    m_pOnEffectDummyItem = nullptr;
    m_pOnAuraDummy = nullptr;
}

// Starters for events
bool StartEvents_Event(Map* map, uint32 id, Object* source, Object* target,
    bool isStart /*=true*/, Unit* forwardToPvp /*=NULL*/)
{
    assert(source);

    // Handle SD2 script
    if (sScriptMgr::Instance()->OnProcessEvent(id, source, target, isStart))
        return true;

    // Handle PvP Calls
    if (forwardToPvp && source->GetTypeId() == TYPEID_GAMEOBJECT)
    {
        BattleGround* bg = nullptr;
        OutdoorPvP* opvp = nullptr;
        if (forwardToPvp->GetTypeId() == TYPEID_PLAYER)
        {
            bg = ((Player*)forwardToPvp)->GetBattleGround();
            if (!bg)
                opvp = sOutdoorPvPMgr::Instance()->GetScript(
                    ((Player*)forwardToPvp)->GetZoneId());
        }
        else
        {
            if (map->IsBattleGroundOrArena())
                bg = ((BattleGroundMap*)map)->GetBG();
            else // Use the go, because GOs don't move
                opvp = sOutdoorPvPMgr::Instance()->GetScript(
                    ((GameObject*)source)->GetZoneId());
        }

        /*if (bg && bg->HandleEvent(id, static_cast<GameObject*>(source)))
            return true;*/

        if (opvp && opvp->HandleEvent(id, static_cast<GameObject*>(source)))
            return true;
    }

    /*Map::ScriptExecutionParam execParam =
    Map::SCRIPT_EXEC_PARAM_UNIQUE_BY_SOURCE_TARGET;
    if (source->isType(TYPEMASK_CREATURE_OR_GAMEOBJECT))
        execParam = Map::SCRIPT_EXEC_PARAM_UNIQUE_BY_SOURCE;
    else if (target && target->isType(TYPEMASK_CREATURE_OR_GAMEOBJECT))
        execParam = Map::SCRIPT_EXEC_PARAM_UNIQUE_BY_TARGET;*/

    /*return*/ map->ScriptsStart(
        sEventScripts, id, source, target /*, execParam*/);
    return true;
}

uint32 GetAreaTriggerScriptId(uint32 triggerId)
{
    return sScriptMgr::Instance()->GetAreaTriggerScriptId(triggerId);
}

uint32 GetEventIdScriptId(uint32 eventId)
{
    return sScriptMgr::Instance()->GetEventIdScriptId(eventId);
}

uint32 GetScriptId(const char* name)
{
    return sScriptMgr::Instance()->GetScriptId(name);
}

char const* GetScriptName(uint32 id)
{
    return sScriptMgr::Instance()->GetScriptName(id);
}

uint32 GetScriptIdsCount()
{
    return sScriptMgr::Instance()->GetScriptIdsCount();
}
