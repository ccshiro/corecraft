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

#include "SQLStorages.h"
#include "Database/DatabaseEnv.h"
#include "Database/SQLStorage.h"
#include "Database/SQLStorageImpl.h"

const char CreatureInfosrcfmt[] =
    "iiiiiiiisssiiiiiiiiiiifffiffiifiiiiiiiiiiffiiiiiiiiiiiiiiiiiiisiiffllliiii"
    "iiffs";
const char CreatureInfodstfmt[] =
    "iiiiiiiisssiiiiiiiiiiifffiffiifiiiiiiiiiiffiiiiiiiiiiiiiiiiiiisiiffllliiii"
    "iiffi";
const char CreatureDataAddonInfofmt[] = "iiibbiiss";
const char CreatureModelfmt[] = "ifffbii";
const char CreatureInfoAddonInfofmt[] = "iiibbiiss";
const char EquipmentInfofmt[] = "iiii";
const char EquipmentInfoRawfmt[] = "iiiiiiiiii";
const char GameObjectInfosrcfmt[] = "iiisssiifiiiiiiiiiiiiiiiiiiiiiiiiiiiiss";
const char GameObjectInfodstfmt[] = "iiisssiifiiiiiiiiiiiiiiiiiiiiiiiiiiiisi";
const char ItemPrototypesrcfmt[] =
    "iiiisiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiffiffiffiffiffiiiiiiiiiifiii"
    "fiiiiiifiiiiiifiiiiiifiiiiiifiiiisiiiiiiiiiiiiiiiiiiiiiiiiifsiiiiiii";
const char ItemPrototypedstfmt[] =
    "iiiisiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiffiffiffiffiffiiiiiiiiiifiii"
    "fiiiiiifiiiiiifiiiiiifiiiiiifiiiisiiiiiiiiiiiiiiiiiiiiiiiiifiiiiiiii";
const char PageTextfmt[] = "isi";
const char InstanceTemplatesrcfmt[] = "iiiiiis";
const char InstanceTemplatedstfmt[] = "iiiiiii";
const char WorldTemplatesrcfmt[] = "is";
const char WorldTemplatedstfmt[] = "ii";
const char ConditionsSrcFmt[] = "iiii";
const char ConditionsDstFmt[] = "iiii";

SQLStorage sCreatureStorage(
    CreatureInfosrcfmt, CreatureInfodstfmt, "entry", "creature_template");
SQLStorage sCreatureDataAddonStorage(
    CreatureDataAddonInfofmt, "guid", "creature_addon");
SQLStorage sCreatureModelStorage(
    CreatureModelfmt, "modelid", "creature_model_info");
SQLStorage sCreatureInfoAddonStorage(
    CreatureInfoAddonInfofmt, "entry", "creature_template_addon");
SQLStorage sEquipmentStorage(
    EquipmentInfofmt, "entry", "creature_equip_template");
SQLStorage sEquipmentStorageRaw(
    EquipmentInfoRawfmt, "entry", "creature_equip_template_raw");
SQLStorage sGOStorage(
    GameObjectInfosrcfmt, GameObjectInfodstfmt, "entry", "gameobject_template");
SQLStorage sItemStorage(
    ItemPrototypesrcfmt, ItemPrototypedstfmt, "entry", "item_template");
SQLStorage sPageTextStore(PageTextfmt, "entry", "page_text");
SQLStorage sInstanceTemplate(
    InstanceTemplatesrcfmt, InstanceTemplatedstfmt, "map", "instance_template");
SQLStorage sWorldTemplate(
    WorldTemplatesrcfmt, WorldTemplatedstfmt, "map", "world_template");
SQLStorage sConditionStorage(
    ConditionsSrcFmt, ConditionsDstFmt, "condition_entry", "conditions");
