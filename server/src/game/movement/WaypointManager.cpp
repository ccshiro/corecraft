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

#include "WaypointManager.h"
#include "MapManager.h"
#include "ObjectMgr.h"
#include "ProgressBar.h"
#include "ScriptMgr.h"
#include "Database/DatabaseEnv.h"
#include "maps/map_grid.h"
#include "Policies/Singleton.h"

namespace movement
{
bool WaypointBehavior::isEmpty()
{
    if (emote || spell || model1 || model2)
        return false;

    for (auto& elem : textid)
        if (elem)
            return false;

    return true;
}

WaypointBehavior::WaypointBehavior(const WaypointBehavior& b)
{
    emote = b.emote;
    spell = b.spell;
    model1 = b.model1;
    model2 = b.model2;
    for (int i = 0; i < MAX_WAYPOINT_TEXT; ++i)
        textid[i] = b.textid[i];
}

void WaypointManager::Load()
{
    Cleanup();

    uint32 total_paths = 0;
    uint32 total_nodes = 0;
    uint32 total_behaviors = 0;

    std::set<uint32> movementScriptSet;

    for (ScriptMapMap::const_iterator itr =
             sCreatureMovementScripts.second.begin();
         itr != sCreatureMovementScripts.second.end(); ++itr)
        movementScriptSet.insert(itr->first);

    // creature_movement
    QueryResult* result = WorldDatabase.Query(
        "SELECT id, COUNT(point) FROM creature_movement GROUP BY id");

    if (result)
    {
        total_paths = (uint32)result->GetRowCount();
        BarGoLink bar(total_paths);

        do
        {
            bar.step();
            Field* fields = result->Fetch();

            uint32 id = fields[0].GetUInt32();
            uint32 count = fields[1].GetUInt32();

            m_pathMap[id].resize(count);
            total_nodes += count;
        } while (result->NextRow());

        logging.info("Paths loaded");

        delete result;

        //                                   0   1      2           3
        //                                   4           5         6
        result = WorldDatabase.Query(
            "SELECT id, point, position_x, position_y, position_z, waittime, "
            "script_id,"
            //   7        8        9        10       11       12     13     14
            //   15      16      17
            "textid1, textid2, textid3, textid4, textid5, emote, spell, "
            "orientation, model1, model2, run FROM creature_movement");

        BarGoLink barRow((int)result->GetRowCount());

        // error after load, we check if creature guid corresponding to the path
        // id has proper MovementType
        std::set<uint32> creatureNoMoveType;

        do
        {
            barRow.step();
            Field* fields = result->Fetch();
            uint32 id = fields[0].GetUInt32();
            uint32 point = fields[1].GetUInt32();

            const CreatureData* cData =
                sObjectMgr::Instance()->GetCreatureData(id);

            if (!cData)
            {
                logging.error(
                    "Table creature_movement contain path for creature guid "
                    "%u, but this creature guid does not exist. Skipping.",
                    id);
                continue;
            }

            if (cData->movementType != (int)movement::gen::waypoint)
                creatureNoMoveType.insert(id);

            WaypointPath& path = m_pathMap[id];

            // the cleanup queries make sure the following is true
            assert(point >= 1 && point <= path.size());

            WaypointNode& node = path[point - 1];

            node.x = fields[2].GetFloat();
            node.y = fields[3].GetFloat();
            node.z = fields[4].GetFloat();
            node.orientation = fields[14].GetFloat();
            node.delay = fields[5].GetUInt32();
            node.run = fields[17].GetBool();
            node.script_id = fields[6].GetUInt32();

            // prevent using invalid coordinates
            if (!maps::verify_coords(node.x, node.y))
            {
                std::unique_ptr<QueryResult> result1(WorldDatabase.PQuery(
                    "SELECT id, map FROM creature WHERE guid = '%u'", id));
                if (result1)
                    logging.error(
                        "Creature (guidlow %d, entry %d) have invalid "
                        "coordinates in his waypoint %d (X: %f, Y: %f).",
                        id, result1->Fetch()[0].GetUInt32(), point, node.x,
                        node.y);
                else
                    logging.error(
                        "Waypoint path %d, have invalid coordinates in his "
                        "waypoint %d (X: %f, Y: %f).",
                        id, point, node.x, node.y);
                continue;
            }

            if (node.script_id)
            {
                if (sCreatureMovementScripts.second.find(node.script_id) ==
                    sCreatureMovementScripts.second.end())
                {
                    logging.error(
                        "Table creature_movement for id %u, point %u have "
                        "script_id %u that does not exist in "
                        "`creature_movement_scripts`, ignoring",
                        id, point, node.script_id);
                    continue;
                }

                movementScriptSet.erase(node.script_id);
            }

            // WaypointBehavior can be dropped in time. Script_id added may 2010
            // and can handle all the below behavior.

            WaypointBehavior be;
            be.model1 = fields[15].GetUInt32();
            be.model2 = fields[16].GetUInt32();
            be.emote = fields[12].GetUInt32();
            be.spell = fields[13].GetUInt32();

            for (int i = 0; i < MAX_WAYPOINT_TEXT; ++i)
            {
                be.textid[i] = fields[7 + i].GetUInt32();

                if (be.textid[i])
                {
                    if (be.textid[i] < MIN_DB_SCRIPT_STRING_ID ||
                        be.textid[i] >= MAX_DB_SCRIPT_STRING_ID)
                    {
                        logging.error(
                            "Table `db_script_string` not have string id  %u",
                            be.textid[i]);
                        continue;
                    }
                }
            }

            if (be.spell && !sSpellStore.LookupEntry(be.spell))
            {
                logging.error(
                    "Table creature_movement references unknown spellid %u. "
                    "Skipping id %u with point %u.",
                    be.spell, id, point);
                be.spell = 0;
            }

            if (be.emote)
            {
                if (!sEmotesStore.LookupEntry(be.emote))
                    logging.error(
                        "Waypoint path %u (Point %u) are using emote %u, but "
                        "emote does not exist.",
                        id, point, be.emote);
            }

            // save memory by not storing empty behaviors
            if (!be.isEmpty())
            {
                node.behavior = new WaypointBehavior(be);
                ++total_behaviors;
            }
            else
                node.behavior = nullptr;
        } while (result->NextRow());

        if (!creatureNoMoveType.empty())
        {
            for (const auto& elem : creatureNoMoveType)
            {
                const CreatureData* cData =
                    sObjectMgr::Instance()->GetCreatureData(elem);
                const CreatureInfo* cInfo =
                    ObjectMgr::GetCreatureTemplate(cData->id);

                logging.error(
                    "Table creature_movement has waypoint for creature guid %u "
                    "(entry %u), but MovementType is not "
                    "WAYPOINT_MOTION_TYPE(2). Creature will not use this path.",
                    elem, cData->id);

                if (cInfo->MovementType == (int)movement::gen::waypoint)
                    logging.error(
                        "    creature_template for this entry has MovementType "
                        "WAYPOINT_MOTION_TYPE(2), did you intend to use "
                        "creature_movement_template ?");
            }
        }
        delete result;
    }
    logging.info(
        "Loaded Waypoints. In total: %u paths, %u nodes and %u behaviors",
        total_paths, total_nodes, total_behaviors);

    // creature_movement_template
    result = WorldDatabase.Query(
        "SELECT entry, COUNT(point) FROM creature_movement_template GROUP BY "
        "entry");

    if (result)
    {
        total_nodes = 0;
        total_behaviors = 0;
        total_paths = (uint32)result->GetRowCount();
        BarGoLink barRow(total_paths);

        do
        {
            barRow.step();
            Field* fields = result->Fetch();

            uint32 entry = fields[0].GetUInt32();
            uint32 count = fields[1].GetUInt32();

            m_pathTemplateMap[entry].resize(count);
            total_nodes += count;
        } while (result->NextRow());

        delete result;

        logging.info("Path templates loaded");

        //                                   0      1      2           3
        //                                   4           5         6
        result = WorldDatabase.Query(
            "SELECT entry, point, position_x, position_y, position_z, "
            "waittime, script_id,"
            //   7        8        9        10       11       12     13     14
            //   15      16      17
            "textid1, textid2, textid3, textid4, textid5, emote, spell, "
            "orientation, model1, model2, run FROM creature_movement_template");

        BarGoLink bar(result->GetRowCount());

        do
        {
            bar.step();
            Field* fields = result->Fetch();

            uint32 entry = fields[0].GetUInt32();
            uint32 point = fields[1].GetUInt32();

            const CreatureInfo* cInfo = ObjectMgr::GetCreatureTemplate(entry);

            if (!cInfo)
            {
                logging.error(
                    "Table creature_movement_template references unknown "
                    "creature template %u. Skipping.",
                    entry);
                continue;
            }

            WaypointPath& path = m_pathTemplateMap[entry];

            // the cleanup queries make sure the following is true
            assert(point >= 1 && point <= path.size());

            WaypointNode& node = path[point - 1];

            node.x = fields[2].GetFloat();
            node.y = fields[3].GetFloat();
            node.z = fields[4].GetFloat();
            node.orientation = fields[14].GetFloat();
            node.delay = fields[5].GetUInt32();
            node.run = fields[17].GetBool();
            node.script_id = fields[6].GetUInt32();

            // prevent using invalid coordinates
            if (!maps::verify_coords(node.x, node.y))
            {
                logging.error(
                    "Table creature_movement_template for entry %u (point %u) "
                    "are using invalid coordinates position_x: %f, position_y: "
                    "%f)",
                    entry, point, node.x, node.y);
                continue;
            }

            if (node.script_id)
            {
                if (sCreatureMovementScripts.second.find(node.script_id) ==
                    sCreatureMovementScripts.second.end())
                {
                    logging.error(
                        "Table creature_movement_template for entry %u, point "
                        "%u have script_id %u that does not exist in "
                        "`creature_movement_scripts`, ignoring",
                        entry, point, node.script_id);
                    continue;
                }

                movementScriptSet.erase(node.script_id);
            }

            WaypointBehavior be;
            be.model1 = fields[15].GetUInt32();
            be.model2 = fields[16].GetUInt32();
            be.emote = fields[12].GetUInt32();
            be.spell = fields[13].GetUInt32();

            for (int i = 0; i < MAX_WAYPOINT_TEXT; ++i)
            {
                be.textid[i] = fields[7 + i].GetUInt32();

                if (be.textid[i])
                {
                    if (be.textid[i] < MIN_DB_SCRIPT_STRING_ID ||
                        be.textid[i] >= MAX_DB_SCRIPT_STRING_ID)
                    {
                        logging.error(
                            "Table `db_script_string` not have string id %u",
                            be.textid[i]);
                        continue;
                    }
                }
            }

            if (be.spell && !sSpellStore.LookupEntry(be.spell))
            {
                logging.error(
                    "Table creature_movement_template references unknown "
                    "spellid %u. Skipping id %u with point %u.",
                    be.spell, entry, point);
                be.spell = 0;
            }

            if (be.emote)
            {
                if (!sEmotesStore.LookupEntry(be.emote))
                    logging.error(
                        "Waypoint template path %u (point %u) are using emote "
                        "%u, but emote does not exist.",
                        entry, point, be.emote);
            }

            // save memory by not storing empty behaviors
            if (!be.isEmpty())
            {
                node.behavior = new WaypointBehavior(be);
                ++total_behaviors;
            }
            else
                node.behavior = nullptr;
        } while (result->NextRow());

        delete result;
    }
    logging.info(
        "Loaded Waypoint Templates. In total: %u paths, %u nodes and %u "
        "behaviors\n",
        total_paths, total_nodes, total_behaviors);

    if (!movementScriptSet.empty())
    {
        for (const auto& elem : movementScriptSet)
            logging.error(
                "Table `creature_movement_scripts` contain unused script, id "
                "%u.",
                elem);
    }

    LoadSplines();
}

void WaypointManager::Cleanup()
{
    // check if points need to be renumbered and do it
    if (QueryResult* result = WorldDatabase.Query(
            "SELECT 1 from creature_movement As T WHERE point <> (SELECT "
            "COUNT(*) FROM creature_movement WHERE id = T.id AND point <= "
            "T.point) LIMIT 1"))
    {
        delete result;
        WorldDatabase.DirectExecute(
            "CREATE TEMPORARY TABLE temp LIKE creature_movement");
        WorldDatabase.DirectExecute(
            "INSERT INTO temp SELECT * FROM creature_movement");
        WorldDatabase.DirectExecute(
            "ALTER TABLE creature_movement DROP PRIMARY KEY");
        WorldDatabase.DirectExecute(
            "UPDATE creature_movement AS T SET point = (SELECT COUNT(*) FROM "
            "temp WHERE id = T.id AND point <= T.point)");
        WorldDatabase.DirectExecute(
            "ALTER TABLE creature_movement ADD PRIMARY KEY (id, point)");
        WorldDatabase.DirectExecute("DROP TABLE temp");

        logging.error(
            "Table `creature_movement` was auto corrected for using points out "
            "of order (invalid or points missing)");

        assert(!(result = WorldDatabase.Query(
                     "SELECT 1 from creature_movement As T WHERE point <> "
                     "(SELECT COUNT(*) FROM creature_movement WHERE id = T.id "
                     "AND point <= T.point) LIMIT 1")));
    }

    if (QueryResult* result = WorldDatabase.Query(
            "SELECT 1 from creature_movement_template As T WHERE point <> "
            "(SELECT COUNT(*) FROM creature_movement_template WHERE entry = "
            "T.entry AND point <= T.point) LIMIT 1"))
    {
        delete result;
        WorldDatabase.DirectExecute(
            "CREATE TEMPORARY TABLE temp LIKE creature_movement_template");
        WorldDatabase.DirectExecute(
            "INSERT INTO temp SELECT * FROM creature_movement_template");
        WorldDatabase.DirectExecute(
            "ALTER TABLE creature_movement_template DROP PRIMARY KEY");
        WorldDatabase.DirectExecute(
            "UPDATE creature_movement_template AS T SET point = (SELECT "
            "COUNT(*) FROM temp WHERE entry = T.entry AND point <= T.point)");
        WorldDatabase.DirectExecute(
            "ALTER TABLE creature_movement_template ADD PRIMARY KEY (entry, "
            "point)");
        WorldDatabase.DirectExecute("DROP TABLE temp");

        logging.error(
            "Table `creature_movement_template` was auto corrected for using "
            "points out of order (invalid or points missing)");

        assert(
            !(result = WorldDatabase.Query(
                  "SELECT 1 from creature_movement_template As T WHERE point "
                  "<> (SELECT COUNT(*) FROM creature_movement_template WHERE "
                  "entry = T.entry AND point <= T.point) LIMIT 1")));
    }
}

void WaypointManager::Unload()
{
    for (auto& elem : m_pathMap)
        _clearPath(elem.second);
    m_pathMap.clear();

    for (auto& elem : m_pathTemplateMap)
        _clearPath(elem.second);
    m_pathTemplateMap.clear();
}

void WaypointManager::_clearPath(WaypointPath& path)
{
    for (WaypointPath::const_iterator itr = path.begin(); itr != path.end();
         ++itr)
        if (itr->behavior)
            delete itr->behavior;
    path.clear();
}

/// - Insert after the last point
void WaypointManager::AddLastNode(
    uint32 id, float x, float y, float z, float o, uint32 delay, uint32 wpGuid)
{
    _addNode(id, GetLastPoint(id, 0) + 1, x, y, z, o, delay, wpGuid);
}

/// - Insert after a certain point
void WaypointManager::AddAfterNode(uint32 id, uint32 point, float x, float y,
    float z, float o, uint32 delay, uint32 wpGuid)
{
    for (uint32 i = GetLastPoint(id, 0); i > point; i--)
        WorldDatabase.PExecuteLog(
            "UPDATE creature_movement SET point=point+1 WHERE id=%u AND "
            "point=%u",
            id, i);

    _addNode(id, point + 1, x, y, z, o, delay, wpGuid);
}

/// - Insert without checking for collision
void WaypointManager::_addNode(uint32 id, uint32 point, float x, float y,
    float z, float o, uint32 delay, uint32 wpGuid)
{
    if (point == 0)
        return; // counted from 1 in the DB
    WorldDatabase.PExecuteLog(
        "INSERT INTO creature_movement "
        "(id,point,position_x,position_y,position_z,orientation,wpguid,"
        "waittime) "
        "VALUES (%u,%u, %f,%f,%f,%f, %u,%u)",
        id, point, x, y, z, o, wpGuid, delay);
    auto itr = m_pathMap.find(id);
    if (itr == m_pathMap.end())
        itr = m_pathMap.insert(WaypointPathMap::value_type(id, WaypointPath()))
                  .first;
    itr->second.insert(itr->second.begin() + (point - 1),
        WaypointNode(x, y, z, o, delay, 0, nullptr));
}

uint32 WaypointManager::GetLastPoint(uint32 id, uint32 default_notfound)
{
    uint32 point = default_notfound;
    /*QueryResult *result = WorldDatabase.PQuery( "SELECT MAX(point) FROM
    creature_movement WHERE id = '%u'", id);
    if( result )
    {
        point = (*result)[0].GetUInt32()+1;
        delete result;
    }*/
    WaypointPathMap::const_iterator itr = m_pathMap.find(id);
    if (itr != m_pathMap.end() && itr->second.size() != 0)
        point = itr->second.size();
    return point;
}

void WaypointManager::DeleteNode(uint32 id, uint32 point)
{
    if (point == 0)
        return; // counted from 1 in the DB
    WorldDatabase.PExecuteLog(
        "DELETE FROM creature_movement WHERE id=%u AND point=%u", id, point);
    WorldDatabase.PExecuteLog(
        "UPDATE creature_movement SET point=point-1 WHERE id=%u AND point>%u",
        id, point);
    auto itr = m_pathMap.find(id);
    if (itr != m_pathMap.end() && point <= itr->second.size())
        itr->second.erase(itr->second.begin() + (point - 1));
}

void WaypointManager::DeletePath(uint32 id)
{
    WorldDatabase.PExecuteLog("DELETE FROM creature_movement WHERE id=%u", id);
    auto itr = m_pathMap.find(id);
    if (itr != m_pathMap.end())
        _clearPath(itr->second);
    // the path is not removed from the map, just cleared
    // WMGs have pointers to the path, so deleting them would crash
    // this wastes some memory, but these functions are
    // only meant to be called by GM commands
}

void WaypointManager::SetNodePosition(
    uint32 id, uint32 point, float x, float y, float z)
{
    if (point == 0)
        return; // counted from 1 in the DB
    WorldDatabase.PExecuteLog(
        "UPDATE creature_movement SET position_x=%f, position_y=%f, "
        "position_z=%f WHERE id=%u AND point=%u",
        x, y, z, id, point);
    auto itr = m_pathMap.find(id);
    if (itr != m_pathMap.end() && point <= itr->second.size())
    {
        itr->second[point - 1].x = x;
        itr->second[point - 1].y = y;
        itr->second[point - 1].z = z;
    }
}

void WaypointManager::SetNodeText(
    uint32 id, uint32 point, const char* text_field, const char* text)
{
    if (point == 0)
        return; // counted from 1 in the DB
    if (!text_field)
        return;
    std::string field = text_field;
    WorldDatabase.escape_string(field);

    if (!text)
    {
        WorldDatabase.PExecuteLog(
            "UPDATE creature_movement SET %s=NULL WHERE id='%u' AND point='%u'",
            field.c_str(), id, point);
    }
    else
    {
        std::string text2 = text;
        WorldDatabase.escape_string(text2);
        WorldDatabase.PExecuteLog(
            "UPDATE creature_movement SET %s='%s' WHERE id='%u' AND point='%u'",
            field.c_str(), text2.c_str(), id, point);
    }

    auto itr = m_pathMap.find(id);
    if (itr != m_pathMap.end() && point <= itr->second.size())
    {
        WaypointNode& node = itr->second[point - 1];
        if (!node.behavior)
            node.behavior = new WaypointBehavior();

        //        if(field == "text1") node.behavior->text[0] = text ? text :
        //        "";
        //        if(field == "text2") node.behavior->text[1] = text ? text :
        //        "";
        //        if(field == "text3") node.behavior->text[2] = text ? text :
        //        "";
        //        if(field == "text4") node.behavior->text[3] = text ? text :
        //        "";
        //        if(field == "text5") node.behavior->text[4] = text ? text :
        //        "";
        if (field == "emote")
            node.behavior->emote = text ? atoi(text) : 0;
        if (field == "spell")
            node.behavior->spell = text ? atoi(text) : 0;
        if (field == "model1")
            node.behavior->model1 = text ? atoi(text) : 0;
        if (field == "model2")
            node.behavior->model2 = text ? atoi(text) : 0;
        if (field == "run")
            node.run = text ? atoi(text) : 0;
    }
}

void WaypointManager::CheckTextsExistance(std::set<int32>& ids)
{
    WaypointPathMap::const_iterator pmItr = m_pathMap.begin();
    for (; pmItr != m_pathMap.end(); ++pmItr)
    {
        for (auto& elem : pmItr->second)
        {
            WaypointBehavior* be = elem.behavior;
            if (!be)
                continue;

            // Now we check text existence and put all zero texts ids to the end
            // of array

            // Counting leading zeros for futher textid shift
            int zeroCount = 0;
            for (int j = 0; j < MAX_WAYPOINT_TEXT; ++j)
            {
                if (!be->textid[j])
                {
                    ++zeroCount;
                    continue;
                }
                else
                {
                    if (!sObjectMgr::Instance()->GetMangosStringLocale(
                            be->textid[j]))
                    {
                        logging.error(
                            "Some waypoint has textid%u with not existing %u "
                            "text.",
                            j, be->textid[j]);
                        be->textid[j] = 0;
                        ++zeroCount;
                        continue;
                    }
                    else
                        ids.erase(be->textid[j]);

                    // Shifting check
                    if (zeroCount)
                    {
                        // Correct textid but some zeros leading, so move it
                        // forward.
                        be->textid[j - zeroCount] = be->textid[j];
                        be->textid[j] = 0;
                    }
                }
            }
        }
    }

    WaypointPathTemplateMap::const_iterator wptItr = m_pathTemplateMap.begin();
    for (; wptItr != m_pathTemplateMap.end(); ++wptItr)
    {
        for (auto& elem : wptItr->second)
        {
            WaypointBehavior* be = elem.behavior;
            if (!be)
                continue;

            // Now we check text existence and put all zero texts ids to the end
            // of array

            // Counting leading zeros for futher textid shift
            int zeroCount = 0;
            for (int j = 0; j < MAX_WAYPOINT_TEXT; ++j)
            {
                if (!be->textid[j])
                {
                    ++zeroCount;
                    continue;
                }
                else
                {
                    if (!sObjectMgr::Instance()->GetMangosStringLocale(
                            be->textid[j]))
                    {
                        logging.error(
                            "Some waypoint has textid%u with not existing %u "
                            "text.",
                            j, be->textid[j]);
                        be->textid[j] = 0;
                        ++zeroCount;
                        continue;
                    }
                    else
                        ids.erase(be->textid[j]);

                    // Shifting check
                    if (zeroCount)
                    {
                        // Correct textid but some zeros leading, so move it
                        // forward.
                        be->textid[j - zeroCount] = be->textid[j];
                        be->textid[j] = 0;
                    }
                }
            }
        }
    }
}

void WaypointManager::LoadSplines()
{
    // Reload not possible
    if (!splines_.empty())
        return;

    std::unique_ptr<QueryResult> result(WorldDatabase.PQuery(
        "SELECT id, point, x, y, z FROM splines ORDER BY id, point ASC"));
    if (!result)
    {
        logging.info("Loaded splines, table is empty!\n");
        return;
    }

    BarGoLink bar(result->GetRowCount());
    int count = 0;

    uint32 current_id = 0;
    uint32 next_point = 0;
    int skip_id = -1;
    std::vector<G3D::Vector3> spline;
    do
    {
        bar.step();

        G3D::Vector3 coords;
        uint32 id, point;

        Field* fields = result->Fetch();
        id = fields[0].GetUInt32();
        point = fields[1].GetUInt32();
        coords.x = fields[2].GetFloat();
        coords.y = fields[3].GetFloat();
        coords.z = fields[4].GetFloat();

        // Skip all points of broken spline
        if (skip_id > 0 && (uint32)skip_id == id)
            continue;

        if (!maps::verify_coords(coords.x, coords.y))
        {
            logging.error(
                "Table `splines`: Invalid coordinates for splines.id=%u", id);
            skip_id = (int)id;
            continue;
        }

        if (id == 0)
        {
            logging.error("Table `splines`: 0 is an invalid spline id");
            skip_id = (int)id;
            continue;
        }

        if (current_id != id)
        {
            if (!spline.empty() && skip_id == -1)
                splines_[current_id] = std::move(spline);
            spline.clear();

            current_id = id;
            next_point = 0;
            skip_id = -1;
        }

        if (next_point != point)
        {
            logging.error(
                "Table `splines`: Points in spline not consecutive for "
                "splines.id=%u",
                id);
            skip_id = (int)id;
            continue;
        }
        ++next_point;

        ++count;
        spline.push_back(coords);
        // Duplicate first point of each spline
        // TODO: How to fix this?
        if (point == 0)
            spline.push_back(coords);

    } while (result->NextRow());

    if (!spline.empty() && skip_id == -1)
        splines_[current_id] = std::move(spline);

    logging.info("Loaded splines, %d splines made up of %d points!\n",
        (uint32)splines_.size(), count);
}
}
