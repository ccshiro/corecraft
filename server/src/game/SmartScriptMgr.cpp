/*
 * Copyright (C) 2008-2012 TrinityCore <http://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "SmartScriptMgr.h"
#include "BehavioralAI.h"
#include "CreatureTextMgr.h"
#include "GameEventMgr.h"
#include "movement/WaypointMovementGenerator.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "ProgressBar.h"
#include "SpellMgr.h"
#include "Database/DatabaseEnv.h"
#include "maps/map_grid.h"

void SmartWaypointMgr::LoadFromDB()
{
    for (auto& elem : waypoint_map)
        delete elem.second;
    waypoint_map.clear();

    std::unique_ptr<QueryResult> result(WorldDatabase.PQuery(
        "SELECT entry, pointid, position_x, position_y, position_z FROM "
        "waypoints ORDER BY entry ASC, pointid ASC"));
    if (!result)
    {
        logging.info(
            "Loaded 0 SmartAI Waypoint Paths. DB table `waypoints` is "
            "empty.\n");
        return;
    }

    uint32 count = 0;
    uint32 total = 0;
    uint32 current_entry = 0;
    uint32 expected_id = 0;
    bool ignoreCurrent = false;

    BarGoLink bar(result->GetRowCount());

    do
    {
        bar.step();
        Field* fields = result->Fetch();
        uint32 entry = fields[0].GetUInt32();
        uint32 id = fields[1].GetUInt32();
        float x, y, z;
        x = fields[2].GetFloat();
        y = fields[3].GetFloat();
        z = fields[4].GetFloat();

        if (entry == 0)
        {
            logging.error(
                "SmartWaypointMgr::LoadFromDB: Skipped path with entry 0. "
                "Lowest entry must be 1.");
            continue;
        }

        if (ignoreCurrent && current_entry == entry)
            continue;
        ignoreCurrent = false;

        if (current_entry != entry)
        {
            // Starting on new path
            waypoint_map[entry] = new WPPath();
            expected_id = 0;
            ++count;
        }

        if (expected_id != id)
        {
            logging.error(
                "SmartWaypointMgr::LoadFromDB: Path entry %u, unexpected point "
                "id %u, expected %u. Ignoring rest of path.",
                entry, id, expected_id);
            ignoreCurrent = true;
            continue;
        }

        ++expected_id;
        (*waypoint_map[entry]).emplace_back(id, x, y, z);

        current_entry = entry;
        ++total;
    } while (result->NextRow());

    logging.info("Loaded %u SmartAI waypoint paths (total %u waypoints)\n",
        count, total);
}

SmartWaypointMgr::~SmartWaypointMgr()
{
    for (auto& elem : waypoint_map)
        delete elem.second;

    waypoint_map.clear();
}

void SmartGroupWaypointMgr::LoadFromDB()
{
    for (auto& elem : waypoint_map)
        delete elem.second;
    waypoint_map.clear();

    std::unique_ptr<QueryResult> result(WorldDatabase.PQuery(
        "SELECT id, point, x, y, z, o, delay, run, mmap FROM "
        "waypoints_group ORDER BY id ASC, point ASC"));
    if (!result)
    {
        logging.info(
            "Loaded 0 SmartAI Waypoint Paths. DB table `waypoints_group` is "
            "empty.\n");
        return;
    }

    uint32 count = 0;
    uint32 total = 0;
    uint32 current_id = 0;
    uint32 expected_point = 0;
    bool ignoreCurrent = false;

    BarGoLink bar(result->GetRowCount());

    do
    {
        bar.step();
        Field* fields = result->Fetch();
        uint32 id = fields[0].GetUInt32();
        uint32 point = fields[1].GetUInt32();
        float x, y, z, o;
        x = fields[2].GetFloat();
        y = fields[3].GetFloat();
        z = fields[4].GetFloat();
        o = fields[5].GetFloat();
        uint32 delay = fields[6].GetFloat();
        bool run = fields[7].GetBool();
        bool mmap = fields[8].GetBool();

        if (id == 0)
        {
            logging.error(
                "SmartGroupWaypointMgr::LoadFromDB: Skipped path with id 0.");
            continue;
        }

        if (ignoreCurrent && current_id == id)
            continue;
        ignoreCurrent = false;

        if (current_id != id)
        {
            // Starting on new path
            waypoint_map[id] = new std::vector<SmartGwp>;
            expected_point = 0;
            ++count;
        }

        if (expected_point != point)
        {
            logging.error(
                "SmartGroupWaypointMgr::LoadFromDB: Path id %u, unexpected "
                "point id %u, expected %u. Ignoring rest of path.",
                id, point, expected_point);
            ignoreCurrent = true;
            continue;
        }

        ++expected_point;
        waypoint_map[id]->emplace_back(SmartGwp(x, y, z, o, delay, run, mmap));

        current_id = id;
        ++total;
    } while (result->NextRow());

    logging.info(
        "Loaded %u SmartAI group waypoint paths (total %u waypoints)\n", count,
        total);
}

SmartGroupWaypointMgr::~SmartGroupWaypointMgr()
{
    for (auto& elem : waypoint_map)
        delete elem.second;
    waypoint_map.clear();
}

void SmartAIMgr::LoadSmartAIFromDB()
{
    for (auto& elem : mEventMap)
        elem.clear(); // Drop Existing SmartAI List

    std::unique_ptr<QueryResult> result(WorldDatabase.PQuery(
        "SELECT entryorguid, source_type, id, link, event_type, "
        "event_phase_mask, event_chance, event_flags, event_param1, "
        "event_param2, event_param3, event_param4, action_type, action_param1, "
        "action_param2, action_param3, action_param4, action_param5, "
        "action_param6, target_type, target_param1, target_param2, "
        "target_param3, target_param4, target_param5, target_x, target_y, "
        "target_z, target_o FROM smart_scripts ORDER BY entryorguid, "
        "source_type, id, link"));
    if (!result)
    {
        logging.info(
            "Loaded 0 SmartAI scripts. DB table `smartai_scripts` is "
            "empty.\n");
        return;
    }

    uint32 count = 0;
    BarGoLink bar(result->GetRowCount());

    do
    {
        bar.step();
        Field* fields = result->Fetch();

        SmartScriptHolder temp;

        temp.entryOrGuid = fields[0].GetInt32();
        SmartScriptType source_type = (SmartScriptType)fields[1].GetUInt8();
        if (source_type >= SMART_SCRIPT_TYPE_MAX)
        {
            logging.error(
                "SmartAIMgr::LoadSmartAIFromDB: invalid source_type (%u), "
                "skipped loading.",
                uint32(source_type));
            continue;
        }
        if (temp.entryOrGuid >= 0)
        {
            switch (source_type)
            {
            case SMART_SCRIPT_TYPE_CREATURE:
            {
                if (!sObjectMgr::Instance()->GetCreatureTemplate(
                        (uint32)temp.entryOrGuid))
                {
                    logging.error(
                        "SmartAIMgr::LoadSmartAIFromDB: Creature entry (%u) "
                        "does not exist, skipped loading.",
                        uint32(temp.entryOrGuid));
                    continue;
                }
                break;
            }
            case SMART_SCRIPT_TYPE_GAMEOBJECT:
            {
                if (!sObjectMgr::Instance()->GetGameObjectInfo(
                        (uint32)temp.entryOrGuid))
                {
                    logging.error(
                        "SmartAIMgr::LoadSmartAIFromDB: GameObject entry (%u) "
                        "does not exist, skipped loading.",
                        uint32(temp.entryOrGuid));
                    continue;
                }
                break;
            }
            case SMART_SCRIPT_TYPE_TIMED_ACTIONLIST:
                break; // nothing to check, really
            case SMART_SCRIPT_TYPE_AREATRIGGER:
            {
                if (!sAreaTriggerStore.LookupEntry((uint32)temp.entryOrGuid))
                {
                    logging.error(
                        "SmartAIMgr::LoadSmartAIFromDB: Areatrigger entry (%u) "
                        "does not exist, skipped loading.",
                        uint32(temp.entryOrGuid));
                    continue;
                }
                break;
            }
            default:
                logging.error(
                    "SmartAIMgr::LoadSmartAIFromDB: not yet implemented "
                    "source_type %u",
                    (uint32)source_type);
                continue;
            }
        }
        else
        {
            if (!sObjectMgr::Instance()->GetCreatureData(
                    uint32(abs(temp.entryOrGuid))))
            {
                logging.error(
                    "SmartAIMgr::LoadSmartAIFromDB: Creature guid (%u) does "
                    "not exist, skipped loading.",
                    uint32(abs(temp.entryOrGuid)));
                continue;
            }
        }

        temp.source_type = source_type;
        temp.event_id = fields[2].GetUInt16();
        temp.link = fields[3].GetUInt16();
        temp.event.type = (SMART_EVENT)fields[4].GetUInt8();
        temp.event.event_phase_mask = fields[5].GetUInt32();
        temp.event.event_chance = fields[6].GetUInt8();
        temp.event.event_flags = fields[7].GetUInt32();

        temp.event.raw.param1 = fields[8].GetUInt32();
        temp.event.raw.param2 = fields[9].GetUInt32();
        temp.event.raw.param3 = fields[10].GetUInt32();
        temp.event.raw.param4 = fields[11].GetUInt32();

        temp.action.type = (SMART_ACTION)fields[12].GetUInt8();
        temp.action.raw.param1 = fields[13].GetUInt32();
        temp.action.raw.param2 = fields[14].GetUInt32();
        temp.action.raw.param3 = fields[15].GetUInt32();
        temp.action.raw.param4 = fields[16].GetUInt32();
        temp.action.raw.param5 = fields[17].GetUInt32();
        temp.action.raw.param6 = fields[18].GetUInt32();

        temp.target.type = (SMARTAI_TARGETS)fields[19].GetUInt8();
        temp.target.raw.param1 = fields[20].GetUInt32();
        temp.target.raw.param2 = fields[21].GetUInt32();
        temp.target.raw.param3 = fields[22].GetUInt32();
        temp.target.raw.param4 = fields[23].GetUInt32();
        temp.target.raw.param5 = fields[24].GetUInt32();
        temp.target.x = fields[25].GetFloat();
        temp.target.y = fields[26].GetFloat();
        temp.target.z = fields[27].GetFloat();
        temp.target.o = fields[28].GetFloat();

        // check target
        if (!IsTargetValid(temp))
            continue;

        // check all event and action params
        if (!IsEventValid(temp))
            continue;

        // creature entry / guid not found in storage, create empty event list
        // for it and increase counters
        if (mEventMap[source_type].find(temp.entryOrGuid) ==
            mEventMap[source_type].end())
        {
            ++count;
            SmartAIEventList eventList;
            mEventMap[source_type][temp.entryOrGuid] = eventList;
        }
        // store the new event
        mEventMap[source_type][temp.entryOrGuid].push_back(temp);
    } while (result->NextRow());

    logging.info("Loaded %u SmartAI scripts\n", count);
}

bool SmartAIMgr::IsTargetValid(SmartScriptHolder const& e)
{
    switch (e.GetTargetType())
    {
    // Normal targets with no checks
    case SMART_TARGET_NONE:
    case SMART_TARGET_SELF:
    case SMART_TARGET_VICTIM:
    case SMART_TARGET_HOSTILE_SECOND_AGGRO:
    case SMART_TARGET_HOSTILE_LAST_AGGRO:
    case SMART_TARGET_HOSTILE_RANDOM:
    case SMART_TARGET_HOSTILE_RANDOM_NOT_TOP:
    case SMART_TARGET_ACTION_INVOKER:
    case SMART_TARGET_POSITION:
    case SMART_TARGET_STORED:
    case SMART_TARGET_INVOKER_PARTY:
    case SMART_TARGET_OWNER:
    case SMART_TARGET_THREAT_LIST:
    case SMART_TARGET_SAVED_POS:
    case SMART_TARGET_CHARMER:
    case SMART_TARGET_NONE_SELECTED_TARGET:
    case SMART_TARGET_CREATURE_GROUP:
        break;
    // Normal targets with checks
    case SMART_TARGET_CREATURE_GUID:
    {
        if (!e.target.guid.entry || !IsCreatureValid(e, e.target.guid.entry))
            return false;
        break;
    }
    case SMART_TARGET_GAMEOBJECT_GUID:
    {
        if (!e.target.guid.entry || !IsGameObjectValid(e, e.target.guid.entry))
            return false;
        break;
    }
    case SMART_TARGET_RELATIVE_POSITION:
    {
        if (!IsMinMaxValid(e, e.target.relativePos.minAngle,
                e.target.relativePos.maxAngle) ||
            !IsMinMaxValid(
                e, e.target.relativePos.minDist, e.target.relativePos.maxDist))
            return false;
        break;
    }
    // Grid targets
    case SMART_TARGET_GRID_STANDARD:
    case SMART_TARGET_GRID_ENTRY:
    {
        if (!IsMinMaxValid(e, e.target.grid.min_dist, e.target.grid.max_dist))
            return false;
        if ((e.target.grid.type_mask & ~0x1FF) != 0 ||
            e.target.grid.type_mask == 0)
        {
            logging.error(
                "SmartAIMgr: invalid type mask for Entry: %u, Id: %u.",
                e.entryOrGuid, e.event_id);
            return false;
        }
        if ((e.target.grid.selection_mask & ~0x1FF) != 0)
        {
            logging.error(
                "SmartAIMgr: invalid selection mask for Entry: %u, Id: %u.",
                e.entryOrGuid, e.event_id);
            return false;
        }
        break;
    }
    // Not a recognized target
    default:
        logging.error(
            "SmartAIMgr: Not handled target_type(%u), Entry %d SourceType %u "
            "Event %u Action %u, skipped.",
            e.GetTargetType(), e.entryOrGuid, e.GetScriptType(), e.event_id,
            e.GetActionType());
        return false;
    }
    return true;
}

bool SmartAIMgr::IsEventValid(SmartScriptHolder& e)
{
    if (e.event.type >= SMART_EVENT_END)
    {
        logging.error(
            "SmartAIMgr: EntryOrGuid %d using event(%u) has invalid event type "
            "(%u), skipped.",
            e.entryOrGuid, e.event_id, e.GetEventType());
        return false;
    }
    // in SMART_SCRIPT_TYPE_TIMED_ACTIONLIST all event types are overriden by
    // core
    if (e.GetScriptType() != SMART_SCRIPT_TYPE_TIMED_ACTIONLIST &&
        !(SmartAIEventMask[e.event.type][1] &
            SmartAITypeMask[e.GetScriptType()][1]))
    {
        logging.error(
            "SmartAIMgr: EntryOrGuid %d, event type %u can not be used for "
            "Script type %u",
            e.entryOrGuid, e.GetEventType(), e.GetScriptType());
        return false;
    }
    if (e.action.type < 0 || e.action.type >= SMART_ACTION_END)
    {
        logging.error(
            "SmartAIMgr: EntryOrGuid %d using event(%u) has invalid action "
            "type (%u), skipped.",
            e.entryOrGuid, e.event_id, e.GetActionType());
        return false;
    }
    if (e.event.event_phase_mask > SMART_EVENT_PHASE_MAX_VALUE)
    {
        logging.error(
            "SmartAIMgr: EntryOrGuid %d using event(%u) has invalid phase mask "
            "(%u), skipped.",
            e.entryOrGuid, e.event_id, e.event.event_phase_mask);
        return false;
    }
    if (e.event.event_flags > SMART_EVENT_FLAGS_ALL)
    {
        logging.error(
            "SmartAIMgr: EntryOrGuid %d using event(%u) has invalid event "
            "flags (%u), skipped.",
            e.entryOrGuid, e.event_id, e.event.event_flags);
        return false;
    }
    if (e.GetScriptType() == SMART_SCRIPT_TYPE_TIMED_ACTIONLIST)
    {
        e.event.type = SMART_EVENT_UPDATE_OOC; // force default OOC, can change
                                               // when calling the script!
        if (!IsMinMaxValid(
                e, e.event.minMaxRepeat.min, e.event.minMaxRepeat.max))
            return false;

        if (!IsMinMaxValid(e, e.event.minMaxRepeat.repeatMin,
                e.event.minMaxRepeat.repeatMax))
            return false;
    }
    else
    {
        uint32 type = e.event.type;
        switch (type)
        {
        case SMART_EVENT_UPDATE:
        case SMART_EVENT_UPDATE_IC:
        case SMART_EVENT_UPDATE_OOC:
        case SMART_EVENT_HEALTH_PCT:
        case SMART_EVENT_HEALTH_FLAT:
        case SMART_EVENT_MANA_PCT:
        case SMART_EVENT_TARGET_HEALTH_PCT:
        case SMART_EVENT_TARGET_MANA_PCT:
        case SMART_EVENT_RANGE:
        case SMART_EVENT_DAMAGED:
        case SMART_EVENT_DAMAGED_TARGET:
        case SMART_EVENT_RECEIVE_HEAL:
            if (!IsMinMaxValid(
                    e, e.event.minMaxRepeat.min, e.event.minMaxRepeat.max))
                return false;

            if (!IsMinMaxValid(e, e.event.minMaxRepeat.repeatMin,
                    e.event.minMaxRepeat.repeatMax))
                return false;
            break;
        case SMART_EVENT_SPELLHIT:
        case SMART_EVENT_SPELLHIT_TARGET:
            if (e.event.spellHit.spell)
            {
                SpellEntry const* spellInfo =
                    sSpellStore.LookupEntry(e.event.spellHit.spell);
                if (!spellInfo)
                {
                    logging.error(
                        "SmartAIMgr: Entry %d SourceType %u Event %u Action %u "
                        "uses non-existent Spell entry %u, skipped.",
                        e.entryOrGuid, e.GetScriptType(), e.event_id,
                        e.GetActionType(), e.event.spellHit.spell);
                    return false;
                }
                if (e.event.spellHit.school &&
                    (e.event.spellHit.school & spellInfo->SchoolMask) !=
                        spellInfo->SchoolMask)
                {
                    logging.error(
                        "SmartAIMgr: Entry %d SourceType %u Event %u Action %u "
                        "uses Spell entry %u with invalid school mask, "
                        "skipped.",
                        e.entryOrGuid, e.GetScriptType(), e.event_id,
                        e.GetActionType(), e.event.spellHit.spell);
                    return false;
                }
            }
            if (!IsMinMaxValid(e, e.event.spellHit.cooldownMin,
                    e.event.spellHit.cooldownMax))
                return false;
            break;
        case SMART_EVENT_OOC_LOS:
        case SMART_EVENT_IC_LOS:
            if (!IsMinMaxValid(
                    e, e.event.los.cooldownMin, e.event.los.cooldownMax))
                return false;
            break;
        case SMART_EVENT_RESPAWN:
            if (e.event.respawn.type == SMART_SCRIPT_RESPAWN_CONDITION_MAP &&
                !sMapStore.LookupEntry(e.event.respawn.map))
            {
                logging.error(
                    "SmartAIMgr: Entry %d SourceType %u Event %u Action %u "
                    "uses non-existent Map entry %u, skipped.",
                    e.entryOrGuid, e.GetScriptType(), e.event_id,
                    e.GetActionType(), e.event.respawn.map);
                return false;
            }
            if (e.event.respawn.type == SMART_SCRIPT_RESPAWN_CONDITION_AREA &&
                !GetAreaEntryByAreaID(e.event.respawn.area))
            {
                logging.error(
                    "SmartAIMgr: Entry %d SourceType %u Event %u Action %u "
                    "uses non-existent Area entry %u, skipped.",
                    e.entryOrGuid, e.GetScriptType(), e.event_id,
                    e.GetActionType(), e.event.respawn.area);
                return false;
            }
            break;
        case SMART_EVENT_FRIENDLY_HEALTH_IC:
        case SMART_EVENT_FRIENDLY_HEALTH:
            if (!NotNULL(e, e.event.friendlyHealth.radius))
                return false;

            if (!IsMinMaxValid(e, e.event.friendlyHealth.repeatMin,
                    e.event.friendlyHealth.repeatMax))
                return false;
            break;
        case SMART_EVENT_FRIENDLY_IS_CC:
            if (!IsMinMaxValid(e, e.event.friendlyCC.repeatMin,
                    e.event.friendlyCC.repeatMax))
                return false;
            break;
        case SMART_EVENT_FRIENDLY_MISSING_BUFF:
        {
            if (!IsSpellValid(e, e.event.missingBuff.spell))
                return false;

            if (!NotNULL(e, e.event.missingBuff.radius))
                return false;

            if (!IsMinMaxValid(e, e.event.missingBuff.repeatMin,
                    e.event.missingBuff.repeatMax))
                return false;
            break;
        }
        case SMART_EVENT_KILL:
            if (!IsMinMaxValid(
                    e, e.event.kill.cooldownMin, e.event.kill.cooldownMax))
                return false;

            if (e.event.kill.creature &&
                !IsCreatureValid(e, e.event.kill.creature))
                return false;
            break;
        case SMART_EVENT_TARGET_CASTING:
        case SMART_EVENT_PASSENGER_BOARDED:
        case SMART_EVENT_PASSENGER_REMOVED:
            if (!IsMinMaxValid(
                    e, e.event.minMax.repeatMin, e.event.minMax.repeatMax))
                return false;
            break;
        case SMART_EVENT_SUMMON_DESPAWNED:
        case SMART_EVENT_SUMMONED_UNIT:
            if (e.event.summoned.creature &&
                !IsCreatureValid(e, e.event.summoned.creature))
                return false;

            if (!IsMinMaxValid(e, e.event.summoned.cooldownMin,
                    e.event.summoned.cooldownMax))
                return false;
            break;
        case SMART_EVENT_ACCEPTED_QUEST:
        case SMART_EVENT_REWARD_QUEST:
            if (e.event.quest.quest && !IsQuestValid(e, e.event.quest.quest))
                return false;
            break;
        case SMART_EVENT_RECEIVE_EMOTE:
        {
            if (e.event.emote.emote &&
                !IsTextEmoteValid(e, e.event.emote.emote))
                return false;

            if (!IsMinMaxValid(
                    e, e.event.emote.cooldownMin, e.event.emote.cooldownMax))
                return false;
            break;
        }
        case SMART_EVENT_HAS_AURA:
        case SMART_EVENT_TARGET_BUFFED:
        {
            if (!IsSpellValid(e, e.event.aura.spell))
                return false;

            if (!IsMinMaxValid(
                    e, e.event.aura.repeatMin, e.event.aura.repeatMax))
                return false;
            break;
        }
        case SMART_EVENT_TRANSPORT_ADDCREATURE:
        {
            if (e.event.transportAddCreature.creature &&
                !IsCreatureValid(e, e.event.transportAddCreature.creature))
                return false;
            break;
        }
        case SMART_EVENT_MOVEMENTINFORM:
        {
            if (e.event.movementInform.type >= (int)movement::gen::max)
            {
                logging.error(
                    "SmartAIMgr: Entry %d SourceType %u Event %u Action %u "
                    "uses invalid Motion type %u, skipped.",
                    e.entryOrGuid, e.GetScriptType(), e.event_id,
                    e.GetActionType(), e.event.movementInform.type);
                return false;
            }
            break;
        }
        case SMART_EVENT_DATA_SET:
        {
            if (!IsMinMaxValid(e, e.event.dataSet.cooldownMin,
                    e.event.dataSet.cooldownMax))
                return false;
            break;
        }
        case SMART_EVENT_AREATRIGGER_ONTRIGGER:
        {
            if (e.event.areatrigger.id &&
                !IsAreaTriggerValid(e, e.event.areatrigger.id))
                return false;
            break;
        }
        case SMART_EVENT_TEXT_OVER:
            // if (e.event.textOver.textGroupID && !IsTextValid(e,
            // e.event.textOver.textGroupID)) return false;// 0 is a valid text
            // group!
            break;
        case SMART_EVENT_LINK:
        {
            if (e.link && e.link == e.event_id)
            {
                logging.error(
                    "SmartAIMgr: Entry %d SourceType %u, Event %u, Link Event "
                    "is linking self (infinite loop), skipped.",
                    e.entryOrGuid, e.GetScriptType(), e.event_id);
                return false;
            }
            if (!IsMinMaxValid(
                    e, e.event.minMax.repeatMin, e.event.minMax.repeatMax))
                return false;
            break;
        }
        case SMART_EVENT_DUMMY_EFFECT:
        {
            if (!IsSpellValid(e, e.event.dummy.spell))
                return false;

            if (e.event.dummy.effIndex > EFFECT_INDEX_2)
                return false;
            break;
        }
        case SMART_EVENT_IS_BEHIND_TARGET:
        {
            if (!IsMinMaxValid(e, e.event.behindTarget.cooldownMin,
                    e.event.behindTarget.cooldownMax))
                return false;
            break;
        }
        case SMART_EVENT_GAME_EVENT_START:
        case SMART_EVENT_GAME_EVENT_END:
        {
            GameEventMgr::GameEventDataMap const& events =
                sGameEventMgr::Instance()->GetEventMap();
            if (e.event.gameEvent.gameEventId >= events.size() ||
                !events[e.event.gameEvent.gameEventId].isValid())
                return false;
            break;
        }
        case SMART_EVENT_ACTION_DONE:
        {
            if (e.event.doAction.eventId > 1003 /*EVENT_CHARGE*/) // FIXME
            {
                logging.error(
                    "SmartAIMgr: Entry %d SourceType %u Event %u Action %u "
                    "uses invalid event id %u, skipped.",
                    e.entryOrGuid, e.GetScriptType(), e.event_id,
                    e.GetActionType(), e.event.doAction.eventId);
                return false;
            }
            break;
        }
        case SMART_EVENT_HAS_AURA_WITH_MECHANIC:
        {
            if (e.event.auraMechanic.mechanicMask == 0 ||
                !IsMinMaxValid(e, e.event.auraMechanic.cooldownMin,
                    e.event.auraMechanic.cooldownMax))
                return false;
            break;
        }
        case SMART_EVENT_RESET:
        {
            if (e.event.raw.param1 >= 0x40)
            {
                logging.error(
                    "SmartAIMgr: Entry %d SourceType %u Event %u Action %u "
                    "has invalid Reset type mask, skipped.",
                    e.entryOrGuid, e.GetScriptType(), e.event_id,
                    e.GetActionType());
                return false;
            }
            break;
        }
        case SMART_EVENT_GO_STATE_CHANGED:
        case SMART_EVENT_GO_EVENT_INFORM:
        case SMART_EVENT_TIMED_EVENT_TRIGGERED:
        case SMART_EVENT_INSTANCE_PLAYER_ENTER:
        case SMART_EVENT_TRANSPORT_RELOCATE:
        case SMART_EVENT_CHARMED:
        case SMART_EVENT_CHARM_EXPIRED:
        case SMART_EVENT_CORPSE_REMOVED:
        case SMART_EVENT_AI_INIT:
        case SMART_EVENT_TRANSPORT_ADDPLAYER:
        case SMART_EVENT_TRANSPORT_REMOVE_PLAYER:
        case SMART_EVENT_AGGRO:
        case SMART_EVENT_DEATH:
        case SMART_EVENT_EVADE:
        case SMART_EVENT_REACHED_HOME:
        case SMART_EVENT_QUEST_ACCEPTED:
        case SMART_EVENT_QUEST_OBJ_COPLETETION:
        case SMART_EVENT_QUEST_COMPLETION:
        case SMART_EVENT_QUEST_REWARDED:
        case SMART_EVENT_QUEST_FAIL:
        case SMART_EVENT_JUST_SUMMONED:
        case SMART_EVENT_WAYPOINT_START:
        case SMART_EVENT_WAYPOINT_REACHED:
        case SMART_EVENT_WAYPOINT_PAUSED:
        case SMART_EVENT_WAYPOINT_RESUMED:
        case SMART_EVENT_WAYPOINT_STOPPED:
        case SMART_EVENT_WAYPOINT_ENDED:
        case SMART_EVENT_GOSSIP_SELECT:
        case SMART_EVENT_GOSSIP_HELLO:
        case SMART_EVENT_JUST_CREATED:
        case SMART_EVENT_ON_SPELLCLICK:
        case SMART_EVENT_AI_NOTIFICATION:
        case SMART_EVENT_SPAWN:
        case SMART_EVENT_BEFORE_DEATH:
        case SMART_EVENT_DISENGAGE_CALLBACK:
        case SMART_EVENT_LEASH:
            break;
        default:
            logging.error(
                "SmartAIMgr: Not handled event_type(%u), Entry %d SourceType "
                "%u Event %u Action %u, skipped.",
                e.GetEventType(), e.entryOrGuid, e.GetScriptType(), e.event_id,
                e.GetActionType());
            return false;
        }
    }

    switch (e.GetActionType())
    {
    case SMART_ACTION_SET_FACTION:
        if (e.action.faction.factionID &&
            !sFactionTemplateStore.LookupEntry(e.action.faction.factionID))
        {
            logging.error(
                "SmartAIMgr: Entry %d SourceType %u Event %u Action %u uses "
                "non-existent Faction %u, skipped.",
                e.entryOrGuid, e.GetScriptType(), e.event_id, e.GetActionType(),
                e.action.faction.factionID);
            return false;
        }
        break;
    case SMART_ACTION_MORPH_TO_ENTRY_OR_MODEL:
    case SMART_ACTION_MOUNT_TO_ENTRY_OR_MODEL:
        if (e.action.morphOrMount.creature || e.action.morphOrMount.model)
        {
            if (e.action.morphOrMount.creature > 0 &&
                !sObjectMgr::Instance()->GetCreatureTemplate(
                    e.action.morphOrMount.creature))
            {
                logging.error(
                    "SmartAIMgr: Entry %d SourceType %u Event %u Action %u "
                    "uses non-existent Creature entry %u, skipped.",
                    e.entryOrGuid, e.GetScriptType(), e.event_id,
                    e.GetActionType(), e.action.morphOrMount.creature);
                return false;
            }

            if (e.action.morphOrMount.model)
            {
                if (e.action.morphOrMount.creature)
                {
                    logging.error(
                        "SmartAIMgr: Entry %d SourceType %u Event %u Action %u "
                        "has ModelID set with also set CreatureId, skipped.",
                        e.entryOrGuid, e.GetScriptType(), e.event_id,
                        e.GetActionType());
                    return false;
                }
                else if (!sCreatureDisplayInfoStore.LookupEntry(
                             e.action.morphOrMount.model))
                {
                    logging.error(
                        "SmartAIMgr: Entry %d SourceType %u Event %u Action %u "
                        "uses non-existent Model id %u, skipped.",
                        e.entryOrGuid, e.GetScriptType(), e.event_id,
                        e.GetActionType(), e.action.morphOrMount.model);
                    return false;
                }
            }
        }
        break;
    case SMART_ACTION_SOUND:
        if (!IsSoundValid(e, e.action.sound.sound))
            return false;
        if (e.action.sound.range > TEXT_RANGE_WORLD)
        {
            logging.error(
                "SmartAIMgr: Entry %d SourceType %u Event %u Action %u uses "
                "invalid Text Range %u, skipped.",
                e.entryOrGuid, e.GetScriptType(), e.event_id, e.GetActionType(),
                e.action.sound.range);
            return false;
        }
        break;
    case SMART_ACTION_SET_EMOTE_STATE:
    case SMART_ACTION_PLAY_EMOTE:
        if (!IsEmoteValid(e, e.action.emote.emote))
            return false;
        break;
    case SMART_ACTION_FAIL_QUEST:
        if (!e.action.questGroup.quest ||
            !IsQuestValid(e, e.action.questGroup.quest))
            return false;
        break;
    case SMART_ACTION_ADD_QUEST:
        if (!e.action.quest.quest || !IsQuestValid(e, e.action.quest.quest))
            return false;
        break;
    case SMART_ACTION_ACTIVATE_TAXI:
    {
        if (!sTaxiPathStore.LookupEntry(e.action.taxi.id))
        {
            logging.error(
                "SmartAIMgr: Entry %d SourceType %u Event %u Action %u uses "
                "invalid Taxi path ID %u, skipped.",
                e.entryOrGuid, e.GetScriptType(), e.event_id, e.GetActionType(),
                e.action.taxi.id);
            return false;
        }
        break;
    }
    case SMART_ACTION_RANDOM_EMOTE:
        if (e.action.randomEmote.emote1 &&
            !IsEmoteValid(e, e.action.randomEmote.emote1))
            return false;

        if (e.action.randomEmote.emote2 &&
            !IsEmoteValid(e, e.action.randomEmote.emote2))
            return false;

        if (e.action.randomEmote.emote3 &&
            !IsEmoteValid(e, e.action.randomEmote.emote3))
            return false;

        if (e.action.randomEmote.emote4 &&
            !IsEmoteValid(e, e.action.randomEmote.emote4))
            return false;

        if (e.action.randomEmote.emote5 &&
            !IsEmoteValid(e, e.action.randomEmote.emote5))
            return false;

        if (e.action.randomEmote.emote6 &&
            !IsEmoteValid(e, e.action.randomEmote.emote6))
            return false;
        break;
    case SMART_ACTION_ADD_AURA:
    case SMART_ACTION_CAST:
    case SMART_ACTION_INVOKER_CAST:
        if (!IsSpellValid(e, e.action.cast.spell))
            return false;
        break;
    case SMART_ACTION_CALL_AREAEXPLOREDOREVENTHAPPENS:
    case SMART_ACTION_CALL_GROUPEVENTHAPPENS:
        if (Quest const* qid =
                sObjectMgr::Instance()->GetQuestTemplate(e.action.quest.quest))
        {
            if (!qid->HasSpecialFlag(QUEST_SPECIAL_FLAG_EXPLORATION_OR_EVENT))
            {
                logging.error(
                    "SmartAIMgr: Entry %d SourceType %u Event %u Action %u "
                    "SpecialFlags for Quest entry %u does not include "
                    "FLAGS_EXPLORATION_OR_EVENT(2), skipped.",
                    e.entryOrGuid, e.GetScriptType(), e.event_id,
                    e.GetActionType(), e.action.quest.quest);
                return false;
            }
        }
        else
        {
            logging.error(
                "SmartAIMgr: Entry %d SourceType %u Event %u Action %u uses "
                "non-existent Quest entry %u, skipped.",
                e.entryOrGuid, e.GetScriptType(), e.event_id, e.GetActionType(),
                e.action.quest.quest);
            return false;
        }
        break;
    case SMART_ACTION_SEND_CASTCREATUREORGO:
        if (!IsQuestValid(e, e.action.castCreatureOrGO.quest))
            return false;

        if (!IsSpellValid(e, e.action.castCreatureOrGO.spell))
            return false;
        break;
    case SMART_ACTION_SET_EVENT_PHASE:
        if (e.action.setEventPhase.phase > SMART_EVENT_PHASE_MAX)
        {
            logging.error(
                "SmartAIMgr: Entry %d SourceType %u Event %u Action %u "
                "attempts to set phase %u. Phase mask cannot be used past "
                "phase %u, skipped.",
                e.entryOrGuid, e.GetScriptType(), e.event_id, e.GetActionType(),
                e.action.setEventPhase.phase, SMART_EVENT_PHASE_MAX - 1);
            return false;
        }
        break;
    case SMART_ACTION_INC_EVENT_PHASE:
        if (!e.action.incEventPhase.inc && !e.action.incEventPhase.dec)
        {
            logging.error(
                "SmartAIMgr: Entry %d SourceType %u Event %u Action %u is "
                "incrementing phase by 0, skipped.",
                e.entryOrGuid, e.GetScriptType(), e.event_id,
                e.GetActionType());
            return false;
        }
        else if (e.action.incEventPhase.inc > SMART_EVENT_PHASE_MAX ||
                 e.action.incEventPhase.dec > SMART_EVENT_PHASE_MAX)
        {
            logging.error(
                "SmartAIMgr: Entry %d SourceType %u Event %u Action %u "
                "attempts to increment phase by too large value, skipped.",
                e.entryOrGuid, e.GetScriptType(), e.event_id,
                e.GetActionType());
            return false;
        }
        break;
    case SMART_ACTION_CALL_CASTEDCREATUREORGO:
        if (!IsCreatureValid(e, e.action.castedCreatureOrGO.creature))
            return false;

        if (!IsSpellValid(e, e.action.castedCreatureOrGO.spell))
            return false;
        break;
    case SMART_ACTION_REMOVEAURASFROMSPELL:
        if (e.action.removeAura.spell != 0 &&
            !IsSpellValid(e, e.action.removeAura.spell))
            return false;
        break;
    case SMART_ACTION_RANDOM_PHASE:
    {
        if (e.action.randomPhase.phase1 > SMART_EVENT_PHASE_MAX ||
            e.action.randomPhase.phase2 > SMART_EVENT_PHASE_MAX ||
            e.action.randomPhase.phase3 > SMART_EVENT_PHASE_MAX ||
            e.action.randomPhase.phase4 > SMART_EVENT_PHASE_MAX ||
            e.action.randomPhase.phase5 > SMART_EVENT_PHASE_MAX ||
            e.action.randomPhase.phase6 > SMART_EVENT_PHASE_MAX)
        {
            logging.error(
                "SmartAIMgr: Entry %d SourceType %u Event %u Action %u "
                "attempts to set invalid phase, skipped.",
                e.entryOrGuid, e.GetScriptType(), e.event_id,
                e.GetActionType());
            return false;
        }
    }
    break;
    case SMART_ACTION_RANDOM_PHASE_RANGE: // PhaseMin, PhaseMax
    {
        if (e.action.randomPhaseRange.phaseMin > SMART_EVENT_PHASE_MAX ||
            e.action.randomPhaseRange.phaseMax > SMART_EVENT_PHASE_MAX)
        {
            logging.error(
                "SmartAIMgr: Entry %d SourceType %u Event %u Action %u "
                "attempts to set invalid phase, skipped.",
                e.entryOrGuid, e.GetScriptType(), e.event_id,
                e.GetActionType());
            return false;
        }
        if (!IsMinMaxValid(e, e.action.randomPhaseRange.phaseMin,
                e.action.randomPhaseRange.phaseMax))
            return false;
        break;
    }
    case SMART_ACTION_SUMMON_CREATURE:
        if (!IsCreatureValid(e, e.action.summonCreature.creature))
            return false;
        if (e.action.summonCreature.type < TEMPSUMMON_TIMED_OR_DEAD_DESPAWN ||
            e.action.summonCreature.type > TEMPSUMMON_TIMED_DEATH)
        {
            logging.error(
                "SmartAIMgr: Entry %d SourceType %u Event %u Action %u uses "
                "incorrect TempSummonType %u, skipped.",
                e.entryOrGuid, e.GetScriptType(), e.event_id, e.GetActionType(),
                e.action.summonCreature.type);
            return false;
        }
        break;
    case SMART_ACTION_CALL_KILLEDMONSTER:
        if (!IsCreatureValid(e, e.action.killedMonster.creature))
            return false;
        break;
    case SMART_ACTION_UPDATE_TEMPLATE:
        if (e.action.updateTemplate.creature &&
            !IsCreatureValid(e, e.action.updateTemplate.creature))
            return false;
        break;
    case SMART_ACTION_SET_SHEATH:
        if (e.action.setSheath.sheath &&
            e.action.setSheath.sheath >= MAX_SHEATH_STATE)
        {
            logging.error(
                "SmartAIMgr: Entry %d SourceType %u Event %u Action %u uses "
                "incorrect Sheath state %u, skipped.",
                e.entryOrGuid, e.GetScriptType(), e.event_id, e.GetActionType(),
                e.action.setSheath.sheath);
            return false;
        }
        break;
    case SMART_ACTION_SET_REACT_STATE:
    {
        if (e.action.react.state > REACT_AGGRESSIVE)
        {
            logging.error(
                "SmartAIMgr: Creature %d Event %u Action %u uses invalid React "
                "State %u, skipped.",
                e.entryOrGuid, e.event_id, e.GetActionType(),
                e.action.react.state);
            return false;
        }
        break;
    }
    case SMART_ACTION_SUMMON_GO:
        if (!IsGameObjectValid(e, e.action.summonGO.entry))
            return false;
        break;
    case SMART_ACTION_ADD_ITEM:
    case SMART_ACTION_REMOVE_ITEM:
        if (!IsItemValid(e, e.action.item.entry))
            return false;

        if (!NotNULL(e, e.action.item.count))
            return false;
        break;
    case SMART_ACTION_WP_STOP:
        if (e.action.wpStop.quest && !IsQuestValid(e, e.action.wpStop.quest))
            return false;
        break;
    case SMART_ACTION_WP_START:
    {
        if (!sSmartWaypointMgr::Instance()->GetPath(e.action.wpStart.pathID))
        {
            logging.error(
                "SmartAIMgr: Creature %d Event %u Action %u uses non-existent "
                "WaypointPath id %u, skipped.",
                e.entryOrGuid, e.event_id, e.GetActionType(),
                e.action.wpStart.pathID);
            return false;
        }
        if (e.action.wpStart.quest && !IsQuestValid(e, e.action.wpStart.quest))
            return false;
        if (e.action.wpStart.reactState > REACT_AGGRESSIVE)
        {
            logging.error(
                "SmartAIMgr: Creature %d Event %u Action %u uses invalid React "
                "State %u, skipped.",
                e.entryOrGuid, e.event_id, e.GetActionType(),
                e.action.wpStart.reactState);
            return false;
        }
        break;
    }
    case SMART_ACTION_CREATE_TIMED_EVENT:
    {
        if (!IsMinMaxValid(e, e.action.timeEvent.min, e.action.timeEvent.max))
            return false;

        if (!IsMinMaxValid(
                e, e.action.timeEvent.repeatMin, e.action.timeEvent.repeatMax))
            return false;
        break;
    }
    case SMART_ACTION_RANDOM_ACTION_IN_RANGE:
    {
        if (e.action.randomAction.minAction >= e.action.randomAction.maxAction)
        {
            logging.error(
                "SmartAIMgr: Creature %d Event %u Action %u has invalid "
                "SMART_ACTION_RANDOM_ACTION_IN_RANGE type in smart_scripts "
                "table",
                e.entryOrGuid, e.event_id, e.GetActionType());
            return false;
        }
        break;
    }
    case SMART_ACTION_SET_HEALTH:
    {
        int cnt = 0;
        if (e.action.setHealth.flatHp > 0)
            ++cnt;
        if (e.action.setHealth.pctHp > 0)
            ++cnt;
        if (e.action.setHealth.flatIncr > 0)
            ++cnt;
        if (e.action.setHealth.flatDecr > 0)
            ++cnt;
        if (e.action.setHealth.pctIncr > 0)
            ++cnt;
        if (e.action.setHealth.pctDecr > 0)
            ++cnt;
        if (cnt != 1)
        {
            logging.error(
                "SmartAIMgr: Creature %d Event %u Action %u has invalid "
                "SMART_ACTION_SET_HEALTH type in smart_scripts table",
                e.entryOrGuid, e.event_id, e.GetActionType());
            return false;
        }
        break;
    }
    case SMART_ACTION_SET_STAND_STATE:
    {
        if (e.action.standState.state >= MAX_UNIT_STAND_STATE)
        {
            logging.error(
                "SmartAIMgr: Creature %d Event %u Action %u has invalid "
                "SMART_ACTION_SET_STAND_STATE type in smart_scripts table",
                e.entryOrGuid, e.event_id, e.GetActionType());
            return false;
        }
        break;
    }
    case SMART_ACTION_BEHAVIORAL_CHANGE_BEHAVIOR:
    {
        if (e.action.behaviorChange.behavior >= AI_BEHAVIOR_MAX)
        {
            logging.error(
                "SmartAIMgr: Creature %d Event %u Action %u has invalid "
                "SMART_ACTION_BEHAVIORAL_CHANGE_BEHAVIOR behavior index",
                e.entryOrGuid, e.event_id, e.GetActionType());
            return false;
        }
        break;
    }
    case SMART_ACTION_DISENGAGE:
    {
        if (e.action.disengage.distance == 0)
        {
            logging.error(
                "SmartAIMgr: Creature %d Event %u Action %u has invalid "
                "SMART_ACTION_DISENGAGE type in smart_scripts table",
                e.entryOrGuid, e.event_id, e.GetActionType());
            return false;
        }
        break;
    }
    case SMART_ACTION_PUSH_POINT_GEN:
    {
        if (e.action.PointGen.interactDistance > 60)
        {
            logging.error(
                "SmartAIMgr: Creature %d Event %u Action %u has invalid "
                "SMART_ACTION_PUSH_POINT_GEN type in smart_scripts table "
                "(interactDistance cannot exceed 60)",
                e.entryOrGuid, e.event_id, e.GetActionType());
            return false;
        }
        break;
    }
    case SMART_ACTION_SET_RUN:
    {
        logging.warning(
            "SmartAIMgr: Creature %d Event %u Action %u uses "
            "SMART_ACTION_SET_RUN. This action is broken by design. Please use "
            "the run boolean on the movement generator actions. Fix ASAP!",
            e.entryOrGuid, e.event_id, e.GetActionType());
        break;
    }
    case SMART_ACTION_POP_MOVE_GENS:
    {
        if (e.action.raw.param1 >= (int)movement::gen::max)
        {
            logging.warning(
                "SmartAIMgr: Creature %d Event %u Action %u uses "
                "SMART_ACTION_POP_MOVE_GENS, however the specified id does not "
                "denote a valid movement generator id.",
                e.entryOrGuid, e.event_id, e.GetActionType());
        }
        break;
    }
    case SMART_ACTION_GROUP_MOVE_IN_FORMATION:
    {
        if (!sSmartGroupWaypointMgr::Instance()->GetPath(e.action.raw.param1))
        {
            logging.error(
                "SmartAIMgr: Creature %d Event %u Action %u uses non-existent "
                "Group Waypoint Path id %u, skipped.",
                e.entryOrGuid, e.event_id, e.GetActionType(),
                e.action.raw.param2);
            return false;
        }
        break;
    }
    case SMART_ACTION_PLAY_SPLINE:
    {
        if (!sWaypointMgr::Instance()->GetSpline(e.action.raw.param1))
        {
            logging.error(
                "SmartAIMgr: Creature %d Event %u Action %u uses non-existent "
                "spline id %u, skipped.",
                e.entryOrGuid, e.event_id, e.GetActionType(),
                e.action.raw.param1);
            return false;
        }
        break;
    }
    case SMART_ACTION_MOD_EVENT_TIMER:
    {
        if (!IsMinMaxValid(e, e.action.raw.param2, e.action.raw.param3))
        {
            logging.error(
                "SmartAIMgr: Creature %d Event %u Action %u invalid min-max ",
                e.entryOrGuid, e.event_id, e.GetActionType());
            return false;
        }
        break;
    }
    case SMART_ACTION_SET_BEHAVIORAL_AI_CD:
    {
        if (!IsMinMaxValid(e, e.action.raw.param3, e.action.raw.param4))
        {
            logging.error(
                "SmartAIMgr: Creature %d Event %u Action %u invalid min-max ",
                e.entryOrGuid, e.event_id, e.GetActionType());
            return false;
        }
        break;
    }
    case SMART_ACTION_FOLLOW:
    case SMART_ACTION_SET_ORIENTATION:
    case SMART_ACTION_STORE_TARGET_LIST:
    case SMART_ACTION_EVADE:
    case SMART_ACTION_FLEE_FOR_ASSIST:
    case SMART_ACTION_DIE:
    case SMART_ACTION_SET_IN_COMBAT_WITH_ZONE:
    case SMART_ACTION_SET_ACTIVE:
    case SMART_ACTION_STORE_VARIABLE_DECIMAL:
    case SMART_ACTION_WP_RESUME:
    case SMART_ACTION_KILL_UNIT:
    case SMART_ACTION_SET_INVINCIBILITY_HP_LEVEL:
    case SMART_ACTION_RESET_GOBJECT:
    case SMART_ACTION_ATTACK_START:
    case SMART_ACTION_THREAT_ALL_PCT:
    case SMART_ACTION_THREAT_SINGLE_PCT:
    case SMART_ACTION_SET_INST_DATA:
    case SMART_ACTION_SET_INST_DATA64:
    case SMART_ACTION_AUTO_ATTACK:
    case SMART_ACTION_ALLOW_COMBAT_MOVEMENT:
    case SMART_ACTION_CALL_FOR_HELP:
    case SMART_ACTION_SET_DATA:
    case SMART_ACTION_MOVE_FORWARD:
    case SMART_ACTION_SET_VISIBILITY:
    case SMART_ACTION_WP_PAUSE:
    case SMART_ACTION_SET_FLY:
    case SMART_ACTION_SET_SWIM:
    case SMART_ACTION_FORCE_DESPAWN:
    case SMART_ACTION_SET_INGAME_PHASE_MASK:
    case SMART_ACTION_SET_UNIT_FLAG:
    case SMART_ACTION_REMOVE_UNIT_FLAG:
    case SMART_ACTION_PLAYMOVIE:
    case SMART_ACTION_RESPAWN_TARGET:
    case SMART_ACTION_CLOSE_GOSSIP:
    case SMART_ACTION_EQUIP:
    case SMART_ACTION_TRIGGER_TIMED_EVENT:
    case SMART_ACTION_REMOVE_TIMED_EVENT:
    case SMART_ACTION_ACTIVATE_GOBJECT:
    case SMART_ACTION_CALL_SCRIPT_RESET:
    case SMART_ACTION_NONE:
    case SMART_ACTION_CALL_TIMED_ACTIONLIST:
    case SMART_ACTION_SET_NPC_FLAG:
    case SMART_ACTION_ADD_NPC_FLAG:
    case SMART_ACTION_REMOVE_NPC_FLAG:
    case SMART_ACTION_TALK:
    case SMART_ACTION_TALK2:
    case SMART_ACTION_SIMPLE_TALK:
    case SMART_ACTION_KILL_SAY:
    case SMART_ACTION_CROSS_CAST:
    case SMART_ACTION_CALL_RANDOM_TIMED_ACTIONLIST:
    case SMART_ACTION_CALL_RANDOM_RANGE_TIMED_ACTIONLIST:
    case SMART_ACTION_PUSH_RANDOM_GEN:
    case SMART_ACTION_SET_UNIT_FIELD_BYTES_1:
    case SMART_ACTION_REMOVE_UNIT_FIELD_BYTES_1:
    case SMART_ACTION_INTERRUPT_SPELL:
    case SMART_ACTION_SEND_GO_CUSTOM_ANIM:
    case SMART_ACTION_SET_DYNAMIC_FLAG:
    case SMART_ACTION_ADD_DYNAMIC_FLAG:
    case SMART_ACTION_REMOVE_DYNAMIC_FLAG:
    case SMART_ACTION_SEND_GOSSIP_MENU:
    case SMART_ACTION_GO_SET_LOOT_STATE:
    case SMART_ACTION_SEND_TARGET_TO_TARGET:
    case SMART_ACTION_SET_HEALTH_REGEN:
    case SMART_ACTION_NOTIFY_AI:
    case SMART_ACTION_SET_AGGRO_DISTANCE:
    case SMART_ACTION_SWITCH_TARGET:
    case SMART_ACTION_BEHAVIORAL_TOGGLE:
    case SMART_ACTION_SPAR:
    case SMART_ACTION_DISABLE_COMBAT_REACTIONS:
    case SMART_ACTION_SET_FOCUS_TARGET:
    case SMART_ACTION_SAVE_CURRENT_PHASE:
    case SMART_ACTION_LOAD_SAVED_PHASE:
    case SMART_ACTION_TELEPORT:
    case SMART_ACTION_WP_SET_RUN:
    case SMART_ACTION_PAUSE_GROUP_MOVEMENT:
    case SMART_ACTION_TOGGLE_PET_BEHAVIOR:
    case SMART_ACTION_TEMPSUMMON_LEASH:
    case SMART_ACTION_SAVE_POS:
    case SMART_ACTION_PUSH_STOPPED_GEN:
    case SMART_ACTION_PUSH_IDLE_GEN:
    case SMART_ACTION_PUSH_FALL_GEN:
    case SMART_ACTION_GROUP_CREATE:
    case SMART_ACTION_GROUP_INVITE:
    case SMART_ACTION_GROUP_LEAVE:
    case SMART_ACTION_GROUP_DISBAND:
    case SMART_ACTION_GROUP_ABANDON_FORMATION:
    case SMART_ACTION_GROUP_ADD_FLAG:
    case SMART_ACTION_GROUP_REMOVE_FLAG:
    case SMART_ACTION_PACIFY:
    case SMART_ACTION_RESET_LOOT_RECIPIENTS:
    case SMART_ACTION_FWD_INVOKER:
        break;
    default:
        logging.error(
            "SmartAIMgr: Not handled action_type(%u), event_type(%u), Entry %d "
            "SourceType %u Event %u, skipped.",
            e.GetActionType(), e.GetEventType(), e.entryOrGuid,
            e.GetScriptType(), e.event_id);
        return false;
    }

    return true;
}

/*bool SmartAIMgr::IsTextValid(SmartScriptHolder const& e, uint32 id) // unused
{
    bool error = false;
    uint32 entry = 0;
    if (e.entryOrGuid >= 0)
        entry = uint32(e.entryOrGuid);
    else {
        entry = uint32(abs(e.entryOrGuid));
        CreatureData const* data =
sObjectMgr::Instance()->GetCreatureData(entry);
        if (!data)
        {
            logging.error("SmartAIMgr: Entry %d SourceType %u
Event %u Action %u using non-existent Creature guid %d, skipped.",
e.entryOrGuid, e.GetScriptType(), e.event_id, e.GetActionType(), entry);
            return false;
        }
        else
            entry = data->id;
    }
    if (!entry || !sCreatureTextMgr::Instance()->TextExist(entry, uint8(id)))
        error = true;
    if (error)
    {
        logging.error("SmartAIMgr: Entry %d SourceType %u Event
%u Action %u using non-existent Text id %d, skipped.", e.entryOrGuid,
e.GetScriptType(), e.source_type, e.GetActionType(), id);
        return false;
    }
    return true;
}*/
