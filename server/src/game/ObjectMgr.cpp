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

#include "ObjectMgr.h"
#include "AccountMgr.h"
#include "ArenaTeam.h"
#include "Chat.h"
#include "GameEventMgr.h"
#include "GossipDef.h"
#include "Group.h"
#include "InstanceData.h"
#include "Language.h"
#include "logging.h"
#include "Mail.h"
#include "MapManager.h"
#include "MapPersistentStateMgr.h"
#include "ObjectGuid.h"
#include "PoolManager.h"
#include "ProgressBar.h"
#include "SQLStorages.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "Transport.h"
#include "UpdateMask.h"
#include "Util.h"
#include "World.h"
#include "Database/DatabaseEnv.h"
#include "Database/SQLStorageImpl.h"
#include "movement/WaypointManager.h"
#include "Policies/Singleton.h"
#include <limits>

bool normalizePlayerName(std::string& name)
{
    if (name.empty())
        return false;

    wchar_t wstr_buf[MAX_INTERNAL_PLAYER_NAME + 1];
    size_t wstr_len = MAX_INTERNAL_PLAYER_NAME;

    if (!Utf8toWStr(name, &wstr_buf[0], wstr_len))
        return false;

    wstr_buf[0] = wcharToUpper(wstr_buf[0]);
    for (size_t i = 1; i < wstr_len; ++i)
        wstr_buf[i] = wcharToLower(wstr_buf[i]);

    if (!WStrToUtf8(wstr_buf, wstr_len, name))
        return false;

    return true;
}

LanguageDesc lang_description[LANGUAGES_COUNT] = {{LANG_ADDON, 0, 0},
    {LANG_UNIVERSAL, 0, 0}, {LANG_ORCISH, 669, SKILL_LANG_ORCISH},
    {LANG_DARNASSIAN, 671, SKILL_LANG_DARNASSIAN},
    {LANG_TAURAHE, 670, SKILL_LANG_TAURAHE},
    {LANG_DWARVISH, 672, SKILL_LANG_DWARVEN},
    {LANG_COMMON, 668, SKILL_LANG_COMMON},
    {LANG_DEMONIC, 815, SKILL_LANG_DEMON_TONGUE},
    {LANG_TITAN, 816, SKILL_LANG_TITAN},
    {LANG_THALASSIAN, 813, SKILL_LANG_THALASSIAN},
    {LANG_DRACONIC, 814, SKILL_LANG_DRACONIC},
    {LANG_KALIMAG, 817, SKILL_LANG_OLD_TONGUE},
    {LANG_GNOMISH, 7340, SKILL_LANG_GNOMISH},
    {LANG_TROLL, 7341, SKILL_LANG_TROLL},
    {LANG_GUTTERSPEAK, 17737, SKILL_LANG_GUTTERSPEAK},
    {LANG_DRAENEI, 29932, SKILL_LANG_DRAENEI}, {LANG_ZOMBIE, 0, 0},
    {LANG_GNOMISH_BINARY, 0, 0}, {LANG_GOBLIN_BINARY, 0, 0}};

LanguageDesc const* GetLanguageDescByID(uint32 lang)
{
    for (auto& elem : lang_description)
    {
        if (uint32(elem.lang_id) == lang)
            return &elem;
    }

    return nullptr;
}

template <typename T>
T IdGenerator<T>::Generate()
{
    if (m_nextGuid >= std::numeric_limits<T>::max() - 1)
    {
        logging.error(
            "%s guid overflow!! Can't continue, shutting down server. ",
            m_name);
        World::StopNow(ERROR_EXIT_CODE);
    }
    return m_nextGuid++;
}

template uint32 IdGenerator<uint32>::Generate();
template uint64 IdGenerator<uint64>::Generate();

ObjectMgr::ObjectMgr()
  : m_ArenaTeamIds("Arena team ids"), m_AuctionIds("Auction ids"),
    m_GuildIds("Guild ids"), m_ItemTextIds("Item text ids"),
    m_MailIds("Mail ids"), m_PetNumbers("Pet numbers"),
    m_FirstTemporaryCreatureGuid(1), m_FirstTemporaryGameObjectGuid(1)
{
    // Only zero condition left, others will be added while loading DB tables
    mConditions.resize(1);

    static_creatures_.set_empty_key(0xFFFFFFFFFFFFFFFF);
    static_game_objects_.set_empty_key(0xFFFFFFFFFFFFFFFF);
    static_corpses_.set_empty_key(0xFFFFFFFFFFFFFFFF);
    static_elevators_.set_empty_key(-1);
}

ObjectMgr::~ObjectMgr()
{
    for (auto& elem : mQuestTemplates)
        delete elem.second;

    for (auto& elem : petInfo)
        delete[] elem.second;

    // free only if loaded
    for (auto& elem : playerClassInfo)
        delete[] elem.levelInfo;

    for (auto& elem : playerInfo)
        for (auto& elem_class_ : elem)
            delete[] elem_class_.levelInfo;

    // free objects
    for (auto& elem : mGroupMap)
        delete elem.second;

    for (auto& elem : mArenaTeamMap)
        delete elem.second;

    for (auto& elem : m_mCacheVendorTemplateItemMap)
        elem.second.Clear();

    for (auto& elem : m_mCacheVendorItemMap)
        elem.second.Clear();

    for (auto& elem : m_mCacheTrainerSpellMap)
        elem.second.Clear();
}

Group* ObjectMgr::GetGroupById(uint32 id) const
{
    auto itr = mGroupMap.find(id);
    if (itr != mGroupMap.end())
        return itr->second;

    return nullptr;
}

ArenaTeam* ObjectMgr::GetArenaTeamById(uint32 arenateamid) const
{
    auto itr = mArenaTeamMap.find(arenateamid);
    if (itr != mArenaTeamMap.end())
        return itr->second;

    return nullptr;
}

ArenaTeam* ObjectMgr::GetArenaTeamByName(const std::string& arenateamname) const
{
    for (const auto& elem : mArenaTeamMap)
        if (elem.second->GetName() == arenateamname)
            return elem.second;

    return nullptr;
}

ArenaTeam* ObjectMgr::GetArenaTeamByCaptain(ObjectGuid guid) const
{
    for (const auto& elem : mArenaTeamMap)
        if (elem.second->GetCaptainGuid() == guid)
            return elem.second;

    return nullptr;
}

CreatureInfo const* ObjectMgr::GetCreatureTemplate(uint32 id)
{
    return sCreatureStorage.LookupEntry<CreatureInfo>(id);
}

void ObjectMgr::AddLocaleString(
    std::string const& s, LocaleConstant locale, std::vector<std::string>& data)
{
    if (!s.empty())
    {
        if (data.size() <= size_t(locale))
            data.resize(locale + 1);

        data[locale] = s;
    }
}

void ObjectMgr::LoadCreatureLocales()
{
    mCreatureLocaleMap.clear(); // need for reload case

    QueryResult* result = WorldDatabase.Query(
        "SELECT "
        "entry,name_loc1,subname_loc1,name_loc2,subname_loc2,name_loc3,subname_"
        "loc3,name_loc4,subname_loc4,name_loc5,subname_loc5,name_loc6,subname_"
        "loc6,name_loc7,subname_loc7,name_loc8,subname_loc8 FROM "
        "locales_creature");

    if (!result)
    {
        logging.info(
            "Loaded 0 creature locale strings. DB table `locales_creature` "
            "is empty.");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 entry = fields[0].GetUInt32();

        if (!GetCreatureTemplate(entry))
        {
            logging.warning(
                "Table `locales_creature` has data for not existed creature "
                "entry %u, skipped.",
                entry);
            continue;
        }

        CreatureLocale& data = mCreatureLocaleMap[entry];

        for (int i = 1; i < MAX_LOCALE; ++i)
        {
            std::string str = fields[1 + 2 * (i - 1)].GetCppString();
            if (!str.empty())
            {
                int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    if ((int32)data.Name.size() <= idx)
                        data.Name.resize(idx + 1);

                    data.Name[idx] = str;
                }
            }
            str = fields[1 + 2 * (i - 1) + 1].GetCppString();
            if (!str.empty())
            {
                int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    if ((int32)data.SubName.size() <= idx)
                        data.SubName.resize(idx + 1);

                    data.SubName[idx] = str;
                }
            }
        }
    } while (result->NextRow());

    delete result;

    logging.info("Loaded %lu creature locale strings",
        (unsigned long)mCreatureLocaleMap.size());
}

void ObjectMgr::LoadGossipMenuItemsLocales()
{
    mGossipMenuItemsLocaleMap.clear(); // need for reload case

    QueryResult* result = WorldDatabase.Query(
        "SELECT menu_id,id,"
        "option_text_loc1,box_text_loc1,option_text_loc2,box_text_loc2,"
        "option_text_loc3,box_text_loc3,option_text_loc4,box_text_loc4,"
        "option_text_loc5,box_text_loc5,option_text_loc6,box_text_loc6,"
        "option_text_loc7,box_text_loc7,option_text_loc8,box_text_loc8 "
        "FROM locales_gossip_menu_option");

    if (!result)
    {
        logging.info(
            "Loaded 0 gossip_menu_option locale strings. DB table "
            "`locales_gossip_menu_option` is empty.");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint16 menuId = fields[0].GetUInt16();
        uint16 id = fields[1].GetUInt16();

        GossipMenuItemsMapBounds bounds = GetGossipMenuItemsMapBounds(menuId);

        bool found = false;
        if (bounds.first != bounds.second)
        {
            for (auto itr = bounds.first; itr != bounds.second; ++itr)
            {
                if (itr->second.id == id)
                {
                    found = true;
                    break;
                }
            }
        }

        if (!found)
        {
            logging.warning(
                "Table `locales_gossip_menu_option` has data for nonexistent "
                "gossip menu %u item %u, skipped.",
                menuId, id);
            continue;
        }

        GossipMenuItemsLocale& data =
            mGossipMenuItemsLocaleMap[MAKE_PAIR32(menuId, id)];

        for (int i = 1; i < MAX_LOCALE; ++i)
        {
            std::string str = fields[2 + 2 * (i - 1)].GetCppString();
            if (!str.empty())
            {
                int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    if ((int32)data.OptionText.size() <= idx)
                        data.OptionText.resize(idx + 1);

                    data.OptionText[idx] = str;
                }
            }
            str = fields[2 + 2 * (i - 1) + 1].GetCppString();
            if (!str.empty())
            {
                int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    if ((int32)data.BoxText.size() <= idx)
                        data.BoxText.resize(idx + 1);

                    data.BoxText[idx] = str;
                }
            }
        }
    } while (result->NextRow());

    delete result;

    logging.info("Loaded %lu gossip_menu_option locale strings",
        (unsigned long)mGossipMenuItemsLocaleMap.size());
}

void ObjectMgr::LoadPointOfInterestLocales()
{
    mPointOfInterestLocaleMap.clear(); // need for reload case

    QueryResult* result = WorldDatabase.Query(
        "SELECT "
        "entry,icon_name_loc1,icon_name_loc2,icon_name_loc3,icon_name_loc4,"
        "icon_name_loc5,icon_name_loc6,icon_name_loc7,icon_name_loc8 FROM "
        "locales_points_of_interest");

    if (!result)
    {
        logging.info(
            "Loaded 0 points_of_interest locale strings. DB table "
            "`locales_points_of_interest` is empty.");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 entry = fields[0].GetUInt32();

        if (!GetPointOfInterest(entry))
        {
            logging.warning(
                "Table `locales_points_of_interest` has data for nonexistent "
                "POI entry %u, skipped.",
                entry);
            continue;
        }

        PointOfInterestLocale& data = mPointOfInterestLocaleMap[entry];

        for (int i = 1; i < MAX_LOCALE; ++i)
        {
            std::string str = fields[i].GetCppString();
            if (str.empty())
                continue;

            int idx = GetOrNewIndexForLocale(LocaleConstant(i));
            if (idx >= 0)
            {
                if ((int32)data.IconName.size() <= idx)
                    data.IconName.resize(idx + 1);

                data.IconName[idx] = str;
            }
        }
    } while (result->NextRow());

    delete result;

    logging.info("Loaded " SIZEFMTD " points_of_interest locale strings\n",
        mPointOfInterestLocaleMap.size());
}

struct SQLCreatureLoader : public SQLStorageLoaderBase<SQLCreatureLoader>
{
    template <class D>
    void convert_from_str(uint32 /*field_pos*/, char const* src, D& dst)
    {
        dst = D(sScriptMgr::Instance()->GetScriptId(src));
    }
};

void ObjectMgr::LoadCreatureTemplates()
{
    SQLCreatureLoader loader;
    loader.Load(sCreatureStorage);

    logging.info(
        "Loaded %u creature definitions\n", sCreatureStorage.RecordCount);

    std::set<uint32> heroicEntries; // already loaded heroic value in creatures
    std::set<uint32>
        hasHeroicEntries; // already loaded creatures with heroic entry values

    // check data correctness
    for (uint32 i = 1; i < sCreatureStorage.MaxEntry; ++i)
    {
        CreatureInfo const* cInfo =
            sCreatureStorage.LookupEntry<CreatureInfo>(i);
        if (!cInfo)
            continue;

        if (cInfo->HeroicEntry)
        {
            CreatureInfo const* heroicInfo =
                GetCreatureTemplate(cInfo->HeroicEntry);
            if (!heroicInfo)
            {
                logging.error(
                    "Creature (Entry: %u) have `heroic_entry`=%u but creature "
                    "entry %u not exist.",
                    i, cInfo->HeroicEntry, cInfo->HeroicEntry);
                continue;
            }

            if (heroicEntries.find(i) != heroicEntries.end())
            {
                logging.error(
                    "Creature (Entry: %u) listed as heroic but have value in "
                    "`heroic_entry`.",
                    i);
                continue;
            }

            if (heroicEntries.find(cInfo->HeroicEntry) != heroicEntries.end())
            {
                logging.error(
                    "Creature (Entry: %u) already listed as heroic for another "
                    "entry.",
                    cInfo->HeroicEntry);
                continue;
            }

            if (hasHeroicEntries.find(cInfo->HeroicEntry) !=
                hasHeroicEntries.end())
            {
                logging.error(
                    "Creature (Entry: %u) have `heroic_entry`=%u but creature "
                    "entry %u have heroic entry also.",
                    i, cInfo->HeroicEntry, cInfo->HeroicEntry);
                continue;
            }

            if (cInfo->unit_class != heroicInfo->unit_class)
            {
                logging.error(
                    "Creature (Entry: %u, class %u) has different `unit_class` "
                    "in heroic mode (Entry: %u, class %u).",
                    i, cInfo->unit_class, cInfo->HeroicEntry,
                    heroicInfo->unit_class);
                continue;
            }

            if (cInfo->npcflag != heroicInfo->npcflag)
            {
                logging.error(
                    "Creature (Entry: %u) has different `npcflag` in heroic "
                    "mode (Entry: %u).",
                    i, cInfo->HeroicEntry);
                continue;
            }

            if (cInfo->trainer_class != heroicInfo->trainer_class)
            {
                logging.error(
                    "Creature (Entry: %u) has different `trainer_class` in "
                    "heroic mode (Entry: %u).",
                    i, cInfo->HeroicEntry);
                continue;
            }

            if (cInfo->trainer_race != heroicInfo->trainer_race)
            {
                logging.error(
                    "Creature (Entry: %u) has different `trainer_race` in "
                    "heroic mode (Entry: %u).",
                    i, cInfo->HeroicEntry);
                continue;
            }

            if (cInfo->trainer_type != heroicInfo->trainer_type)
            {
                logging.error(
                    "Creature (Entry: %u) has different `trainer_type` in "
                    "heroic mode (Entry: %u).",
                    i, cInfo->HeroicEntry);
                continue;
            }

            if (cInfo->trainer_spell != heroicInfo->trainer_spell)
            {
                logging.error(
                    "Creature (Entry: %u) has different `trainer_spell` in "
                    "heroic mode (Entry: %u).",
                    i, cInfo->HeroicEntry);
                continue;
            }

            if (heroicInfo->AIName && *heroicInfo->AIName)
            {
                logging.error(
                    "Heroic mode creature (Entry: %u) has `AIName`, but in any "
                    "case will used normal mode creature (Entry: %u) AIName.",
                    cInfo->HeroicEntry, i);
                continue;
            }

            if (heroicInfo->ScriptID)
            {
                logging.error(
                    "Heroic mode creature (Entry: %u) has `ScriptName`, but in "
                    "any case will used normal mode creature (Entry: %u) "
                    "ScriptName.",
                    cInfo->HeroicEntry, i);
                continue;
            }

            hasHeroicEntries.insert(i);
            heroicEntries.insert(cInfo->HeroicEntry);
        }

        FactionTemplateEntry const* factionTemplate =
            sFactionTemplateStore.LookupEntry(cInfo->faction_A);
        if (!factionTemplate)
            logging.error(
                "Creature (Entry: %u) has nonexistent faction_A template (%u)",
                cInfo->Entry, cInfo->faction_A);

        factionTemplate = sFactionTemplateStore.LookupEntry(cInfo->faction_H);
        if (!factionTemplate)
            logging.error(
                "Creature (Entry: %u) has nonexistent faction_H template (%u)",
                cInfo->Entry, cInfo->faction_H);

        for (int k = 0; k < MAX_KILL_CREDIT; ++k)
        {
            if (cInfo->KillCredit[k])
            {
                if (!GetCreatureTemplate(cInfo->KillCredit[k]))
                {
                    logging.error(
                        "Creature (Entry: %u) has nonexistent creature entry "
                        "in `KillCredit%d` (%u)",
                        cInfo->Entry, k + 1, cInfo->KillCredit[k]);
                    const_cast<CreatureInfo*>(cInfo)->KillCredit[k] = 0;
                }
            }
        }

        // used later for scale
        CreatureDisplayInfoEntry const* displayScaleEntry = nullptr;

        for (int i = 0; i < MAX_CREATURE_MODEL; ++i)
        {
            if (cInfo->ModelId[i])
            {
                CreatureDisplayInfoEntry const* displayEntry =
                    sCreatureDisplayInfoStore.LookupEntry(cInfo->ModelId[i]);
                if (!displayEntry)
                {
                    logging.error(
                        "Creature (Entry: %u) has nonexistent modelid_%d (%u), "
                        "can crash client",
                        cInfo->Entry, i + 1, cInfo->ModelId[i]);
                    const_cast<CreatureInfo*>(cInfo)->ModelId[i] = 0;
                }
                else if (!displayScaleEntry)
                    displayScaleEntry = displayEntry;

                CreatureModelInfo const* minfo =
                    sCreatureModelStorage.LookupEntry<CreatureModelInfo>(
                        cInfo->ModelId[i]);
                if (!minfo)
                    logging.error(
                        "Creature (Entry: %u) are using modelid_%d (%u), but "
                        "creature_model_info are missing for this model.",
                        cInfo->Entry, i + 1, cInfo->ModelId[i]);

                if (minfo &&
                    minfo->combat_reach < minfo->bounding_radius + 0.5f)
                    logging.error(
                        "Creature (Entry: %u) uses modelid_%d (%u), which has "
                        "what looks like an invalid combat_reach in relation "
                        "to bounding_radius.",
                        cInfo->Entry, i + 1, cInfo->ModelId[i]);
            }
        }

        if (!displayScaleEntry)
            logging.error(
                "Creature (Entry: %u) has nonexistent modelid in "
                "modelid_1/modelid_2/modelid_3/modelid_4",
                cInfo->Entry);

        // use below code for 0-checks for unit_class
        if (!cInfo->unit_class)
            logging.warning(
                "Creature (Entry: %u) not has proper unit_class(%u) for "
                "creature_template",
                cInfo->Entry, cInfo->unit_class);
        else if (((1 << (cInfo->unit_class - 1)) & CLASSMASK_ALL_CREATURES) ==
                 0)
            logging.error(
                "Creature (Entry: %u) has invalid unit_class(%u) for "
                "creature_template",
                cInfo->Entry, cInfo->unit_class);

        if (cInfo->dmgschool >= MAX_SPELL_SCHOOL)
        {
            logging.error(
                "Creature (Entry: %u) has invalid spell school value (%u) in "
                "`dmgschool`",
                cInfo->Entry, cInfo->dmgschool);
            const_cast<CreatureInfo*>(cInfo)->dmgschool = SPELL_SCHOOL_NORMAL;
        }

        if (cInfo->baseattacktime == 0)
            const_cast<CreatureInfo*>(cInfo)->baseattacktime = BASE_ATTACK_TIME;

        if (cInfo->rangeattacktime == 0)
            const_cast<CreatureInfo*>(cInfo)->rangeattacktime =
                BASE_ATTACK_TIME;

        if ((cInfo->npcflag & UNIT_NPC_FLAG_TRAINER) &&
            cInfo->trainer_type >= MAX_TRAINER_TYPE)
            logging.error("Creature (Entry: %u) has wrong trainer type %u",
                cInfo->Entry, cInfo->trainer_type);

        if (cInfo->type && !sCreatureTypeStore.LookupEntry(cInfo->type))
        {
            logging.error(
                "Creature (Entry: %u) has invalid creature type (%u) in `type`",
                cInfo->Entry, cInfo->type);
            const_cast<CreatureInfo*>(cInfo)->type = CREATURE_TYPE_HUMANOID;
        }

        // must exist or used hidden but used in data horse case
        if (cInfo->family && !sCreatureFamilyStore.LookupEntry(cInfo->family) &&
            cInfo->family != CREATURE_FAMILY_HORSE_CUSTOM)
        {
            logging.error(
                "Creature (Entry: %u) has invalid creature family (%u) in "
                "`family`",
                cInfo->Entry, cInfo->family);
            const_cast<CreatureInfo*>(cInfo)->family = 0;
        }

        if (cInfo->InhabitType <= 0 || cInfo->InhabitType > INHABIT_ANYWHERE)
        {
            logging.error(
                "Creature (Entry: %u) has wrong value (%u) in `InhabitType`, "
                "creature will not correctly walk/swim/fly",
                cInfo->Entry, cInfo->InhabitType);
            const_cast<CreatureInfo*>(cInfo)->InhabitType = INHABIT_ANYWHERE;
        }

        if (cInfo->PetSpellDataId)
        {
            CreatureSpellDataEntry const* spellDataId =
                sCreatureSpellDataStore.LookupEntry(cInfo->PetSpellDataId);
            if (!spellDataId)
                logging.error(
                    "Creature (Entry: %u) has non-existing PetSpellDataId (%u)",
                    cInfo->Entry, cInfo->PetSpellDataId);
        }

        for (int j = 0; j < CREATURE_MAX_SPELLS; ++j)
        {
            if (cInfo->spells[j] && !sSpellStore.LookupEntry(cInfo->spells[j]))
            {
                logging.error(
                    "Creature (Entry: %u) has non-existing Spell%d (%u), set "
                    "to 0",
                    cInfo->Entry, j + 1, cInfo->spells[j]);
                const_cast<CreatureInfo*>(cInfo)->spells[j] = 0;
            }
        }

        if (cInfo->MovementType > (int)movement::gen::random_waterair)
        {
            logging.error(
                "Creature (Entry: %u) has wrong movement generator type (%u), "
                "ignore and set to IDLE.",
                cInfo->Entry, cInfo->MovementType);
            const_cast<CreatureInfo*>(cInfo)->MovementType =
                (int)movement::gen::idle;
        }

        if (cInfo->equipmentId > 0) // 0 no equipment
        {
            if (!GetEquipmentInfo(cInfo->equipmentId) &&
                !GetEquipmentInfoRaw(cInfo->equipmentId))
            {
                logging.error(
                    "Table `creature_template` have creature (Entry: %u) with "
                    "equipment_id %u not found in table "
                    "`creature_equip_template` or "
                    "`creature_equip_template_raw`, set to no equipment.",
                    cInfo->Entry, cInfo->equipmentId);
                const_cast<CreatureInfo*>(cInfo)->equipmentId = 0;
            }
        }

        if (cInfo->vendorId > 0)
        {
            if (!(cInfo->npcflag & UNIT_NPC_FLAG_VENDOR))
                logging.error(
                    "Table `creature_template` have creature (Entry: %u) with "
                    "vendor_id %u but not have flag UNIT_NPC_FLAG_VENDOR (%u), "
                    "vendor items will ignored.",
                    cInfo->Entry, cInfo->vendorId, UNIT_NPC_FLAG_VENDOR);
        }

        /// if not set custom creature scale then load scale from
        /// CreatureDisplayInfo.dbc
        if (cInfo->scale <= 0.0f)
        {
            if (displayScaleEntry)
                const_cast<CreatureInfo*>(cInfo)->scale =
                    displayScaleEntry->scale;
            else
                const_cast<CreatureInfo*>(cInfo)->scale = DEFAULT_OBJECT_SCALE;
        }
    }
}

void ObjectMgr::ConvertCreatureAddonAuras(
    CreatureDataAddon* addon, char const* table, char const* guidEntryStr)
{
    // Now add the auras, format "spell1 spell2 ..."
    char* p, *s;
    std::vector<int> val;
    s = p = (char*)reinterpret_cast<char const*>(addon->auras);
    if (p)
    {
        while (p[0] != 0)
        {
            ++p;
            if (p[0] == ' ')
            {
                val.push_back(atoi(s));
                s = ++p;
            }
        }
        if (p != s)
            val.push_back(atoi(s));

        // free char* loaded memory
        delete[](char*)reinterpret_cast<char const*>(addon->auras);
    }

    // empty list
    if (val.empty())
    {
        addon->auras = nullptr;
        return;
    }

    // replace by new structures array
    const_cast<uint32*&>(addon->auras) = new uint32[val.size() + 1];

    uint32 i = 0;
    for (auto& elem : val)
    {
        uint32& cAura = const_cast<uint32&>(addon->auras[i]);
        cAura = uint32(elem);

        SpellEntry const* AdditionalSpellInfo = sSpellStore.LookupEntry(cAura);
        if (!AdditionalSpellInfo)
        {
            logging.error(
                "Creature (%s: %u) has wrong spell %u defined in `auras` field "
                "in `%s`.",
                guidEntryStr, addon->guidOrEntry, cAura, table);
            continue;
        }

        if (std::find(&addon->auras[0], &addon->auras[i], cAura) !=
            &addon->auras[i])
        {
            logging.error(
                "Creature (%s: %u) has duplicate spell %u defined in `auras` "
                "field in `%s`.",
                guidEntryStr, addon->guidOrEntry, cAura, table);
            continue;
        }

        ++i;
    }

    // fill terminator element (after last added)
    const_cast<uint32&>(addon->auras[i]) = 0;
}

void ObjectMgr::LoadCreatureAddons(
    SQLStorage& creatureaddons, char const* entryName, char const* comment)
{
    creatureaddons.Load();

    logging.info("Loaded %u %s\n", creatureaddons.RecordCount, comment);

    // check data correctness and convert 'auras'
    for (uint32 i = 1; i < creatureaddons.MaxEntry; ++i)
    {
        CreatureDataAddon const* addon =
            creatureaddons.LookupEntry<CreatureDataAddon>(i);
        if (!addon)
            continue;

        if (addon->mount)
        {
            if (!sCreatureDisplayInfoStore.LookupEntry(addon->mount))
            {
                logging.error(
                    "Creature (%s %u) have invalid displayInfoId for mount "
                    "(%u) defined in `%s`.",
                    entryName, addon->guidOrEntry, addon->mount,
                    creatureaddons.GetTableName());
                const_cast<CreatureDataAddon*>(addon)->mount = 0;
            }
        }

        if (addon->sheath_state > SHEATH_STATE_RANGED)
            logging.error(
                "Creature (%s %u) has unknown sheath state (%u) defined in "
                "`%s`.",
                entryName, addon->guidOrEntry, addon->sheath_state,
                creatureaddons.GetTableName());

        if (!sEmotesStore.LookupEntry(addon->emote))
        {
            logging.error(
                "Creature (%s %u) have invalid emote (%u) defined in `%s`.",
                entryName, addon->guidOrEntry, addon->emote,
                creatureaddons.GetTableName());
            const_cast<CreatureDataAddon*>(addon)->emote = 0;
        }

        ConvertCreatureAddonAuras(const_cast<CreatureDataAddon*>(addon),
            creatureaddons.GetTableName(), entryName);
    }
}

void ObjectMgr::LoadCreatureAddons()
{
    LoadCreatureAddons(
        sCreatureInfoAddonStorage, "Entry", "creature template addons");

    // check entry ids
    for (uint32 i = 1; i < sCreatureInfoAddonStorage.MaxEntry; ++i)
        if (CreatureDataAddon const* addon =
                sCreatureInfoAddonStorage.LookupEntry<CreatureDataAddon>(i))
            if (!sCreatureStorage.LookupEntry<CreatureInfo>(addon->guidOrEntry))
                logging.error(
                    "Creature (Entry: %u) does not exist but has a record in "
                    "`%s`",
                    addon->guidOrEntry,
                    sCreatureInfoAddonStorage.GetTableName());

    LoadCreatureAddons(sCreatureDataAddonStorage, "GUID", "creature addons");

    // check entry ids
    for (uint32 i = 1; i < sCreatureDataAddonStorage.MaxEntry; ++i)
        if (CreatureDataAddon const* addon =
                sCreatureDataAddonStorage.LookupEntry<CreatureDataAddon>(i))
            if (mCreatureDataMap.find(addon->guidOrEntry) ==
                mCreatureDataMap.end())
                logging.error(
                    "Creature (GUID: %u) does not exist but has a record in "
                    "`creature_addon`",
                    addon->guidOrEntry);
}

EquipmentInfo const* ObjectMgr::GetEquipmentInfo(uint32 entry) const
{
    return sEquipmentStorage.LookupEntry<EquipmentInfo>(entry);
}

EquipmentInfoRaw const* ObjectMgr::GetEquipmentInfoRaw(uint32 entry) const
{
    return sEquipmentStorageRaw.LookupEntry<EquipmentInfoRaw>(entry);
}

void ObjectMgr::LoadEquipmentTemplates()
{
    sEquipmentStorage.Load(true);

    for (uint32 i = 0; i < sEquipmentStorage.MaxEntry; ++i)
    {
        EquipmentInfo const* eqInfo =
            sEquipmentStorage.LookupEntry<EquipmentInfo>(i);

        if (!eqInfo)
            continue;

        for (uint8 j = 0; j < 3; ++j)
        {
            if (!eqInfo->equipentry[j])
                continue;

            ItemPrototype const* itemProto =
                GetItemPrototype(eqInfo->equipentry[j]);
            if (!itemProto)
            {
                logging.error(
                    "Unknown item (entry=%u) in "
                    "creature_equip_template.equipentry%u for entry = %u, "
                    "forced to 0.",
                    eqInfo->equipentry[j], j + 1, i);
                const_cast<EquipmentInfo*>(eqInfo)->equipentry[j] = 0;
                continue;
            }

            if (itemProto->InventoryType != INVTYPE_WEAPON &&
                itemProto->InventoryType != INVTYPE_SHIELD &&
                itemProto->InventoryType != INVTYPE_RANGED &&
                itemProto->InventoryType != INVTYPE_2HWEAPON &&
                itemProto->InventoryType != INVTYPE_WEAPONMAINHAND &&
                itemProto->InventoryType != INVTYPE_WEAPONOFFHAND &&
                itemProto->InventoryType != INVTYPE_HOLDABLE &&
                itemProto->InventoryType != INVTYPE_THROWN &&
                itemProto->InventoryType != INVTYPE_RANGEDRIGHT &&
                itemProto->InventoryType != INVTYPE_RELIC)
            {
                logging.error(
                    "Item (entry=%u) in creature_equip_template.equipentry%u "
                    "for entry = %u is not equipable in a hand, forced to 0.",
                    eqInfo->equipentry[j], j + 1, i);
                const_cast<EquipmentInfo*>(eqInfo)->equipentry[j] = 0;
            }
        }
    }

    logging.info("Loaded %u equipment template", sEquipmentStorage.RecordCount);

    sEquipmentStorageRaw.Load(false);
    for (uint32 i = 1; i < sEquipmentStorageRaw.MaxEntry; ++i)
        if (sEquipmentStorageRaw.LookupEntry<EquipmentInfoRaw>(i))
            if (sEquipmentStorage.LookupEntry<EquipmentInfo>(i))
                logging.error(
                    "Table 'creature_equip_template_raw` have redundant data "
                    "for ID %u ('creature_equip_template` already have data)",
                    i);

    logging.info("Loaded %u equipment template (deprecated format)\n",
        sEquipmentStorageRaw.RecordCount);
}

CreatureModelInfo const* ObjectMgr::GetCreatureModelInfo(uint32 modelid) const
{
    return sCreatureModelStorage.LookupEntry<CreatureModelInfo>(modelid);
}

// generally models that does not have a gender(2), or has alternative model for
// same gender
uint32 ObjectMgr::GetCreatureModelAlternativeModel(uint32 modelId) const
{
    if (const CreatureModelInfo* modelInfo = GetCreatureModelInfo(modelId))
        return modelInfo->modelid_alternative;

    return 0;
}

CreatureModelInfo const* ObjectMgr::GetCreatureModelRandomGender(
    uint32 display_id) const
{
    CreatureModelInfo const* minfo = GetCreatureModelInfo(display_id);
    if (!minfo)
        return nullptr;

    // If a model for another gender exists, 50% chance to use it
    if (minfo->modelid_other_gender != 0 && urand(0, 1) == 0)
    {
        CreatureModelInfo const* minfo_tmp =
            GetCreatureModelInfo(minfo->modelid_other_gender);
        if (!minfo_tmp)
        {
            logging.error(
                "Model (Entry: %u) has modelid_other_gender %u not found in "
                "table `creature_model_info`. ",
                minfo->modelid, minfo->modelid_other_gender);
            return minfo; // not fatal, just use the previous one
        }
        else
            return minfo_tmp;
    }
    else
        return minfo;
}

// returns: points of health per stamina
float ObjectMgr::GetPetStaminaScaling(uint32 cid) const
{
    auto itr = pet_scaling_.find(cid);
    if (itr != pet_scaling_.end())
        return itr->second.stamina;

    // default value
    return 10.0f;
}

// returns: points of mana per intellect
float ObjectMgr::GetPetIntellectScaling(uint32 cid) const
{
    auto itr = pet_scaling_.find(cid);
    if (itr != pet_scaling_.end())
        return itr->second.intellect;

    // default value
    return 15.0f;
}

uint32 ObjectMgr::GetModelForRace(uint32 sourceModelId, uint32 racemask) const
{
    uint32 modelId = 0;

    CreatureModelRaceMapBounds bounds =
        m_mCreatureModelRaceMap.equal_range(sourceModelId);

    for (auto itr = bounds.first; itr != bounds.second; ++itr)
    {
        if (!(itr->second.racemask & racemask))
            continue;

        if (itr->second.creature_entry)
        {
            const CreatureInfo* cInfo =
                GetCreatureTemplate(itr->second.creature_entry);
            modelId = Creature::ChooseDisplayId(cInfo);
        }
        else
        {
            modelId = itr->second.modelid_racial;
        }
    }

    return modelId;
}

void ObjectMgr::LoadCreatureModelInfo()
{
    sCreatureModelStorage.Load();

    // post processing
    for (uint32 i = 1; i < sCreatureModelStorage.MaxEntry; ++i)
    {
        CreatureModelInfo const* minfo =
            sCreatureModelStorage.LookupEntry<CreatureModelInfo>(i);
        if (!minfo)
            continue;

        if (!sCreatureDisplayInfoStore.LookupEntry(minfo->modelid))
            logging.error(
                "Table `creature_model_info` has model for nonexistent model "
                "id (%u).",
                minfo->modelid);

        if (minfo->gender >= MAX_GENDER)
        {
            logging.error(
                "Table `creature_model_info` has invalid gender (%u) for model "
                "id (%u).",
                uint32(minfo->gender), minfo->modelid);
            const_cast<CreatureModelInfo*>(minfo)->gender = GENDER_MALE;
        }

        if (minfo->modelid_other_gender)
        {
            if (minfo->modelid_other_gender == minfo->modelid)
            {
                logging.error(
                    "Table `creature_model_info` has redundant "
                    "modelid_other_gender model (%u) defined for model id %u.",
                    minfo->modelid_other_gender, minfo->modelid);
                const_cast<CreatureModelInfo*>(minfo)->modelid_other_gender = 0;
            }
            else if (!sCreatureDisplayInfoStore.LookupEntry(
                         minfo->modelid_other_gender))
            {
                logging.error(
                    "Table `creature_model_info` has nonexistent "
                    "modelid_other_gender model (%u) defined for model id %u.",
                    minfo->modelid_other_gender, minfo->modelid);
                const_cast<CreatureModelInfo*>(minfo)->modelid_other_gender = 0;
            }
        }

        if (minfo->modelid_alternative)
        {
            if (minfo->modelid_alternative == minfo->modelid)
            {
                logging.error(
                    "Table `creature_model_info` has redundant "
                    "modelid_alternative model (%u) defined for model id %u.",
                    minfo->modelid_alternative, minfo->modelid);
                const_cast<CreatureModelInfo*>(minfo)->modelid_alternative = 0;
            }
            else if (!sCreatureDisplayInfoStore.LookupEntry(
                         minfo->modelid_alternative))
            {
                logging.error(
                    "Table `creature_model_info` has nonexistent "
                    "modelid_alternative model (%u) defined for model id %u.",
                    minfo->modelid_alternative, minfo->modelid);
                const_cast<CreatureModelInfo*>(minfo)->modelid_alternative = 0;
            }
        }
    }

    // character races expected have model info data in table
    for (uint32 race = 1; race < sChrRacesStore.GetNumRows(); ++race)
    {
        ChrRacesEntry const* raceEntry = sChrRacesStore.LookupEntry(race);
        if (!raceEntry)
            continue;

        if (!((1 << (race - 1)) & RACEMASK_ALL_PLAYABLE))
            continue;

        if (CreatureModelInfo const* minfo =
                GetCreatureModelInfo(raceEntry->model_f))
        {
            if (minfo->gender != GENDER_FEMALE)
                logging.error(
                    "Table `creature_model_info` have wrong gender %u for "
                    "character race %u female model id %u",
                    minfo->gender, race, raceEntry->model_f);

            if (minfo->modelid_other_gender != raceEntry->model_m)
                logging.error(
                    "Table `creature_model_info` have wrong other gender model "
                    "id %u for character race %u female model id %u",
                    minfo->modelid_other_gender, race, raceEntry->model_f);

            if (minfo->bounding_radius <= 0.0f)
            {
                logging.error(
                    "Table `creature_model_info` have wrong bounding_radius %f "
                    "for character race %u female model id %u, use %f instead",
                    minfo->bounding_radius, race, raceEntry->model_f,
                    DEFAULT_BOUNDING_RADIUS);
                const_cast<CreatureModelInfo*>(minfo)->bounding_radius =
                    DEFAULT_BOUNDING_RADIUS;
            }

            if (minfo->combat_reach != 1.5f)
            {
                logging.error(
                    "Table `creature_model_info` have wrong combat_reach %f "
                    "for character race %u female model id %u, expected always "
                    "1.5f",
                    minfo->combat_reach, race, raceEntry->model_f);
                const_cast<CreatureModelInfo*>(minfo)->combat_reach = 1.5f;
            }
        }
        else
            logging.error(
                "Table `creature_model_info` expect have data for character "
                "race %u female model id %u",
                race, raceEntry->model_f);

        if (CreatureModelInfo const* minfo =
                GetCreatureModelInfo(raceEntry->model_m))
        {
            if (minfo->gender != GENDER_MALE)
                logging.error(
                    "Table `creature_model_info` have wrong gender %u for "
                    "character race %u male model id %u",
                    minfo->gender, race, raceEntry->model_m);

            if (minfo->modelid_other_gender != raceEntry->model_f)
                logging.error(
                    "Table `creature_model_info` have wrong other gender model "
                    "id %u for character race %u male model id %u",
                    minfo->modelid_other_gender, race, raceEntry->model_m);

            if (minfo->bounding_radius <= 0.0f)
            {
                logging.error(
                    "Table `creature_model_info` have wrong bounding_radius %f "
                    "for character race %u male model id %u, use %f instead",
                    minfo->bounding_radius, race, raceEntry->model_f,
                    DEFAULT_BOUNDING_RADIUS);
                const_cast<CreatureModelInfo*>(minfo)->bounding_radius =
                    DEFAULT_BOUNDING_RADIUS;
            }

            if (minfo->combat_reach != 1.5f)
            {
                logging.error(
                    "Table `creature_model_info` have wrong combat_reach %f "
                    "for character race %u male model id %u, expected always "
                    "1.5f",
                    minfo->combat_reach, race, raceEntry->model_m);
                const_cast<CreatureModelInfo*>(minfo)->combat_reach = 1.5f;
            }
        }
        else
            logging.error(
                "Table `creature_model_info` expect have data for character "
                "race %u male model id %u",
                race, raceEntry->model_m);
    }

    logging.info("Loaded %u creature model based info\n",
        sCreatureModelStorage.RecordCount);
}

void ObjectMgr::LoadCreatureModelRace()
{
    m_mCreatureModelRaceMap.clear(); // can be used for reload

    QueryResult* result = WorldDatabase.Query(
        "SELECT modelid, racemask, creature_entry, modelid_racial FROM "
        "creature_model_race");

    if (!result)
    {
        logging.error("Loaded creature_model_race, table is empty!\n");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    uint32 count = 0;

    // model, racemask
    std::map<uint32, uint32> model2raceMask;

    do
    {
        bar.step();

        Field* fields = result->Fetch();

        CreatureModelRace raceData;

        raceData.modelid = fields[0].GetUInt32();
        raceData.racemask = fields[1].GetUInt32();
        raceData.creature_entry = fields[2].GetUInt32();
        raceData.modelid_racial = fields[3].GetUInt32();

        if (!sCreatureDisplayInfoStore.LookupEntry(raceData.modelid))
        {
            logging.error(
                "Table `creature_model_race` has model for nonexistent model "
                "id (%u), skipping",
                raceData.modelid);
            continue;
        }

        if (!sCreatureModelStorage.LookupEntry<CreatureModelInfo>(
                raceData.modelid))
        {
            logging.error(
                "Table `creature_model_race` modelid %u does not exist in "
                "creature_model_info, skipping",
                raceData.modelid);
            continue;
        }

        if (!raceData.racemask)
        {
            logging.error(
                "Table `creature_model_race` modelid %u has no racemask "
                "defined, skipping",
                raceData.modelid);
            continue;
        }

        if (!(raceData.racemask & RACEMASK_ALL_PLAYABLE))
        {
            logging.error(
                "Table `creature_model_race` modelid %u include invalid "
                "racemask, skipping",
                raceData.modelid);
            continue;
        }

        std::map<uint32, uint32>::const_iterator model2Race =
            model2raceMask.find(raceData.modelid);

        // can't have same mask for same model several times
        if (model2Race != model2raceMask.end())
        {
            if (model2Race->second & raceData.racemask)
            {
                logging.error(
                    "Table `creature_model_race` modelid %u with racemask %u "
                    "has mask already included for same modelid, skipping",
                    raceData.modelid, raceData.racemask);
                continue;
            }
        }

        model2raceMask[raceData.modelid] |= raceData.racemask;

        // creature_entry is the prefered way
        if (raceData.creature_entry)
        {
            if (raceData.modelid_racial)
                logging.error(
                    "Table `creature_model_race` modelid %u has modelid_racial "
                    "for modelid %u but a creature_entry are already defined, "
                    "modelid_racial will never be used.",
                    raceData.modelid, raceData.modelid_racial);

            if (!sCreatureStorage.LookupEntry<CreatureInfo>(
                    raceData.creature_entry))
            {
                logging.error(
                    "Table `creature_model_race` modelid %u has creature_entry "
                    "for nonexistent creature_template (%u), skipping",
                    raceData.modelid, raceData.creature_entry);
                continue;
            }
        }
        else if (raceData.modelid_racial)
        {
            if (!sCreatureDisplayInfoStore.LookupEntry(raceData.modelid_racial))
            {
                logging.error(
                    "Table `creature_model_race` modelid %u has modelid_racial "
                    "for nonexistent model id (%u), skipping",
                    raceData.modelid, raceData.modelid_racial);
                continue;
            }

            if (!sCreatureModelStorage.LookupEntry<CreatureModelInfo>(
                    raceData.modelid_racial))
            {
                logging.error(
                    "Table `creature_model_race` modelid %u has modelid_racial "
                    "%u, but are not defined in creature_model_info, skipping",
                    raceData.modelid, raceData.modelid_racial);
                continue;
            }
        }
        else
        {
            logging.error(
                "Table `creature_model_race` modelid %u does not have either "
                "creature_entry or modelid_racial defined, skipping",
                raceData.modelid);
            continue;
        }

        m_mCreatureModelRaceMap.insert(
            CreatureModelRaceMap::value_type(raceData.modelid, raceData));

        ++count;
    } while (result->NextRow());

    delete result;

    logging.info("Loaded %u creature_model_race entries\n", count);
}

void ObjectMgr::LoadCreatures()
{
    uint32 count = 0;
    QueryResult* result = WorldDatabase.Query(
        //      0              1            2    3
        "SELECT creature.guid, creature.id, map, modelid,"
        // 4           5           6           7           8
        "equipment_id, position_x, position_y, position_z, orientation, "
        // 9            10         11
        "spawntimesecs, spawndist, currentwaypoint,"
        // 12       13       14          15            16         17
        "curhealth, curmana, DeathState, MovementType, spawnMask, event,"
        // 18                      19
        "pool_creature.pool_entry, pool_creature_template.pool_entry, "
        // 20             21
        "boss_link_entry, boss_link_guid, "
        // 22     23       24       25            26            27
        "leash_x, leash_y, leash_z, leash_radius, aggro_radius, chain_radius "
        "FROM creature "
        "LEFT OUTER JOIN game_event_creature ON creature.guid = "
        "game_event_creature.guid "
        "LEFT OUTER JOIN pool_creature ON creature.guid = pool_creature.guid "
        "LEFT OUTER JOIN pool_creature_template ON creature.id = "
        "pool_creature_template.id");

    if (!result)
    {
        logging.error("Loaded 0 creature. DB table `creature` is empty.\n");
        return;
    }

    // build single time for check creature data
    std::set<uint32> heroicCreatures;
    for (uint32 i = 0; i < sCreatureStorage.MaxEntry; ++i)
        if (CreatureInfo const* cInfo =
                sCreatureStorage.LookupEntry<CreatureInfo>(i))
            if (cInfo->HeroicEntry)
                heroicCreatures.insert(cInfo->HeroicEntry);

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 guid = fields[0].GetUInt32();
        uint32 entry = fields[1].GetUInt32();

        CreatureInfo const* cInfo = GetCreatureTemplate(entry);
        if (!cInfo)
        {
            logging.error(
                "Table `creature` has creature (GUID: %u) with non existing "
                "creature entry %u, skipped.",
                guid, entry);
            continue;
        }

        CreatureData& data = mCreatureDataMap[guid];

        data.id = entry;
        data.guid = guid;
        data.mapid = fields[2].GetUInt32();
        data.modelid_override = fields[3].GetUInt32();
        data.equipmentId = fields[4].GetUInt32();
        data.posX = fields[5].GetFloat();
        data.posY = fields[6].GetFloat();
        data.posZ = fields[7].GetFloat();
        data.orientation = fields[8].GetFloat();
        data.spawntimesecs = fields[9].GetUInt32();
        data.spawndist = fields[10].GetFloat();
        data.currentwaypoint = fields[11].GetUInt32();
        data.curhealth = fields[12].GetUInt32();
        data.curmana = fields[13].GetUInt32();
        data.is_dead = fields[14].GetBool();
        data.movementType = fields[15].GetUInt8();
        data.spawnMask = fields[16].GetUInt8();
        int16 gameEvent = fields[17].GetInt16();
        int16 GuidPoolId = fields[18].GetInt16();
        int16 EntryPoolId = fields[19].GetInt16();
        data.boss_link_entry = fields[20].GetUInt32();
        data.boss_link_guid = fields[21].GetUInt32();
        data.leash_x = fields[22].GetFloat();
        data.leash_y = fields[23].GetFloat();
        data.leash_z = fields[24].GetFloat();
        data.leash_radius = fields[25].GetFloat();
        data.aggro_radius = fields[26].GetFloat();
        data.chain_radius = fields[27].GetFloat();
        data.special_visibility = cInfo->special_visibility;

        MapEntry const* mapEntry = sMapStore.LookupEntry(data.mapid);
        if (!mapEntry)
        {
            logging.error(
                "Table `creature` have creature (GUID: %u) that spawned at "
                "nonexistent map (Id: %u), skipped.",
                guid, data.mapid);
            continue;
        }

        if (mapEntry->IsDungeon())
        {
            if (data.spawnMask & ~SPAWNMASK_DUNGEON_ALL)
                logging.error(
                    "Table `creature` have creature (GUID: %u) that have wrong "
                    "spawn mask %u for non-raid dungeon map (Id: %u).",
                    guid, data.spawnMask, data.mapid);
        }
        else
        {
            if (data.spawnMask & ~SPAWNMASK_REGULAR)
                logging.error(
                    "Table `creature` have creature (GUID: %u) that have wrong "
                    "spawn mask %u for non-dungeon map (Id: %u).",
                    guid, data.spawnMask, data.mapid);
        }

        if (heroicCreatures.find(data.id) != heroicCreatures.end())
        {
            logging.error(
                "Table `creature` have creature (GUID: %u) that listed as "
                "heroic template (entry: %u) in `creature_template`, skipped.",
                guid, data.id);
            continue;
        }

        if (data.modelid_override > 0 &&
            !sCreatureDisplayInfoStore.LookupEntry(data.modelid_override))
        {
            logging.error(
                "Table `creature` GUID %u (entry %u) has model for nonexistent "
                "model id (%u), set to 0.",
                guid, data.id, data.modelid_override);
            data.modelid_override = 0;
        }

        if (data.equipmentId > 0) // -1 no equipment, 0 use default
        {
            if (!GetEquipmentInfo(data.equipmentId) &&
                !GetEquipmentInfoRaw(data.equipmentId))
            {
                logging.error(
                    "Table `creature` have creature (Entry: %u) with "
                    "equipment_id %u not found in table "
                    "`creature_equip_template` or "
                    "`creature_equip_template_raw`, set to no equipment.",
                    data.id, data.equipmentId);
                data.equipmentId = -1;
            }
        }

        if (cInfo->RegenHealth && data.curhealth &&
            data.curhealth < cInfo->minhealth)
        {
            logging.error(
                "Table `creature` have creature (GUID: %u Entry: %u) with "
                "`creature_template`.`RegenHealth`=1 and low current health "
                "(%u), `creature_template`.`minhealth`=%u.",
                guid, data.id, data.curhealth, cInfo->minhealth);
            data.curhealth = 0;
        }

        if (cInfo->RegenMana && data.curmana && data.curmana < cInfo->minmana)
        {
            logging.error(
                "Table `creature` has a creature (GUID %u Entry %u) that has "
                "`creature_template`.`RegenMana`=1"
                " and low current mana (%u). `creature_template`.`minmana`=%u.",
                guid, data.id, data.curmana, cInfo->minmana);
            data.curmana = 0;
        }

        if (cInfo->flags_extra & CREATURE_FLAG_EXTRA_INSTANCE_BIND)
        {
            if (!mapEntry || !mapEntry->IsDungeon())
                logging.error(
                    "Table `creature` have creature (GUID: %u Entry: %u) with "
                    "`creature_template`.`flags_extra` including "
                    "CREATURE_FLAG_EXTRA_INSTANCE_BIND (%u) but creature are "
                    "not in instance.",
                    guid, data.id, CREATURE_FLAG_EXTRA_INSTANCE_BIND);
        }

        if (cInfo->flags_extra & CREATURE_FLAG_EXTRA_AGGRO_ZONE)
        {
            if (!mapEntry || !mapEntry->IsDungeon())
                logging.error(
                    "Table `creature` have creature (GUID: %u Entry: %u) with "
                    "`creature_template`.`flags_extra` including "
                    "CREATURE_FLAG_EXTRA_AGGRO_ZONE (%u) but creature are not "
                    "in instance.",
                    guid, data.id, CREATURE_FLAG_EXTRA_AGGRO_ZONE);
        }

        if (data.spawndist < 0.0f)
        {
            logging.error(
                "Table `creature` have creature (GUID: %u Entry: %u) with "
                "`spawndist`< 0, set to 0.",
                guid, data.id);
            data.spawndist = 0.0f;
        }
        else if (data.movementType == (int)movement::gen::random ||
                 data.movementType == (int)movement::gen::random_waterair)
        {
            if (data.spawndist == 0.0f)
            {
                logging.error(
                    "Table `creature` have creature (GUID: %u Entry: %u) with "
                    "`MovementType`=random, but with "
                    "`spawndist`=0, replace by idle movement type.",
                    guid, data.id);
                data.movementType = (int)movement::gen::idle;
            }
        }
        else if (data.movementType == (int)movement::gen::idle)
        {
            if (data.spawndist != 0.0f)
            {
                logging.error(
                    "Table `creature` have creature (GUID: %u Entry: %u) with "
                    "`MovementType`=0 (idle) have `spawndist`<>0, set to 0.",
                    guid, data.id);
                data.spawndist = 0.0f;
            }
        }

        // Add as static entitiy if not managed by game event or pool systems
        if (gameEvent == 0 && GuidPoolId == 0 && EntryPoolId == 0)
            add_static_creature(&data);

        ++count;

    } while (result->NextRow());

    delete result;

    logging.info(
        "Loaded %lu creatures\n", (unsigned long)mCreatureDataMap.size());
}

void ObjectMgr::LoadGameObjects()
{
    uint32 count = 0;

    QueryResult* result = WorldDatabase.Query(
        //      0                1              2    3           4
        "SELECT gameobject.guid, gameobject.id, map, position_x, position_y, "
        // 5         6
        "position_z, orientation,"
        // 7        8          9          10         11
        "rotation0, rotation1, rotation2, rotation3, spawntimesecs, "
        // 12          13     14         15
        "animprogress, state, spawnMask, event,"
        // 16                        17
        "pool_gameobject.pool_entry, pool_gameobject_template.pool_entry "
        "FROM gameobject "
        "LEFT OUTER JOIN game_event_gameobject ON gameobject.guid = "
        "game_event_gameobject.guid "
        "LEFT OUTER JOIN pool_gameobject ON gameobject.guid = "
        "pool_gameobject.guid "
        "LEFT OUTER JOIN pool_gameobject_template ON gameobject.id = "
        "pool_gameobject_template.id");

    if (!result)
    {
        logging.error(
            "Loaded 0 gameobjects. DB table `gameobject` is empty.\n");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 guid = fields[0].GetUInt32();
        uint32 entry = fields[1].GetUInt32();

        GameObjectInfo const* gInfo = GetGameObjectInfo(entry);
        if (!gInfo)
        {
            logging.error(
                "Table `gameobject` has gameobject (GUID: %u) with non "
                "existing gameobject entry %u, skipped.",
                guid, entry);
            continue;
        }

        if (gInfo->displayId &&
            !sGameObjectDisplayInfoStore.LookupEntry(gInfo->displayId))
        {
            logging.error(
                "Gameobject (GUID: %u Entry %u GoType: %u) have invalid "
                "displayId (%u), not loaded.",
                guid, entry, gInfo->type, gInfo->displayId);
            continue;
        }

        GameObjectData& data = mGameObjectDataMap[guid];

        data.id = entry;
        data.guid = guid;
        data.mapid = fields[2].GetUInt32();
        data.posX = fields[3].GetFloat();
        data.posY = fields[4].GetFloat();
        data.posZ = fields[5].GetFloat();
        data.orientation = fields[6].GetFloat();
        data.rotation0 = fields[7].GetFloat();
        data.rotation1 = fields[8].GetFloat();
        data.rotation2 = fields[9].GetFloat();
        data.rotation3 = fields[10].GetFloat();
        data.spawntimesecs = fields[11].GetInt32();
        data.animprogress = fields[12].GetUInt32();
        uint32 go_state = fields[13].GetUInt32();
        data.spawnMask = fields[14].GetUInt8();
        int16 gameEvent = fields[15].GetInt16();
        int16 GuidPoolId = fields[16].GetInt16();
        int16 EntryPoolId = fields[17].GetInt16();

        MapEntry const* mapEntry = sMapStore.LookupEntry(data.mapid);
        if (!mapEntry)
        {
            logging.error(
                "Table `gameobject` have gameobject (GUID: %u Entry: %u) that "
                "spawned at nonexistent map (Id: %u), skip",
                guid, data.id, data.mapid);
            continue;
        }

        if (mapEntry->IsDungeon())
        {
            if (data.spawnMask & ~SPAWNMASK_DUNGEON_ALL)
                logging.error(
                    "Table `gameobject` have gameobject (GUID: %u Entry: %u) "
                    "that have wrong spawn mask %u for dungeon map (Id: %u), "
                    "skip",
                    guid, data.id, data.spawnMask, data.mapid);
        }
        else
        {
            if (data.spawnMask & ~SPAWNMASK_REGULAR)
                logging.error(
                    "Table `gameobject` have gameobject (GUID: %u Entry: %u) "
                    "that have wrong spawn mask %u for non-dungeon map (Id: "
                    "%u), skip",
                    guid, data.id, data.spawnMask, data.mapid);
        }

        if (data.spawntimesecs == 0 && gInfo->IsDespawnAtAction())
        {
            logging.error(
                "Table `gameobject` have gameobject (GUID: %u Entry: %u) with "
                "`spawntimesecs` (0) value, but gameobejct marked as "
                "despawnable at action.",
                guid, data.id);
        }

        if (go_state >= MAX_GO_STATE)
        {
            logging.error(
                "Table `gameobject` have gameobject (GUID: %u Entry: %u) with "
                "invalid `state` (%u) value, skip",
                guid, data.id, go_state);
            continue;
        }
        data.go_state = GOState(go_state);

        if (data.rotation0 < -1.0f || data.rotation0 > 1.0f)
        {
            logging.error(
                "Table `gameobject` have gameobject (GUID: %u Entry: %u) with "
                "invalid rotation0 (%f) value, skip",
                guid, data.id, data.rotation0);
            continue;
        }

        if (data.rotation1 < -1.0f || data.rotation1 > 1.0f)
        {
            logging.error(
                "Table `gameobject` have gameobject (GUID: %u Entry: %u) with "
                "invalid rotation1 (%f) value, skip",
                guid, data.id, data.rotation1);
            continue;
        }

        if (data.rotation2 < -1.0f || data.rotation2 > 1.0f)
        {
            logging.error(
                "Table `gameobject` have gameobject (GUID: %u Entry: %u) with "
                "invalid rotation2 (%f) value, skip",
                guid, data.id, data.rotation2);
            continue;
        }

        if (data.rotation3 < -1.0f || data.rotation3 > 1.0f)
        {
            logging.error(
                "Table `gameobject` have gameobject (GUID: %u Entry: %u) with "
                "invalid rotation3 (%f) value, skip",
                guid, data.id, data.rotation3);
            continue;
        }

        if (!maps::verify_coords(data.posX, data.posY))
        {
            logging.error(
                "Table `gameobject` have gameobject (GUID: %u Entry: %u) with "
                "invalid coordinates, skip",
                guid, data.id);
            continue;
        }

        // Add as static entitiy if not managed by game event or pool systems
        if (gameEvent == 0 && GuidPoolId == 0 && EntryPoolId == 0)
            add_static_game_object(&data);

        ++count;

    } while (result->NextRow());

    delete result;

    logging.info(
        "Loaded %lu gameobjects\n", (unsigned long)mGameObjectDataMap.size());
}

// name must be checked to correctness (if received) before call this function
ObjectGuid ObjectMgr::GetPlayerGuidByName(std::string name) const
{
    ObjectGuid guid;

    CharacterDatabase.escape_string(name);

    // Player name safe to sending to DB (checked at login) and this function
    // using
    std::unique_ptr<QueryResult> result(CharacterDatabase.PQuery(
        "SELECT guid FROM characters WHERE name = '%s'", name.c_str()));
    if (result)
        guid = ObjectGuid(HIGHGUID_PLAYER, (*result)[0].GetUInt32());

    return guid;
}

bool ObjectMgr::GetPlayerNameByGUID(ObjectGuid guid, std::string& name) const
{
    // prevent DB access for online player
    if (Player* player = GetPlayer(guid))
    {
        name = player->GetName();
        return true;
    }

    uint32 lowguid = guid.GetCounter();

    std::unique_ptr<QueryResult> result(CharacterDatabase.PQuery(
        "SELECT name FROM characters WHERE guid = '%u'", lowguid));

    if (result)
    {
        name = (*result)[0].GetCppString();
        return true;
    }

    return false;
}

Team ObjectMgr::GetPlayerTeamByGUID(ObjectGuid guid) const
{
    // prevent DB access for online player
    if (Player* player = GetPlayer(guid))
        return Player::TeamForRace(player->getRace());

    uint32 lowguid = guid.GetCounter();

    std::unique_ptr<QueryResult> result(CharacterDatabase.PQuery(
        "SELECT race FROM characters WHERE guid = '%u'", lowguid));

    if (result)
    {
        uint8 race = (*result)[0].GetUInt8();
        return Player::TeamForRace(race);
    }

    return TEAM_NONE;
}

uint32 ObjectMgr::GetPlayerAccountIdByGUID(ObjectGuid guid) const
{
    if (!guid.IsPlayer())
        return 0;

    // prevent DB access for online player
    if (Player* player = GetPlayer(guid))
        return player->GetSession()->GetAccountId();

    uint32 lowguid = guid.GetCounter();

    std::unique_ptr<QueryResult> result(CharacterDatabase.PQuery(
        "SELECT account FROM characters WHERE guid = '%u'", lowguid));
    if (result)
    {
        uint32 acc = (*result)[0].GetUInt32();
        return acc;
    }

    return 0;
}

uint32 ObjectMgr::GetPlayerAccountIdByPlayerName(const std::string& name) const
{
    std::unique_ptr<QueryResult> result(CharacterDatabase.PQuery(
        "SELECT account FROM characters WHERE name = '%s'", name.c_str()));
    if (result)
    {
        uint32 acc = (*result)[0].GetUInt32();
        return acc;
    }

    return 0;
}

void ObjectMgr::LoadItemLocales()
{
    mItemLocaleMap.clear(); // need for reload case

    QueryResult* result = WorldDatabase.Query(
        "SELECT "
        "entry,name_loc1,description_loc1,name_loc2,description_loc2,name_loc3,"
        "description_loc3,name_loc4,description_loc4,name_loc5,description_"
        "loc5,name_loc6,description_loc6,name_loc7,description_loc7,name_loc8,"
        "description_loc8 FROM locales_item");

    if (!result)
    {
        logging.info(
            "Loaded 0 Item locale strings. DB table `locales_item` is "
            "empty.");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 entry = fields[0].GetUInt32();

        if (!GetItemPrototype(entry))
        {
            logging.warning(
                "Table `locales_item` has data for nonexistent item entry %u, "
                "skipped.",
                entry);
            continue;
        }

        ItemLocale& data = mItemLocaleMap[entry];

        for (int i = 1; i < MAX_LOCALE; ++i)
        {
            std::string str = fields[1 + 2 * (i - 1)].GetCppString();
            if (!str.empty())
            {
                int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    if ((int32)data.Name.size() <= idx)
                        data.Name.resize(idx + 1);

                    data.Name[idx] = str;
                }
            }

            str = fields[1 + 2 * (i - 1) + 1].GetCppString();
            if (!str.empty())
            {
                int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    if ((int32)data.Description.size() <= idx)
                        data.Description.resize(idx + 1);

                    data.Description[idx] = str;
                }
            }
        }
    } while (result->NextRow());

    delete result;

    logging.info(
        "Loaded %lu Item locale strings", (unsigned long)mItemLocaleMap.size());
}

struct SQLItemLoader : public SQLStorageLoaderBase<SQLItemLoader>
{
    template <class D>
    void convert_from_str(uint32 /*field_pos*/, char const* src, D& dst)
    {
        dst = D(sScriptMgr::Instance()->GetScriptId(src));
    }
};

void ObjectMgr::LoadItemPrototypes()
{
    SQLItemLoader loader;
    loader.Load(sItemStorage);
    logging.info("Loaded %u item prototypes\n", sItemStorage.RecordCount);

    // check data correctness
    for (uint32 i = 1; i < sItemStorage.MaxEntry; ++i)
    {
        ItemPrototype const* proto = sItemStorage.LookupEntry<ItemPrototype>(i);
        ItemEntry const* dbcitem = sItemStore.LookupEntry(i);
        if (!proto)
        {
            /* to many errors, and possible not all items really used in game
            if (dbcitem)
                logging.error("Item (Entry: %u) doesn't exists in
            DB, but must exist.",i);
            */
            continue;
        }

        if (dbcitem)
        {
            if (proto->InventoryType != dbcitem->InventoryType)
            {
                logging.error(
                    "Item (Entry: %u) not correct %u inventory type, must be "
                    "%u (still using DB value).",
                    i, proto->InventoryType, dbcitem->InventoryType);
                // It safe let use InventoryType from DB
            }

            if (proto->DisplayInfoID != dbcitem->DisplayId)
            {
                logging.error(
                    "Item (Entry: %u) not correct %u display id, must be %u "
                    "(using it).",
                    i, proto->DisplayInfoID, dbcitem->DisplayId);
                const_cast<ItemPrototype*>(proto)->DisplayInfoID =
                    dbcitem->DisplayId;
            }
            if (proto->Sheath != dbcitem->Sheath)
            {
                logging.error(
                    "Item (Entry: %u) not correct %u sheath, must be %u  "
                    "(using it).",
                    i, proto->Sheath, dbcitem->Sheath);
                const_cast<ItemPrototype*>(proto)->Sheath = dbcitem->Sheath;
            }
        }
        else
        {
            logging.error(
                "Item (Entry: %u) not correct (not listed in list of existing "
                "items).",
                i);
        }

        if (proto->Class >= MAX_ITEM_CLASS)
        {
            logging.error(
                "Item (Entry: %u) has wrong Class value (%u)", i, proto->Class);
            const_cast<ItemPrototype*>(proto)->Class = ITEM_CLASS_MISC;
        }

        if (proto->SubClass >= MaxItemSubclassValues[proto->Class])
        {
            logging.error(
                "Item (Entry: %u) has wrong Subclass value (%u) for class %u",
                i, proto->SubClass, proto->Class);
            const_cast<ItemPrototype*>(proto)->SubClass =
                0; // exist for all item classes
        }

        if (proto->Quality >= MAX_ITEM_QUALITY)
        {
            logging.error("Item (Entry: %u) has wrong Quality value (%u)", i,
                proto->Quality);
            const_cast<ItemPrototype*>(proto)->Quality = ITEM_QUALITY_NORMAL;
        }

        if (proto->BuyCount <= 0)
        {
            logging.error(
                "Item (Entry: %u) has wrong BuyCount value (%u), set to "
                "default(1).",
                i, proto->BuyCount);
            const_cast<ItemPrototype*>(proto)->BuyCount = 1;
        }

        if (proto->InventoryType >= MAX_INVTYPE)
        {
            logging.error("Item (Entry: %u) has wrong InventoryType value (%u)",
                i, proto->InventoryType);
            const_cast<ItemPrototype*>(proto)->InventoryType =
                INVTYPE_NON_EQUIP;
        }

        if (proto->InventoryType != INVTYPE_NON_EQUIP)
        {
            if (proto->Flags & ITEM_FLAG_LOOTABLE)
            {
                logging.error(
                    "Item container (Entry: %u) has not allowed for containers "
                    "flag ITEM_FLAG_LOOTABLE (%u), flag removed.",
                    i, ITEM_FLAG_LOOTABLE);
                const_cast<ItemPrototype*>(proto)->Flags &= ~ITEM_FLAG_LOOTABLE;
            }

            if (proto->Flags & ITEM_FLAG_PROSPECTABLE)
            {
                logging.error(
                    "Item container (Entry: %u) has not allowed for containers "
                    "flag ITEM_FLAG_PROSPECTABLE (%u), flag removed.",
                    i, ITEM_FLAG_PROSPECTABLE);
                const_cast<ItemPrototype*>(proto)->Flags &=
                    ~ITEM_FLAG_PROSPECTABLE;
            }
        }
        else if (proto->InventoryType != INVTYPE_BAG)
        {
            if (proto->ContainerSlots > 0)
            {
                logging.error(
                    "Non-container item (Entry: %u) has ContainerSlots (%u), "
                    "set to 0.",
                    i, proto->ContainerSlots);
                const_cast<ItemPrototype*>(proto)->ContainerSlots = 0;
            }
        }

        if (proto->RequiredSkill >= MAX_SKILL_TYPE)
        {
            logging.error("Item (Entry: %u) has wrong RequiredSkill value (%u)",
                i, proto->RequiredSkill);
            const_cast<ItemPrototype*>(proto)->RequiredSkill = 0;
        }

        {
            // can be used in equip slot, as page read use in inventory, or
            // spell casting at use
            bool req =
                proto->InventoryType != INVTYPE_NON_EQUIP || proto->PageText;
            if (!req)
            {
                for (int j = 0; j < MAX_ITEM_PROTO_SPELLS; ++j)
                {
                    if (proto->Spells[j].SpellId)
                    {
                        req = true;
                        break;
                    }
                }
            }

            if (req)
            {
                if (!(proto->AllowableClass & CLASSMASK_ALL_PLAYABLE))
                    logging.error(
                        "Item (Entry: %u) not have in `AllowableClass` any "
                        "playable classes (%u) and can't be equipped or use.",
                        i, proto->AllowableClass);

                if (!(proto->AllowableRace & RACEMASK_ALL_PLAYABLE))
                    logging.error(
                        "Item (Entry: %u) not have in `AllowableRace` any "
                        "playable races (%u) and can't be equipped or use.",
                        i, proto->AllowableRace);
            }
        }

        if (proto->RequiredSpell &&
            !sSpellStore.LookupEntry(proto->RequiredSpell))
        {
            logging.error(
                "Item (Entry: %u) have wrong (nonexistent) spell in "
                "RequiredSpell (%u)",
                i, proto->RequiredSpell);
            const_cast<ItemPrototype*>(proto)->RequiredSpell = 0;
        }

        if (proto->RequiredReputationRank >= MAX_REPUTATION_RANK)
            logging.error(
                "Item (Entry: %u) has wrong reputation rank in "
                "RequiredReputationRank (%u), item can't be used.",
                i, proto->RequiredReputationRank);

        if (proto->RequiredReputationFaction)
        {
            if (!sFactionStore.LookupEntry(proto->RequiredReputationFaction))
            {
                logging.error(
                    "Item (Entry: %u) has wrong (not existing) faction in "
                    "RequiredReputationFaction (%u)",
                    i, proto->RequiredReputationFaction);
                const_cast<ItemPrototype*>(proto)->RequiredReputationFaction =
                    0;
            }

            if (proto->RequiredReputationRank == MIN_REPUTATION_RANK)
                logging.error(
                    "Item (Entry: %u) has min. reputation rank in "
                    "RequiredReputationRank (0) but RequiredReputationFaction "
                    "> 0, faction setting is useless.",
                    i);
        }
        else if (proto->RequiredReputationRank > MIN_REPUTATION_RANK)
            logging.error(
                "Item (Entry: %u) has RequiredReputationFaction ==0 but "
                "RequiredReputationRank > 0, rank setting is useless.",
                i);

        if (proto->Stackable == 0)
        {
            logging.error(
                "Item (Entry: %u) has wrong value in stackable (%u), replace "
                "by default 1.",
                i, proto->Stackable);
            const_cast<ItemPrototype*>(proto)->Stackable = 1;
        }
        else if (proto->Stackable > 255)
        {
            logging.error(
                "Item (Entry: %u) has too large value in stackable (%u), "
                "replace by hardcoded upper limit (255).",
                i, proto->Stackable);
            const_cast<ItemPrototype*>(proto)->Stackable = 255;
        }

        /*XXX:*/
        if (proto->ContainerSlots)
        {
            if (proto->ContainerSlots > inventory::max_bag_size)
            {
                logging.error(
                    "Item (Entry: %u) has too large value in ContainerSlots "
                    "(%u), replace by hardcoded limit (%u).",
                    i, proto->ContainerSlots, inventory::max_bag_size);
                const_cast<ItemPrototype*>(proto)->ContainerSlots =
                    inventory::max_bag_size;
            }
        }

        for (int j = 0; j < MAX_ITEM_PROTO_STATS; ++j)
        {
            // for ItemStatValue != 0
            if (proto->ItemStat[j].ItemStatValue &&
                proto->ItemStat[j].ItemStatType >= MAX_ITEM_MOD)
            {
                logging.error("Item (Entry: %u) has wrong stat_type%d (%u)", i,
                    j + 1, proto->ItemStat[j].ItemStatType);
                const_cast<ItemPrototype*>(proto)->ItemStat[j].ItemStatType = 0;
            }
        }

        for (int j = 0; j < MAX_ITEM_PROTO_DAMAGES; ++j)
        {
            if (proto->Damage[j].DamageType >= MAX_SPELL_SCHOOL)
            {
                logging.error("Item (Entry: %u) has wrong dmg_type%d (%u)", i,
                    j + 1, proto->Damage[j].DamageType);
                const_cast<ItemPrototype*>(proto)->Damage[j].DamageType = 0;
            }
        }

        // special format
        if (proto->Spells[0].SpellId == SPELL_ID_GENERIC_LEARN)
        {
            // spell_1
            if (proto->Spells[0].SpellTrigger != ITEM_SPELLTRIGGER_ON_USE)
            {
                logging.error(
                    "Item (Entry: %u) has wrong item spell trigger value in "
                    "spelltrigger_%d (%u) for special learning format",
                    i, 0 + 1, proto->Spells[0].SpellTrigger);
                const_cast<ItemPrototype*>(proto)->Spells[0].SpellId = 0;
                const_cast<ItemPrototype*>(proto)->Spells[0].SpellTrigger =
                    ITEM_SPELLTRIGGER_ON_USE;
                const_cast<ItemPrototype*>(proto)->Spells[1].SpellId = 0;
                const_cast<ItemPrototype*>(proto)->Spells[1].SpellTrigger =
                    ITEM_SPELLTRIGGER_ON_USE;
            }

            // spell_2 have learning spell
            if (proto->Spells[1].SpellTrigger !=
                ITEM_SPELLTRIGGER_LEARN_SPELL_ID)
            {
                logging.error(
                    "Item (Entry: %u) has wrong item spell trigger value in "
                    "spelltrigger_%d (%u) for special learning format.",
                    i, 1 + 1, proto->Spells[1].SpellTrigger);
                const_cast<ItemPrototype*>(proto)->Spells[0].SpellId = 0;
                const_cast<ItemPrototype*>(proto)->Spells[1].SpellId = 0;
                const_cast<ItemPrototype*>(proto)->Spells[1].SpellTrigger =
                    ITEM_SPELLTRIGGER_ON_USE;
            }
            else if (!proto->Spells[1].SpellId)
            {
                logging.error(
                    "Item (Entry: %u) not has expected spell in spellid_%d in "
                    "special learning format.",
                    i, 1 + 1);
                const_cast<ItemPrototype*>(proto)->Spells[0].SpellId = 0;
                const_cast<ItemPrototype*>(proto)->Spells[1].SpellTrigger =
                    ITEM_SPELLTRIGGER_ON_USE;
            }
            else
            {
                SpellEntry const* spellInfo =
                    sSpellStore.LookupEntry(proto->Spells[1].SpellId);
                if (!spellInfo)
                {
                    logging.error(
                        "Item (Entry: %u) has wrong (not existing) spell in "
                        "spellid_%d (%u)",
                        i, 1 + 1, proto->Spells[1].SpellId);
                    const_cast<ItemPrototype*>(proto)->Spells[0].SpellId = 0;
                    const_cast<ItemPrototype*>(proto)->Spells[1].SpellId = 0;
                    const_cast<ItemPrototype*>(proto)->Spells[1].SpellTrigger =
                        ITEM_SPELLTRIGGER_ON_USE;
                }
                // allowed only in special format
                else if (proto->Spells[1].SpellId == SPELL_ID_GENERIC_LEARN)
                {
                    logging.error(
                        "Item (Entry: %u) has broken spell in spellid_%d (%u)",
                        i, 1 + 1, proto->Spells[1].SpellId);
                    const_cast<ItemPrototype*>(proto)->Spells[0].SpellId = 0;
                    const_cast<ItemPrototype*>(proto)->Spells[1].SpellId = 0;
                    const_cast<ItemPrototype*>(proto)->Spells[1].SpellTrigger =
                        ITEM_SPELLTRIGGER_ON_USE;
                }
            }

            // spell_3*,spell_4*,spell_5* is empty
            for (int j = 2; j < MAX_ITEM_PROTO_SPELLS; ++j)
            {
                if (proto->Spells[j].SpellTrigger != ITEM_SPELLTRIGGER_ON_USE)
                {
                    logging.error(
                        "Item (Entry: %u) has wrong item spell trigger value "
                        "in spelltrigger_%d (%u)",
                        i, j + 1, proto->Spells[j].SpellTrigger);
                    const_cast<ItemPrototype*>(proto)->Spells[j].SpellId = 0;
                    const_cast<ItemPrototype*>(proto)->Spells[j].SpellTrigger =
                        ITEM_SPELLTRIGGER_ON_USE;
                }
                else if (proto->Spells[j].SpellId != 0)
                {
                    logging.error(
                        "Item (Entry: %u) has wrong spell in spellid_%d (%u) "
                        "for learning special format",
                        i, j + 1, proto->Spells[j].SpellId);
                    const_cast<ItemPrototype*>(proto)->Spells[j].SpellId = 0;
                }
            }
        }
        // normal spell list
        else
        {
            for (int j = 0; j < MAX_ITEM_PROTO_SPELLS; ++j)
            {
                if (proto->Spells[j].SpellTrigger >= MAX_ITEM_SPELLTRIGGER ||
                    proto->Spells[j].SpellTrigger ==
                        ITEM_SPELLTRIGGER_LEARN_SPELL_ID)
                {
                    logging.error(
                        "Item (Entry: %u) has wrong item spell trigger value "
                        "in spelltrigger_%d (%u)",
                        i, j + 1, proto->Spells[j].SpellTrigger);
                    const_cast<ItemPrototype*>(proto)->Spells[j].SpellId = 0;
                    const_cast<ItemPrototype*>(proto)->Spells[j].SpellTrigger =
                        ITEM_SPELLTRIGGER_ON_USE;
                }
                // on hit can be sued only at weapon
                else if (proto->Spells[j].SpellTrigger ==
                         ITEM_SPELLTRIGGER_CHANCE_ON_HIT)
                {
                    if (proto->Class != ITEM_CLASS_WEAPON)
                        logging.error(
                            "Item (Entry: %u) isn't weapon (Class: %u) but has "
                            "on hit spelltrigger_%d (%u), it will not "
                            "triggered.",
                            i, proto->Class, j + 1,
                            proto->Spells[j].SpellTrigger);
                }

                if (proto->Spells[j].SpellId)
                {
                    SpellEntry const* spellInfo =
                        sSpellStore.LookupEntry(proto->Spells[j].SpellId);
                    if (!spellInfo)
                    {
                        logging.error(
                            "Item (Entry: %u) has wrong (not existing) spell "
                            "in spellid_%d (%u)",
                            i, j + 1, proto->Spells[j].SpellId);
                        const_cast<ItemPrototype*>(proto)->Spells[j].SpellId =
                            0;
                    }
                    // allowed only in special format
                    else if (proto->Spells[j].SpellId == SPELL_ID_GENERIC_LEARN)
                    {
                        logging.error(
                            "Item (Entry: %u) has broken spell in spellid_%d "
                            "(%u)",
                            i, j + 1, proto->Spells[j].SpellId);
                        const_cast<ItemPrototype*>(proto)->Spells[j].SpellId =
                            0;
                    }
                }
            }
        }

        if (proto->Bonding >= MAX_BIND_TYPE)
            logging.error("Item (Entry: %u) has wrong Bonding value (%u)", i,
                proto->Bonding);

        if (proto->PageText)
        {
            if (!sPageTextStore.LookupEntry<PageText>(proto->PageText))
                logging.error(
                    "Item (Entry: %u) has non existing first page (Id:%u)", i,
                    proto->PageText);
        }

        if (proto->LockID && !sLockStore.LookupEntry(proto->LockID))
            logging.error(
                "Item (Entry: %u) has wrong LockID (%u)", i, proto->LockID);

        if (proto->Sheath >= MAX_SHEATHETYPE)
        {
            logging.error(
                "Item (Entry: %u) has wrong Sheath (%u)", i, proto->Sheath);
            const_cast<ItemPrototype*>(proto)->Sheath = SHEATHETYPE_NONE;
        }

        if (proto->RandomProperty &&
            !sItemRandomPropertiesStore.LookupEntry(
                GetItemEnchantMod(proto->RandomProperty)))
        {
            logging.error(
                "Item (Entry: %u) has unknown (wrong or not listed in "
                "`item_enchantment_template`) RandomProperty (%u)",
                i, proto->RandomProperty);
            const_cast<ItemPrototype*>(proto)->RandomProperty = 0;
        }

        if (proto->RandomSuffix &&
            !sItemRandomSuffixStore.LookupEntry(
                GetItemEnchantMod(proto->RandomSuffix)))
        {
            logging.error("Item (Entry: %u) has wrong RandomSuffix (%u)", i,
                proto->RandomSuffix);
            const_cast<ItemPrototype*>(proto)->RandomSuffix = 0;
        }

        // item can have not null only one from field values
        if (proto->RandomProperty && proto->RandomSuffix)
        {
            logging.error(
                "Item (Entry: %u) have RandomProperty==%u and "
                "RandomSuffix==%u, but must have one from field = 0",
                proto->ItemId, proto->RandomProperty, proto->RandomSuffix);
            const_cast<ItemPrototype*>(proto)->RandomSuffix = 0;
        }

        if (proto->ItemSet && !sItemSetStore.LookupEntry(proto->ItemSet))
        {
            logging.error(
                "Item (Entry: %u) have wrong ItemSet (%u)", i, proto->ItemSet);
            const_cast<ItemPrototype*>(proto)->ItemSet = 0;
        }

        if (proto->Area && !GetAreaEntryByAreaID(proto->Area))
            logging.error(
                "Item (Entry: %u) has wrong Area (%u)", i, proto->Area);

        if (proto->Map && !sMapStore.LookupEntry(proto->Map))
            logging.error("Item (Entry: %u) has wrong Map (%u)", i, proto->Map);

        if (proto->BagFamily)
        {
            // check bits
            for (uint32 j = 0; j < sizeof(proto->BagFamily) * 8; ++j)
            {
                uint32 mask = 1 << j;
                if (!(proto->BagFamily & mask))
                    continue;

                ItemBagFamilyEntry const* bf =
                    sItemBagFamilyStore.LookupEntry(j + 1);
                if (!bf)
                {
                    logging.error(
                        "Item (Entry: %u) has bag family bit set not listed in "
                        "ItemBagFamily.dbc, remove bit",
                        i);
                    const_cast<ItemPrototype*>(proto)->BagFamily &= ~mask;
                }
            }
        }

        if (proto->TotemCategory &&
            !sTotemCategoryStore.LookupEntry(proto->TotemCategory))
            logging.error("Item (Entry: %u) has wrong TotemCategory (%u)", i,
                proto->TotemCategory);

        for (int j = 0; j < MAX_ITEM_PROTO_SOCKETS; ++j)
        {
            if (proto->Socket[j].Color &&
                (proto->Socket[j].Color & SOCKET_COLOR_ALL) !=
                    proto->Socket[j].Color)
            {
                logging.error("Item (Entry: %u) has wrong socketColor_%d (%u)",
                    i, j + 1, proto->Socket[j].Color);
                const_cast<ItemPrototype*>(proto)->Socket[j].Color = 0;
            }
        }

        if (proto->GemProperties &&
            !sGemPropertiesStore.LookupEntry(proto->GemProperties))
            logging.error("Item (Entry: %u) has wrong GemProperties (%u)", i,
                proto->GemProperties);

        if (proto->RequiredDisenchantSkill < -1)
        {
            logging.error(
                "Item (Entry: %u) has wrong RequiredDisenchantSkill (%i), set "
                "to (-1).",
                i, proto->RequiredDisenchantSkill);
            const_cast<ItemPrototype*>(proto)->RequiredDisenchantSkill = -1;
        }
        else if (proto->RequiredDisenchantSkill != -1)
        {
            if (proto->Quality > ITEM_QUALITY_EPIC ||
                proto->Quality < ITEM_QUALITY_UNCOMMON)
            {
                logging.warning(
                    "Item (Entry: %u) has unexpected RequiredDisenchantSkill "
                    "(%u) for non-disenchantable quality (%u), reset it.",
                    i, proto->RequiredDisenchantSkill, proto->Quality);
                const_cast<ItemPrototype*>(proto)->RequiredDisenchantSkill = -1;
            }
            else if (proto->Class != ITEM_CLASS_WEAPON &&
                     proto->Class != ITEM_CLASS_ARMOR)
            {
                // some wrong data in wdb for unused items
                logging.warning(
                    "Item (Entry: %u) has unexpected RequiredDisenchantSkill "
                    "(%u) for non-disenchantable item class (%u), reset it.",
                    i, proto->RequiredDisenchantSkill, proto->Class);
                const_cast<ItemPrototype*>(proto)->RequiredDisenchantSkill = -1;
            }
        }

        if (proto->DisenchantID)
        {
            if (proto->Quality > ITEM_QUALITY_EPIC ||
                proto->Quality < ITEM_QUALITY_UNCOMMON)
            {
                logging.error(
                    "Item (Entry: %u) has wrong quality (%u) for "
                    "disenchanting, remove disenchanting loot id.",
                    i, proto->Quality);
                const_cast<ItemPrototype*>(proto)->DisenchantID = 0;
            }
            else if (proto->Class != ITEM_CLASS_WEAPON &&
                     proto->Class != ITEM_CLASS_ARMOR)
            {
                logging.error(
                    "Item (Entry: %u) has wrong item class (%u) for "
                    "disenchanting, remove disenchanting loot id.",
                    i, proto->Class);
                const_cast<ItemPrototype*>(proto)->DisenchantID = 0;
            }
            else if (proto->RequiredDisenchantSkill < 0)
            {
                logging.error(
                    "Item (Entry: %u) marked as non-disenchantable by "
                    "RequiredDisenchantSkill == -1, remove disenchanting loot "
                    "id.",
                    i);
                const_cast<ItemPrototype*>(proto)->DisenchantID = 0;
            }
        }
        else
        {
            // lot DB cases
            if (proto->RequiredDisenchantSkill >= 0)
                logging.warning(
                    "Item (Entry: %u) marked as disenchantable by "
                    "RequiredDisenchantSkill, but not have disenchanting loot "
                    "id.",
                    i);
        }

        if (proto->FoodType >= MAX_PET_DIET)
        {
            logging.error("Item (Entry: %u) has wrong FoodType value (%u)", i,
                proto->FoodType);
            const_cast<ItemPrototype*>(proto)->FoodType = 0;
        }

        if (proto->ExtraFlags)
        {
            if (proto->ExtraFlags & ~ITEM_EXTRA_ALL)
                logging.error(
                    "Item (Entry: %u) has wrong ExtraFlags (%u) with unused "
                    "bits set",
                    i, proto->ExtraFlags);

            if (proto->ExtraFlags & ITEM_EXTRA_NON_CONSUMABLE)
            {
                bool can_be_need = false;
                for (int j = 0; j < MAX_ITEM_PROTO_SPELLS; ++j)
                {
                    if (proto->Spells[j].SpellCharges < 0)
                    {
                        can_be_need = true;
                        break;
                    }
                }

                if (!can_be_need)
                {
                    logging.error(
                        "Item (Entry: %u) has redundant non-consumable flag in "
                        "ExtraFlags, item not have negative charges",
                        i);
                    const_cast<ItemPrototype*>(proto)->ExtraFlags &=
                        ~ITEM_EXTRA_NON_CONSUMABLE;
                }
            }

            if (proto->ExtraFlags & ITEM_EXTRA_REAL_TIME_DURATION)
            {
                if (proto->Duration == 0)
                {
                    logging.error(
                        "Item (Entry: %u) has redundant real-time duration "
                        "flag in ExtraFlags, item not have duration",
                        i);
                    const_cast<ItemPrototype*>(proto)->ExtraFlags &=
                        ~ITEM_EXTRA_REAL_TIME_DURATION;
                }
            }
        }
    }

    // check some dbc referenced items (avoid duplicate reports)
    std::set<uint32> notFoundOutfit;
    for (uint32 i = 1; i < sCharStartOutfitStore.GetNumRows(); ++i)
    {
        CharStartOutfitEntry const* entry =
            sCharStartOutfitStore.LookupEntry(i);
        if (!entry)
            continue;

        for (int j = 0; j < MAX_OUTFIT_ITEMS; ++j)
        {
            if (entry->ItemId[j] <= 0)
                continue;

            uint32 item_id = entry->ItemId[j];

            if (!GetItemPrototype(item_id))
                notFoundOutfit.insert(item_id);
        }
    }

    for (const auto& elem : notFoundOutfit)
        logging.error(
            "Item (Entry: %u) not exist in `item_template` but referenced in "
            "`CharStartOutfit.dbc`",
            elem);
}

void ObjectMgr::LoadItemRequiredTarget()
{
    m_ItemRequiredTarget.clear(); // needed for reload case

    uint32 count = 0;

    QueryResult* result = WorldDatabase.Query(
        "SELECT entry,type,targetEntry FROM item_required_target");

    if (!result)
    {
        logging.error(
            "Loaded 0 ItemRequiredTarget. DB table `item_required_target` "
            "is empty.\n");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 uiItemId = fields[0].GetUInt32();
        uint32 uiType = fields[1].GetUInt32();
        uint32 uiTargetEntry = fields[2].GetUInt32();

        ItemPrototype const* pItemProto =
            sItemStorage.LookupEntry<ItemPrototype>(uiItemId);

        if (!pItemProto)
        {
            logging.error(
                "Table `item_required_target`: Entry %u listed for TargetEntry "
                "%u does not exist in `item_template`.",
                uiItemId, uiTargetEntry);
            continue;
        }

        bool bIsItemSpellValid = false;

        for (int i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
        {
            if (SpellEntry const* pSpellInfo =
                    sSpellStore.LookupEntry(pItemProto->Spells[i].SpellId))
            {
                if (pItemProto->Spells[i].SpellTrigger ==
                    ITEM_SPELLTRIGGER_ON_USE)
                {
                    SpellScriptTargetBounds bounds =
                        sSpellMgr::Instance()->GetSpellScriptTargetBounds(
                            pSpellInfo->Id);
                    if (bounds.first != bounds.second)
                        break;

                    for (int j = 0; j < MAX_EFFECT_INDEX; ++j)
                    {
                        if (pSpellInfo->EffectImplicitTargetA[j] ==
                                TARGET_CHAIN_DAMAGE ||
                            pSpellInfo->EffectImplicitTargetB[j] ==
                                TARGET_CHAIN_DAMAGE ||
                            pSpellInfo->EffectImplicitTargetA[j] ==
                                TARGET_DUELVSPLAYER ||
                            pSpellInfo->EffectImplicitTargetB[j] ==
                                TARGET_DUELVSPLAYER)
                        {
                            bIsItemSpellValid = true;
                            break;
                        }
                    }
                    if (bIsItemSpellValid)
                        break;
                }
            }
        }

        if (!bIsItemSpellValid)
        {
            logging.error(
                "Table `item_required_target`: Spell used by item %u does not "
                "have implicit target TARGET_CHAIN_DAMAGE(6), "
                "TARGET_DUELVSPLAYER(25), already listed in "
                "`spell_script_target` or doesn't have item spelltrigger.",
                uiItemId);
            continue;
        }

        if (!uiType || uiType > MAX_ITEM_REQ_TARGET_TYPE)
        {
            logging.error(
                "Table `item_required_target`: Type %u for TargetEntry %u is "
                "incorrect.",
                uiType, uiTargetEntry);
            continue;
        }

        if (!uiTargetEntry)
        {
            logging.error(
                "Table `item_required_target`: TargetEntry == 0 for Type (%u).",
                uiType);
            continue;
        }

        if (!sCreatureStorage.LookupEntry<CreatureInfo>(uiTargetEntry))
        {
            logging.error(
                "Table `item_required_target`: creature template entry %u does "
                "not exist.",
                uiTargetEntry);
            continue;
        }

        m_ItemRequiredTarget.insert(ItemRequiredTargetMap::value_type(uiItemId,
            ItemRequiredTarget(ItemRequiredTargetType(uiType), uiTargetEntry)));

        ++count;
    } while (result->NextRow());

    delete result;

    logging.info("Loaded %u Item required targets\n", count);
}

void ObjectMgr::LoadPetLevelInfo()
{
    // Loading levels data
    {
        //                                                 0               1
        //                                                 2   3     4    5    6
        //                                                 7     8    9
        QueryResult* result = WorldDatabase.Query(
            "SELECT creature_entry, level, hp, mana, str, agi, sta, inte, spi, "
            "armor FROM pet_levelstats");

        uint32 count = 0;

        if (!result)
        {
            logging.info("Loaded %u level pet stats definitions\n", count);
            logging.error(
                "Error loading `pet_levelstats` table or empty table.");
            return;
        }

        BarGoLink bar(result->GetRowCount());

        do
        {
            Field* fields = result->Fetch();

            uint32 creature_id = fields[0].GetUInt32();
            if (!sCreatureStorage.LookupEntry<CreatureInfo>(creature_id))
            {
                logging.error(
                    "Wrong creature id %u in `pet_levelstats` table, ignoring.",
                    creature_id);
                continue;
            }

            uint32 current_level = fields[1].GetUInt32();
            if (current_level >
                sWorld::Instance()->getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
            {
                if (current_level > STRONG_MAX_LEVEL) // hardcoded level maximum
                    logging.error(
                        "Wrong (> %u) level %u in `pet_levelstats` table, "
                        "ignoring.",
                        STRONG_MAX_LEVEL, current_level);
                else
                {
                    LOG_DEBUG(logging,
                        "Unused (> MaxPlayerLevel in mangosd.conf) level %u in "
                        "`pet_levelstats` table, ignoring.",
                        current_level);
                    ++count; // make result loading percent "expected" correct
                             // in case disabled detail mode for example.
                }
                continue;
            }
            else if (current_level < 1)
            {
                logging.error(
                    "Wrong (<1) level %u in `pet_levelstats` table, ignoring.",
                    current_level);
                continue;
            }

            PetLevelInfo*& pInfoMapEntry = petInfo[creature_id];

            if (pInfoMapEntry == nullptr)
                pInfoMapEntry = new PetLevelInfo[sWorld::Instance()->getConfig(
                    CONFIG_UINT32_MAX_PLAYER_LEVEL)];

            // data for level 1 stored in [0] array element, ...
            PetLevelInfo* pLevelInfo = &pInfoMapEntry[current_level - 1];

            pLevelInfo->health = fields[2].GetUInt16();
            pLevelInfo->mana = fields[3].GetUInt16();
            pLevelInfo->armor = fields[9].GetUInt16();

            for (int i = 0; i < MAX_STATS; i++)
            {
                pLevelInfo->stats[i] = fields[i + 4].GetUInt16();
            }

            bar.step();
            ++count;
        } while (result->NextRow());

        delete result;

        logging.info("Loaded %u level pet stats definitions\n", count);
    }

    // Fill gaps and check integrity
    for (auto& elem : petInfo)
    {
        PetLevelInfo* pInfo = elem.second;

        // fatal error if no level 1 data
        if (!pInfo || pInfo[0].health == 0)
        {
            logging.error(
                "Creature %u does not have pet stats data for Level 1!",
                elem.first);

            exit(1);
        }

        // fill level gaps
        for (uint32 level = 1; level < sWorld::Instance()->getConfig(
                                           CONFIG_UINT32_MAX_PLAYER_LEVEL);
             ++level)
        {
            if (pInfo[level].health == 0)
            {
                logging.error(
                    "Creature %u has no data for Level %i pet stats data, "
                    "using data of Level %i.",
                    elem.first, level + 1, level);
                pInfo[level] = pInfo[level - 1];
            }
        }
    }
}

PetLevelInfo const* ObjectMgr::GetPetLevelInfo(
    uint32 creature_id, uint32 level) const
{
    if (level > sWorld::Instance()->getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
        level = sWorld::Instance()->getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL);

    auto itr = petInfo.find(creature_id);
    if (itr == petInfo.end())
        return nullptr;

    return &itr->second[level -
                        1]; // data for level 1 stored in [0] array element, ...
}

void ObjectMgr::LoadPetScaling()
{
    std::unique_ptr<QueryResult> result(
        WorldDatabase.Query("SELECT `cid`, `sta`, `int` FROM pet_scaling"));
    if (!result)
    {
        logging.info("Loaded 0 pet scaling entries\n");
        logging.error("Error loading `pet_scaling` table, or it's empty.");
        return;
    }

    BarGoLink bar(result->GetRowCount());
    do
    {
        Field* fields = result->Fetch();
        pet_scaling_entry e;

        e.stamina = fields[1].GetFloat();
        e.intellect = fields[2].GetFloat();
        pet_scaling_[fields[0].GetUInt32()] = e;

        bar.step();
    } while (result->NextRow());

    logging.info(
        "Loaded " UI64FMTD " pet scaling entries\n", result->GetRowCount());
}

void ObjectMgr::LoadPlayerInfo()
{
    // Load playercreate
    {
        //                                                0     1      2    3
        //                                                4           5 6
        QueryResult* result = WorldDatabase.Query(
            "SELECT race, class, map, zone, position_x, position_y, "
            "position_z, orientation FROM playercreateinfo");

        uint32 count = 0;

        if (!result)
        {
            logging.info("Loaded %u player create definitions\n", count);
            logging.error(
                "Error loading `playercreateinfo` table or empty table.");

            exit(1);
        }

        BarGoLink bar(result->GetRowCount());

        do
        {
            Field* fields = result->Fetch();

            uint32 current_race = fields[0].GetUInt32();
            uint32 current_class = fields[1].GetUInt32();
            uint32 mapId = fields[2].GetUInt32();
            uint32 areaId = fields[3].GetUInt32();
            float positionX = fields[4].GetFloat();
            float positionY = fields[5].GetFloat();
            float positionZ = fields[6].GetFloat();
            float orientation = fields[7].GetFloat();

            ChrRacesEntry const* rEntry =
                sChrRacesStore.LookupEntry(current_race);
            if (!rEntry || !((1 << (current_race - 1)) & RACEMASK_ALL_PLAYABLE))
            {
                logging.error(
                    "Wrong race %u in `playercreateinfo` table, ignoring.",
                    current_race);
                continue;
            }

            ChrClassesEntry const* cEntry =
                sChrClassesStore.LookupEntry(current_class);
            if (!cEntry ||
                !((1 << (current_class - 1)) & CLASSMASK_ALL_PLAYABLE))
            {
                logging.error(
                    "Wrong class %u in `playercreateinfo` table, ignoring.",
                    current_class);
                continue;
            }

            // accept DB data only for valid position (and non instanceable)
            if (!maps::verify_coords(positionX, positionY))
            {
                logging.error(
                    "Wrong home position for class %u race %u pair in "
                    "`playercreateinfo` table, ignoring.",
                    current_class, current_race);
                continue;
            }

            if (sMapStore.LookupEntry(mapId)->Instanceable())
            {
                logging.error(
                    "Home position in instanceable map for class %u race %u "
                    "pair in `playercreateinfo` table, ignoring.",
                    current_class, current_race);
                continue;
            }

            PlayerInfo* pInfo = &playerInfo[current_race][current_class];

            pInfo->mapId = mapId;
            pInfo->areaId = areaId;
            pInfo->positionX = positionX;
            pInfo->positionY = positionY;
            pInfo->positionZ = positionZ;
            pInfo->orientation = orientation;

            pInfo->displayId_m = rEntry->model_m;
            pInfo->displayId_f = rEntry->model_f;

            bar.step();
            ++count;
        } while (result->NextRow());

        delete result;

        logging.info("Loaded %u player create definitions\n", count);
    }

    // Load playercreate items
    {
        //                                                0     1      2       3
        QueryResult* result = WorldDatabase.Query(
            "SELECT race, class, itemid, amount FROM playercreateinfo_item");

        uint32 count = 0;

        if (!result)
        {
            logging.info("Loaded %u custom player create items\n", count);
        }
        else
        {
            BarGoLink bar(result->GetRowCount());

            do
            {
                Field* fields = result->Fetch();

                uint32 current_race = fields[0].GetUInt32();
                uint32 current_class = fields[1].GetUInt32();

                ChrRacesEntry const* rEntry =
                    sChrRacesStore.LookupEntry(current_race);
                if (!rEntry ||
                    !((1 << (current_race - 1)) & RACEMASK_ALL_PLAYABLE))
                {
                    logging.error(
                        "Wrong race %u in `playercreateinfo_item` table, "
                        "ignoring.",
                        current_race);
                    continue;
                }

                ChrClassesEntry const* cEntry =
                    sChrClassesStore.LookupEntry(current_class);
                if (!cEntry ||
                    !((1 << (current_class - 1)) & CLASSMASK_ALL_PLAYABLE))
                {
                    logging.error(
                        "Wrong class %u in `playercreateinfo_item` table, "
                        "ignoring.",
                        current_class);
                    continue;
                }

                PlayerInfo* pInfo = &playerInfo[current_race][current_class];

                uint32 item_id = fields[2].GetUInt32();

                if (!GetItemPrototype(item_id))
                {
                    logging.error(
                        "Item id %u (race %u class %u) in "
                        "`playercreateinfo_item` table but not listed in "
                        "`item_template`, ignoring.",
                        item_id, current_race, current_class);
                    continue;
                }

                uint32 amount = fields[3].GetUInt32();

                if (!amount)
                {
                    logging.error(
                        "Item id %u (class %u race %u) have amount==0 in "
                        "`playercreateinfo_item` table, ignoring.",
                        item_id, current_race, current_class);
                    continue;
                }

                pInfo->item.push_back(PlayerCreateInfoItem(item_id, amount));

                bar.step();
                ++count;
            } while (result->NextRow());

            delete result;

            logging.info("Loaded %u custom player create items\n", count);
        }
    }

    // Load playercreate spells
    {
        //                                                0     1      2
        QueryResult* result = WorldDatabase.Query(
            "SELECT race, class, Spell FROM playercreateinfo_spell");

        uint32 count = 0;

        if (!result)
        {
            logging.info("Loaded %u player create spells\n", count);
            logging.error(
                "Error loading `playercreateinfo_spell` table or empty table.");
        }
        else
        {
            BarGoLink bar(result->GetRowCount());

            do
            {
                Field* fields = result->Fetch();

                uint32 current_race = fields[0].GetUInt32();
                uint32 current_class = fields[1].GetUInt32();

                ChrRacesEntry const* rEntry =
                    sChrRacesStore.LookupEntry(current_race);
                if (!rEntry ||
                    !((1 << (current_race - 1)) & RACEMASK_ALL_PLAYABLE))
                {
                    logging.error(
                        "Wrong race %u in `playercreateinfo_spell` table, "
                        "ignoring.",
                        current_race);
                    continue;
                }

                ChrClassesEntry const* cEntry =
                    sChrClassesStore.LookupEntry(current_class);
                if (!cEntry ||
                    !((1 << (current_class - 1)) & CLASSMASK_ALL_PLAYABLE))
                {
                    logging.error(
                        "Wrong class %u in `playercreateinfo_spell` table, "
                        "ignoring.",
                        current_class);
                    continue;
                }

                uint32 spell_id = fields[2].GetUInt32();
                if (!sSpellStore.LookupEntry(spell_id))
                {
                    logging.error(
                        "Non existing spell %u in `playercreateinfo_spell` "
                        "table, ignoring.",
                        spell_id);
                    continue;
                }

                PlayerInfo* pInfo = &playerInfo[current_race][current_class];
                pInfo->spell.push_back(spell_id);

                bar.step();
                ++count;
            } while (result->NextRow());

            delete result;

            logging.info("Loaded %u player create spells\n", count);
        }
    }

    // Load playercreate actions
    {
        //                                                0     1      2       3
        //                                                4
        QueryResult* result = WorldDatabase.Query(
            "SELECT race, class, button, action, type FROM "
            "playercreateinfo_action");

        uint32 count = 0;

        if (!result)
        {
            logging.info("Loaded %u player create actions\n", count);
            logging.error(
                "Error loading `playercreateinfo_action` table or empty "
                "table.");
        }
        else
        {
            BarGoLink bar(result->GetRowCount());

            do
            {
                Field* fields = result->Fetch();

                uint32 current_race = fields[0].GetUInt32();
                uint32 current_class = fields[1].GetUInt32();

                ChrRacesEntry const* rEntry =
                    sChrRacesStore.LookupEntry(current_race);
                if (!rEntry ||
                    !((1 << (current_race - 1)) & RACEMASK_ALL_PLAYABLE))
                {
                    logging.error(
                        "Wrong race %u in `playercreateinfo_action` table, "
                        "ignoring.",
                        current_race);
                    continue;
                }

                ChrClassesEntry const* cEntry =
                    sChrClassesStore.LookupEntry(current_class);
                if (!cEntry ||
                    !((1 << (current_class - 1)) & CLASSMASK_ALL_PLAYABLE))
                {
                    logging.error(
                        "Wrong class %u in `playercreateinfo_action` table, "
                        "ignoring.",
                        current_class);
                    continue;
                }

                uint8 action_button = fields[2].GetUInt8();
                uint32 action = fields[3].GetUInt32();
                uint8 action_type = fields[4].GetUInt8();

                if (!Player::IsActionButtonDataValid(
                        action_button, action, action_type, nullptr))
                    continue;

                PlayerInfo* pInfo = &playerInfo[current_race][current_class];
                pInfo->action.push_back(
                    PlayerCreateInfoAction(action_button, action, action_type));

                bar.step();
                ++count;
            } while (result->NextRow());

            delete result;

            logging.info("Loaded %u player create actions\n", count);
        }
    }

    // Loading levels data (class only dependent)
    {
        //                                                 0      1      2 3
        QueryResult* result = WorldDatabase.Query(
            "SELECT class, level, basehp, basemana FROM "
            "player_classlevelstats");

        uint32 count = 0;

        if (!result)
        {
            logging.info("Loaded %u level health/mana definitions\n", count);
            logging.error(
                "Error loading `player_classlevelstats` table or empty table.");

            exit(1);
        }

        BarGoLink bar(result->GetRowCount());

        do
        {
            Field* fields = result->Fetch();

            uint32 current_class = fields[0].GetUInt32();
            if (current_class >= MAX_CLASSES)
            {
                logging.error(
                    "Wrong class %u in `player_classlevelstats` table, "
                    "ignoring.",
                    current_class);
                continue;
            }

            uint32 current_level = fields[1].GetUInt32();
            if (current_level == 0)
            {
                logging.error(
                    "Wrong level %u in `player_classlevelstats` table, "
                    "ignoring.",
                    current_level);
                continue;
            }
            else if (current_level > sWorld::Instance()->getConfig(
                                         CONFIG_UINT32_MAX_PLAYER_LEVEL))
            {
                if (current_level > STRONG_MAX_LEVEL) // hardcoded level maximum
                    logging.error(
                        "Wrong (> %u) level %u in `player_classlevelstats` "
                        "table, ignoring.",
                        STRONG_MAX_LEVEL, current_level);
                else
                {
                    LOG_DEBUG(logging,
                        "Unused (> MaxPlayerLevel in mangosd.conf) level %u in "
                        "`player_classlevelstats` table, ignoring.",
                        current_level);
                    ++count; // make result loading percent "expected" correct
                             // in case disabled detail mode for example.
                }
                continue;
            }

            PlayerClassInfo* pClassInfo = &playerClassInfo[current_class];

            if (!pClassInfo->levelInfo)
                pClassInfo->levelInfo =
                    new PlayerClassLevelInfo[sWorld::Instance()->getConfig(
                        CONFIG_UINT32_MAX_PLAYER_LEVEL)];

            PlayerClassLevelInfo* pClassLevelInfo =
                &pClassInfo->levelInfo[current_level - 1];

            pClassLevelInfo->basehealth = fields[2].GetUInt16();
            pClassLevelInfo->basemana = fields[3].GetUInt16();

            bar.step();
            ++count;
        } while (result->NextRow());

        delete result;

        logging.info("Loaded %u level health/mana definitions\n", count);
    }

    // Fill gaps and check integrity
    for (int class_ = 0; class_ < MAX_CLASSES; ++class_)
    {
        // skip nonexistent classes
        if (!sChrClassesStore.LookupEntry(class_))
            continue;

        PlayerClassInfo* pClassInfo = &playerClassInfo[class_];

        // fatal error if no level 1 data
        if (!pClassInfo->levelInfo || pClassInfo->levelInfo[0].basehealth == 0)
        {
            logging.error(
                "Class %i Level 1 does not have health/mana data!", class_);

            exit(1);
        }

        // fill level gaps
        for (uint32 level = 1; level < sWorld::Instance()->getConfig(
                                           CONFIG_UINT32_MAX_PLAYER_LEVEL);
             ++level)
        {
            if (pClassInfo->levelInfo[level].basehealth == 0)
            {
                logging.error(
                    "Class %i Level %i does not have health/mana data. Using "
                    "stats data of level %i.",
                    class_, level + 1, level);
                pClassInfo->levelInfo[level] = pClassInfo->levelInfo[level - 1];
            }
        }
    }

    // Loading levels data (class/race dependent)
    {
        //                                                 0     1      2      3
        //                                                 4    5    6    7
        QueryResult* result = WorldDatabase.Query(
            "SELECT race, class, level, str, agi, sta, inte, spi FROM "
            "player_levelstats");

        uint32 count = 0;

        if (!result)
        {
            logging.info("Loaded %u level stats definitions\n", count);
            logging.error(
                "Error loading `player_levelstats` table or empty table.");

            exit(1);
        }

        BarGoLink bar(result->GetRowCount());

        do
        {
            Field* fields = result->Fetch();

            uint32 current_race = fields[0].GetUInt32();
            uint32 current_class = fields[1].GetUInt32();

            ChrRacesEntry const* rEntry =
                sChrRacesStore.LookupEntry(current_race);
            if (!rEntry || !((1 << (current_race - 1)) & RACEMASK_ALL_PLAYABLE))
            {
                logging.error(
                    "Wrong race %u in `player_levelstats` table, ignoring.",
                    current_race);
                continue;
            }

            ChrClassesEntry const* cEntry =
                sChrClassesStore.LookupEntry(current_class);
            if (!cEntry ||
                !((1 << (current_class - 1)) & CLASSMASK_ALL_PLAYABLE))
            {
                logging.error(
                    "Wrong class %u in `player_levelstats` table, ignoring.",
                    current_class);
                continue;
            }

            uint32 current_level = fields[2].GetUInt32();
            if (current_level >
                sWorld::Instance()->getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
            {
                if (current_level > STRONG_MAX_LEVEL) // hardcoded level maximum
                    logging.error(
                        "Wrong (> %u) level %u in `player_levelstats` table, "
                        "ignoring.",
                        STRONG_MAX_LEVEL, current_level);
                else
                {
                    LOG_DEBUG(logging,
                        "Unused (> MaxPlayerLevel in mangosd.conf) level %u in "
                        "`player_levelstats` table, ignoring.",
                        current_level);
                    ++count; // make result loading percent "expected" correct
                             // in case disabled detail mode for example.
                }
                continue;
            }

            PlayerInfo* pInfo = &playerInfo[current_race][current_class];

            if (!pInfo->levelInfo)
                pInfo->levelInfo =
                    new PlayerLevelInfo[sWorld::Instance()->getConfig(
                        CONFIG_UINT32_MAX_PLAYER_LEVEL)];

            PlayerLevelInfo* pLevelInfo = &pInfo->levelInfo[current_level - 1];

            for (int i = 0; i < MAX_STATS; ++i)
                pLevelInfo->stats[i] = fields[i + 3].GetUInt8();

            bar.step();
            ++count;
        } while (result->NextRow());

        delete result;

        logging.info("Loaded %u level stats definitions\n", count);
    }

    // Fill gaps and check integrity
    for (int race = 0; race < MAX_RACES; ++race)
    {
        // skip nonexistent races
        if (!((1 << (race - 1)) & RACEMASK_ALL_PLAYABLE) ||
            !sChrRacesStore.LookupEntry(race))
            continue;

        for (int class_ = 0; class_ < MAX_CLASSES; ++class_)
        {
            // skip nonexistent classes
            if (!((1 << (class_ - 1)) & CLASSMASK_ALL_PLAYABLE) ||
                !sChrClassesStore.LookupEntry(class_))
                continue;

            PlayerInfo* pInfo = &playerInfo[race][class_];

            // skip non loaded combinations
            if (!pInfo->displayId_m || !pInfo->displayId_f)
                continue;

            // skip expansion races if not playing with expansion
            if (sWorld::Instance()->getConfig(CONFIG_UINT32_EXPANSION) < 1 &&
                (race == RACE_BLOODELF || race == RACE_DRAENEI))
                continue;

            // fatal error if no level 1 data
            if (!pInfo->levelInfo || pInfo->levelInfo[0].stats[0] == 0)
            {
                logging.error(
                    "Race %i Class %i Level 1 does not have stats data!", race,
                    class_);

                exit(1);
            }

            // fill level gaps
            for (uint32 level = 1; level < sWorld::Instance()->getConfig(
                                               CONFIG_UINT32_MAX_PLAYER_LEVEL);
                 ++level)
            {
                if (pInfo->levelInfo[level].stats[0] == 0)
                {
                    logging.error(
                        "Race %i Class %i Level %i does not have stats data. "
                        "Using stats data of level %i.",
                        race, class_, level + 1, level);
                    pInfo->levelInfo[level] = pInfo->levelInfo[level - 1];
                }
            }
        }
    }

    // Loading xp per level data
    {
        mPlayerXPperLevel.resize(
            sWorld::Instance()->getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL));
        for (uint32 level = 0; level < sWorld::Instance()->getConfig(
                                           CONFIG_UINT32_MAX_PLAYER_LEVEL);
             ++level)
            mPlayerXPperLevel[level] = 0;

        //                                                 0    1
        QueryResult* result = WorldDatabase.Query(
            "SELECT lvl, xp_for_next_level FROM player_xp_for_level");

        uint32 count = 0;

        if (!result)
        {
            logging.info("Loaded %u xp for level definitions\n", count);
            logging.error(
                "Error loading `player_xp_for_level` table or empty table.");

            exit(1);
        }

        BarGoLink bar(result->GetRowCount());

        do
        {
            Field* fields = result->Fetch();

            uint32 current_level = fields[0].GetUInt32();
            uint32 current_xp = fields[1].GetUInt32();

            if (current_level >=
                sWorld::Instance()->getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
            {
                if (current_level > STRONG_MAX_LEVEL) // hardcoded level maximum
                    logging.error(
                        "Wrong (> %u) level %u in `player_xp_for_level` table, "
                        "ignoring.",
                        STRONG_MAX_LEVEL, current_level);
                else
                {
                    LOG_DEBUG(logging,
                        "Unused (> MaxPlayerLevel in mangosd.conf) level %u in "
                        "`player_xp_for_levels` table, ignoring.",
                        current_level);
                    ++count; // make result loading percent "expected" correct
                             // in case disabled detail mode for example.
                }
                continue;
            }
            // PlayerXPperLevel
            mPlayerXPperLevel[current_level] = current_xp;
            bar.step();
            ++count;
        } while (result->NextRow());

        delete result;

        logging.info("Loaded %u xp for level definitions\n", count);
    }

    // fill level gaps
    for (uint32 level = 1;
         level < sWorld::Instance()->getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL);
         ++level)
    {
        if (mPlayerXPperLevel[level] == 0)
        {
            logging.error(
                "Level %i does not have XP for level data. Using data of level "
                "[%i] + 100.",
                level + 1, level);
            mPlayerXPperLevel[level] = mPlayerXPperLevel[level - 1] + 100;
        }
    }
}

void ObjectMgr::GetPlayerClassLevelInfo(
    uint32 class_, uint32 level, PlayerClassLevelInfo* info) const
{
    if (level < 1 || class_ >= MAX_CLASSES)
        return;

    PlayerClassInfo const* pInfo = &playerClassInfo[class_];

    if (level > sWorld::Instance()->getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
        level = sWorld::Instance()->getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL);

    *info = pInfo->levelInfo[level - 1];
}

void ObjectMgr::GetPlayerLevelInfo(
    uint32 race, uint32 class_, uint32 level, PlayerLevelInfo* info) const
{
    if (level < 1 || race >= MAX_RACES || class_ >= MAX_CLASSES)
        return;

    PlayerInfo const* pInfo = &playerInfo[race][class_];
    if (pInfo->displayId_m == 0 || pInfo->displayId_f == 0)
        return;

    if (level <= sWorld::Instance()->getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
        *info = pInfo->levelInfo[level - 1];
    else
        BuildPlayerLevelInfo(race, class_, level, info);
}

void ObjectMgr::BuildPlayerLevelInfo(
    uint8 race, uint8 _class, uint8 level, PlayerLevelInfo* info) const
{
    // base data (last known level)
    *info =
        playerInfo[race][_class].levelInfo[sWorld::Instance()->getConfig(
                                               CONFIG_UINT32_MAX_PLAYER_LEVEL) -
                                           1];

    for (int lvl =
             sWorld::Instance()->getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL) - 1;
         lvl < level; ++lvl)
    {
        switch (_class)
        {
        case CLASS_WARRIOR:
            info->stats[STAT_STRENGTH] += (lvl > 23 ? 2 : (lvl > 1 ? 1 : 0));
            info->stats[STAT_STAMINA] += (lvl > 23 ? 2 : (lvl > 1 ? 1 : 0));
            info->stats[STAT_AGILITY] +=
                (lvl > 36 ? 1 : (lvl > 6 && (lvl % 2) ? 1 : 0));
            info->stats[STAT_INTELLECT] += (lvl > 9 && !(lvl % 2) ? 1 : 0);
            info->stats[STAT_SPIRIT] += (lvl > 9 && !(lvl % 2) ? 1 : 0);
            break;
        case CLASS_PALADIN:
            info->stats[STAT_STRENGTH] += (lvl > 3 ? 1 : 0);
            info->stats[STAT_STAMINA] += (lvl > 33 ? 2 : (lvl > 1 ? 1 : 0));
            info->stats[STAT_AGILITY] +=
                (lvl > 38 ? 1 : (lvl > 7 && !(lvl % 2) ? 1 : 0));
            info->stats[STAT_INTELLECT] += (lvl > 6 && (lvl % 2) ? 1 : 0);
            info->stats[STAT_SPIRIT] += (lvl > 7 ? 1 : 0);
            break;
        case CLASS_HUNTER:
            info->stats[STAT_STRENGTH] += (lvl > 4 ? 1 : 0);
            info->stats[STAT_STAMINA] += (lvl > 4 ? 1 : 0);
            info->stats[STAT_AGILITY] += (lvl > 33 ? 2 : (lvl > 1 ? 1 : 0));
            info->stats[STAT_INTELLECT] += (lvl > 8 && (lvl % 2) ? 1 : 0);
            info->stats[STAT_SPIRIT] +=
                (lvl > 38 ? 1 : (lvl > 9 && !(lvl % 2) ? 1 : 0));
            break;
        case CLASS_ROGUE:
            info->stats[STAT_STRENGTH] += (lvl > 5 ? 1 : 0);
            info->stats[STAT_STAMINA] += (lvl > 4 ? 1 : 0);
            info->stats[STAT_AGILITY] += (lvl > 16 ? 2 : (lvl > 1 ? 1 : 0));
            info->stats[STAT_INTELLECT] += (lvl > 8 && !(lvl % 2) ? 1 : 0);
            info->stats[STAT_SPIRIT] +=
                (lvl > 38 ? 1 : (lvl > 9 && !(lvl % 2) ? 1 : 0));
            break;
        case CLASS_PRIEST:
            info->stats[STAT_STRENGTH] += (lvl > 9 && !(lvl % 2) ? 1 : 0);
            info->stats[STAT_STAMINA] += (lvl > 5 ? 1 : 0);
            info->stats[STAT_AGILITY] +=
                (lvl > 38 ? 1 : (lvl > 8 && (lvl % 2) ? 1 : 0));
            info->stats[STAT_INTELLECT] += (lvl > 22 ? 2 : (lvl > 1 ? 1 : 0));
            info->stats[STAT_SPIRIT] += (lvl > 3 ? 1 : 0);
            break;
        case CLASS_SHAMAN:
            info->stats[STAT_STRENGTH] +=
                (lvl > 34 ? 1 : (lvl > 6 && (lvl % 2) ? 1 : 0));
            info->stats[STAT_STAMINA] += (lvl > 4 ? 1 : 0);
            info->stats[STAT_AGILITY] += (lvl > 7 && !(lvl % 2) ? 1 : 0);
            info->stats[STAT_INTELLECT] += (lvl > 5 ? 1 : 0);
            info->stats[STAT_SPIRIT] += (lvl > 4 ? 1 : 0);
            break;
        case CLASS_MAGE:
            info->stats[STAT_STRENGTH] += (lvl > 9 && !(lvl % 2) ? 1 : 0);
            info->stats[STAT_STAMINA] += (lvl > 5 ? 1 : 0);
            info->stats[STAT_AGILITY] += (lvl > 9 && !(lvl % 2) ? 1 : 0);
            info->stats[STAT_INTELLECT] += (lvl > 24 ? 2 : (lvl > 1 ? 1 : 0));
            info->stats[STAT_SPIRIT] += (lvl > 33 ? 2 : (lvl > 2 ? 1 : 0));
            break;
        case CLASS_WARLOCK:
            info->stats[STAT_STRENGTH] += (lvl > 9 && !(lvl % 2) ? 1 : 0);
            info->stats[STAT_STAMINA] += (lvl > 38 ? 2 : (lvl > 3 ? 1 : 0));
            info->stats[STAT_AGILITY] += (lvl > 9 && !(lvl % 2) ? 1 : 0);
            info->stats[STAT_INTELLECT] += (lvl > 33 ? 2 : (lvl > 2 ? 1 : 0));
            info->stats[STAT_SPIRIT] += (lvl > 38 ? 2 : (lvl > 3 ? 1 : 0));
            break;
        case CLASS_DRUID:
            info->stats[STAT_STRENGTH] +=
                (lvl > 38 ? 2 : (lvl > 6 && (lvl % 2) ? 1 : 0));
            info->stats[STAT_STAMINA] += (lvl > 32 ? 2 : (lvl > 4 ? 1 : 0));
            info->stats[STAT_AGILITY] +=
                (lvl > 38 ? 2 : (lvl > 8 && (lvl % 2) ? 1 : 0));
            info->stats[STAT_INTELLECT] += (lvl > 38 ? 3 : (lvl > 4 ? 1 : 0));
            info->stats[STAT_SPIRIT] += (lvl > 38 ? 3 : (lvl > 5 ? 1 : 0));
        }
    }
}

void ObjectMgr::LoadArenaTeams()
{
    uint32 count = 0;

    QueryResult* result = CharacterDatabase.Query(
        "SELECT "
        "arena_team.arenateamid,name,captainguid,type,BackgroundColor,"
        "EmblemStyle,"
        "EmblemColor,BorderStyle,BorderColor, "
        "rating,games_week,wins_week,games_season,wins_season "
        "FROM arena_team LEFT JOIN arena_team_stats ON arena_team.arenateamid "
        "= arena_team_stats.arenateamid ORDER BY arena_team.arenateamid ASC");

    if (!result)
    {
        logging.info("Loaded %u arenateam definitions\n", count);
        return;
    }

    // load arena_team members
    QueryResult* arenaTeamMembersResult = CharacterDatabase.Query(
        "SELECT "
        "arenateamid,member.guid,played_week,wons_week,played_season,wons_"
        "season,personal_rating,name,class "
        "FROM arena_team_member member LEFT JOIN characters chars on "
        "member.guid = chars.guid ORDER BY member.arenateamid ASC");

    BarGoLink bar(result->GetRowCount());

    do
    {
        bar.step();
        ++count;

        auto newArenaTeam = new ArenaTeam;
        if (!newArenaTeam->LoadArenaTeamFromDB(result) ||
            !newArenaTeam->LoadMembersFromDB(arenaTeamMembersResult))
        {
            newArenaTeam->Disband(nullptr);
            delete newArenaTeam;
            continue;
        }
        AddArenaTeam(newArenaTeam);
    } while (result->NextRow());

    delete result;
    delete arenaTeamMembersResult;

    logging.info("Loaded %u arenateam definitions\n", count);
}

void ObjectMgr::LoadGroups()
{
    // -- loading groups --
    uint32 count = 0;
    //                                                    0         1
    //                                                    2           3
    //                                                    4              5
    //                                                    6      7      8      9
    //                                                    10     11     12
    //                                                    13      14          15
    //                                                    16
    QueryResult* result = CharacterDatabase.Query(
        "SELECT mainTank, mainAssistant, lootMethod, looterGuid, "
        "lootThreshold, icon1, icon2, icon3, icon4, icon5, icon6, icon7, "
        "icon8, isRaid, difficulty, leaderGuid, groupId FROM groups");

    if (!result)
    {
        logging.info("Loaded %u group definitions\n", count);
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        bar.step();
        Field* fields = result->Fetch();
        ++count;
        auto group = new Group;
        if (!group->LoadGroupFromDB(fields))
        {
            group->Disband();
            delete group;
            continue;
        }
        AddGroup(group);
    } while (result->NextRow());

    delete result;

    logging.info("Loaded %u group definitions\n", count);

    // -- loading members --
    count = 0;
    //                                       0           1          2         3
    result = CharacterDatabase.Query(
        "SELECT memberGuid, assistant, subgroup, groupId FROM group_member "
        "ORDER BY groupId");
    if (!result)
    {
        BarGoLink bar2(1);
        bar2.step();
    }
    else
    {
        Group* group =
            nullptr; // used as cached pointer for avoid relookup group
                     // for each member

        BarGoLink bar2(result->GetRowCount());
        do
        {
            bar2.step();
            Field* fields = result->Fetch();
            count++;

            uint32 memberGuidlow = fields[0].GetUInt32();
            ObjectGuid memberGuid = ObjectGuid(HIGHGUID_PLAYER, memberGuidlow);
            bool assistent = fields[1].GetBool();
            uint8 subgroup = fields[2].GetUInt8();
            uint32 groupId = fields[3].GetUInt32();
            if (!group || group->GetId() != groupId)
            {
                group = GetGroupById(groupId);
                if (!group)
                {
                    logging.error(
                        "Incorrect entry in group_member table : no group with "
                        "Id %d for member %s!",
                        groupId, memberGuid.GetString().c_str());
                    CharacterDatabase.PExecute(
                        "DELETE FROM group_member WHERE memberGuid = '%u'",
                        memberGuidlow);
                    continue;
                }
            }

            if (!group->LoadMemberFromDB(memberGuidlow, subgroup, assistent))
            {
                logging.error(
                    "Incorrect entry in group_member table : member %s cannot "
                    "be added to group (Id: %u)!",
                    memberGuid.GetString().c_str(), groupId);
                CharacterDatabase.PExecute(
                    "DELETE FROM group_member WHERE memberGuid = '%u'",
                    memberGuidlow);
            }
        } while (result->NextRow());
        delete result;
    }

    // clean groups
    // TODO: maybe delete from the DB before loading in this case
    for (auto itr = mGroupMap.begin(); itr != mGroupMap.end();)
    {
        if (itr->second->GetMembersCount() < 2)
        {
            itr->second->Disband();
            delete itr->second;
            mGroupMap.erase(itr++);
        }
        else
            ++itr;
    }

    // -- loading instances --
    count = 0;
    result = CharacterDatabase.Query(
        //      0    1   2     3    4          5
        "SELECT gid, id, perm, map, resettime, difficulty FROM group_instance "
        "GI LEFT JOIN instance I ON GI.instance = I.id");

    if (!result)
    {
        BarGoLink bar2(1);
        bar2.step();
    }
    else
    {
        BarGoLink bar2(result->GetRowCount());
        do
        {
            bar2.step();
            ++count;

            Field* fields = result->Fetch();
            uint32 gid = fields[0].GetUInt32();
            uint32 instance = fields[1].GetUInt32();
            bool perm = fields[2].GetUInt8();
            uint32 mapid = fields[3].GetUInt32();
            time_t resettime = (time_t)fields[4].GetUInt64();
            uint32 difficulty = fields[5].GetUInt8();

            auto group = GetGroupById(gid);
            if (!group)
            {
                logging.error(
                    "Table `group_instance` had entry for a non-existant group "
                    "(`gid`: %u).",
                    gid);
                CharacterDatabase.PExecute(
                    "DELETE FROM group_instance WHERE gid=%u", gid);
                continue;
            }

            const MapEntry* mapEntry = sMapStore.LookupEntry(mapid);
            if (!mapEntry || !mapEntry->IsDungeon())
            {
                logging.error(
                    "Table `group_instance` referred to a map that was not a "
                    "dungeon or raid (mapid=%u instance=%u).",
                    mapid, instance);
                CharacterDatabase.PExecute(
                    "DELETE FROM group_instance WHERE gid=%u AND instance=%u",
                    gid, instance);
                continue;
            }

            if (difficulty >= MAX_DIFFICULTY)
            {
                logging.error(
                    "Invalid difficulty in `instance` table (%u).", difficulty);
                continue;
            }

            sMapPersistentStateMgr::Instance()->AddPersistentState(mapEntry,
                instance, (Difficulty)difficulty, resettime, !perm, true);

            if (auto state = sMapPersistentStateMgr::Instance()
                                 ->GetDungeonPersistentState(instance))
            {
                InstanceGroupBind bind;
                bind.state = state;
                bind.perm = perm;
                group->GetInstanceBindsMap(
                    (Difficulty)difficulty)[state->GetMapId()] = bind;
            }
        } while (result->NextRow());
        delete result;
    }

    logging.info("Loaded %u group-instance binds total\n", count);
}

void ObjectMgr::LoadQuests()
{
    // For reload case
    for (QuestMap::const_iterator itr = mQuestTemplates.begin();
         itr != mQuestTemplates.end(); ++itr)
        delete itr->second;

    mQuestTemplates.clear();

    m_ExclusiveQuestGroups.clear();

    //                                                0      1       2
    //                                                3         4           5
    //                                                6                7
    //                                                8              9
    QueryResult* result = WorldDatabase.Query(
        "SELECT entry, Method, ZoneOrSort, MinLevel, QuestLevel, Type, "
        "RequiredClasses, RequiredRaces, RequiredSkill, RequiredSkillValue,"
        //   10                   11                 12                     13
        //   14                     15                   16                17
        "RepObjectiveFaction, RepObjectiveValue, RequiredMinRepFaction, "
        "RequiredMinRepValue, RequiredMaxRepFaction, RequiredMaxRepValue, "
        "SuggestedPlayers, LimitTime,"
        //   18          19            20           21           22           23
        //   24                25         26            27
        "QuestFlags, SpecialFlags, CharTitleId, PrevQuestId, NextQuestId, "
        "ExclusiveGroup, NextQuestInChain, SrcItemId, SrcItemCount, SrcSpell,"
        //   28     29       30          31               32                33
        //   34              35              36              37
        "Title, Details, Objectives, OfferRewardText, RequestItemsText, "
        "EndText, ObjectiveText1, ObjectiveText2, ObjectiveText3, "
        "ObjectiveText4,"
        //   38          39          40          41          42             43
        //   44             45
        "ReqItemId1, ReqItemId2, ReqItemId3, ReqItemId4, ReqItemCount1, "
        "ReqItemCount2, ReqItemCount3, ReqItemCount4,"
        //   46            47            48            49            50
        //   51               52               53
        "ReqSourceId1, ReqSourceId2, ReqSourceId3, ReqSourceId4, "
        "ReqSourceCount1, ReqSourceCount2, ReqSourceCount3, ReqSourceCount4,"
        //   54                  55                  56                  57
        //   58                     59                     60 61
        "ReqCreatureOrGOId1, ReqCreatureOrGOId2, ReqCreatureOrGOId3, "
        "ReqCreatureOrGOId4, ReqCreatureOrGOCount1, ReqCreatureOrGOCount2, "
        "ReqCreatureOrGOCount3, ReqCreatureOrGOCount4,"
        //   62             63             64             65
        "ReqSpellCast1, ReqSpellCast2, ReqSpellCast3, ReqSpellCast4,"
        //   66                67                68                69
        //   70                71
        "RewChoiceItemId1, RewChoiceItemId2, RewChoiceItemId3, "
        "RewChoiceItemId4, RewChoiceItemId5, RewChoiceItemId6,"
        //   72                   73                   74                   75
        //   76                   77
        "RewChoiceItemCount1, RewChoiceItemCount2, RewChoiceItemCount3, "
        "RewChoiceItemCount4, RewChoiceItemCount5, RewChoiceItemCount6,"
        //   78          79          80          81          82             83
        //   84             85
        "RewItemId1, RewItemId2, RewItemId3, RewItemId4, RewItemCount1, "
        "RewItemCount2, RewItemCount3, RewItemCount4,"
        //   86              87              88              89              90
        //   91            92            93            94            95
        "RewRepFaction1, RewRepFaction2, RewRepFaction3, RewRepFaction4, "
        "RewRepFaction5, RewRepValue1, RewRepValue2, RewRepValue3, "
        "RewRepValue4, RewRepValue5,"
        //   96                 97             98                99        100
        //   101                102               103         104     105 106
        "RewHonorableKills, RewOrReqMoney, RewMoneyMaxLevel, RewSpell, "
        "RewSpellCast, RewMailTemplateId, RewMailDelaySecs, PointMapId, "
        "PointX, PointY, PointOpt,"
        //   107            108            109            110            111
        //   112                 113                 114
        "DetailsEmote1, DetailsEmote2, DetailsEmote3, DetailsEmote4, "
        "DetailsEmoteDelay1, DetailsEmoteDelay2, DetailsEmoteDelay3, "
        "DetailsEmoteDelay4,"
        //   115              116            117                118
        //   119                120
        "IncompleteEmote, CompleteEmote, OfferRewardEmote1, OfferRewardEmote2, "
        "OfferRewardEmote3, OfferRewardEmote4,"
        //   121                     122                     123 124
        "OfferRewardEmoteDelay1, OfferRewardEmoteDelay2, "
        "OfferRewardEmoteDelay3, OfferRewardEmoteDelay4,"
        //   125          126
        "StartScript, CompleteScript"
        " FROM quest_template");
    if (!result)
    {
        logging.info("Loaded 0 quests definitions\n");
        logging.error("`quest_template` table is empty!");
        return;
    }

    // create multimap previous quest for each existing quest
    // some quests can have many previous maps set by NextQuestId in previous
    // quest
    // for example set of race quests can lead to single not race specific quest
    BarGoLink bar(result->GetRowCount());
    do
    {
        bar.step();
        Field* fields = result->Fetch();

        auto newQuest = new Quest(fields);
        mQuestTemplates[newQuest->GetQuestId()] = newQuest;
    } while (result->NextRow());

    delete result;

    // Post processing

    std::map<uint32, uint32> usedMailTemplates;

    for (auto iter = mQuestTemplates.begin(); iter != mQuestTemplates.end();
         ++iter)
    {
        Quest* qinfo = iter->second;

        // additional quest integrity checks (GO, creature_template and
        // item_template must be loaded already)

        if (qinfo->GetQuestMethod() >= 3)
        {
            logging.error(
                "Quest %u has `Method` = %u, expected values are 0, 1 or 2.",
                qinfo->GetQuestId(), qinfo->GetQuestMethod());
        }

        if (qinfo->m_SpecialFlags > QUEST_SPECIAL_FLAG_DB_ALLOWED)
        {
            logging.error(
                "Quest %u has `SpecialFlags` = %u, above max flags not allowed "
                "for database.",
                qinfo->GetQuestId(), qinfo->m_SpecialFlags);
        }

        if (qinfo->HasQuestFlag(QUEST_FLAGS_DAILY))
        {
            if (!qinfo->HasSpecialFlag(QUEST_SPECIAL_FLAG_REPEATABLE))
            {
                logging.error(
                    "Daily Quest %u not marked as repeatable in "
                    "`SpecialFlags`, added.",
                    qinfo->GetQuestId());
                qinfo->SetSpecialFlag(QUEST_SPECIAL_FLAG_REPEATABLE);
            }
        }

        if (qinfo->HasQuestFlag(QUEST_FLAGS_AUTO_REWARDED))
        {
            // at auto-reward can be rewarded only RewChoiceItemId[0]
            for (int j = 1; j < QUEST_REWARD_CHOICES_COUNT; ++j)
            {
                if (uint32 id = qinfo->RewChoiceItemId[j])
                {
                    logging.error(
                        "Quest %u has `RewChoiceItemId%d` = %u but item from "
                        "`RewChoiceItemId%d` can't be rewarded with quest flag "
                        "QUEST_FLAGS_AUTO_REWARDED.",
                        qinfo->GetQuestId(), j + 1, id, j + 1);
                    // no changes, quest ignore this data
                }
            }
        }

        // client quest log visual (area case)
        if (qinfo->ZoneOrSort > 0)
        {
            if (!GetAreaEntryByAreaID(qinfo->ZoneOrSort))
            {
                logging.error(
                    "Quest %u has `ZoneOrSort` = %u (zone case) but zone with "
                    "this id does not exist.",
                    qinfo->GetQuestId(), qinfo->ZoneOrSort);
                // no changes, quest not dependent from this value but can have
                // problems at client
            }
        }
        // client quest log visual (sort case)
        if (qinfo->ZoneOrSort < 0)
        {
            QuestSortEntry const* qSort =
                sQuestSortStore.LookupEntry(-int32(qinfo->ZoneOrSort));
            if (!qSort)
            {
                logging.error(
                    "Quest %u has `ZoneOrSort` = %i (sort case) but quest sort "
                    "with this id does not exist.",
                    qinfo->GetQuestId(), qinfo->ZoneOrSort);
                // no changes, quest not dependent from this value but can have
                // problems at client (note some may be 0, we must allow this so
                // no check)
            }
        }

        // RequiredClasses, can be 0/CLASSMASK_ALL_PLAYABLE to allow any class
        if (qinfo->RequiredClasses)
        {
            if (!(qinfo->RequiredClasses & CLASSMASK_ALL_PLAYABLE))
            {
                logging.error(
                    "Quest %u does not contain any playable classes in "
                    "`RequiredClasses` (%u), value set to 0 (all classes).",
                    qinfo->GetQuestId(), qinfo->RequiredClasses);
                qinfo->RequiredClasses = 0;
            }
        }

        // RequiredRaces, can be 0/RACEMASK_ALL_PLAYABLE to allow any race
        if (qinfo->RequiredRaces)
        {
            if (!(qinfo->RequiredRaces & RACEMASK_ALL_PLAYABLE))
            {
                logging.error(
                    "Quest %u does not contain any playable races in "
                    "`RequiredRaces` (%u), value set to 0 (all races).",
                    qinfo->GetQuestId(), qinfo->RequiredRaces);
                qinfo->RequiredRaces = 0;
            }
        }

        // RequiredSkill, can be 0
        if (qinfo->RequiredSkill)
        {
            if (!sSkillLineStore.LookupEntry(qinfo->RequiredSkill))
            {
                logging.error(
                    "Quest %u has `RequiredSkill` = %u but this skill does not "
                    "exist",
                    qinfo->GetQuestId(), qinfo->RequiredSkill);
            }
        }

        if (qinfo->RequiredSkillValue)
        {
            if (qinfo->RequiredSkillValue >
                sWorld::Instance()->GetConfigMaxSkillValue())
            {
                logging.error(
                    "Quest %u has `RequiredSkillValue` = %u but max possible "
                    "skill is %u, quest can't be done.",
                    qinfo->GetQuestId(), qinfo->RequiredSkillValue,
                    sWorld::Instance()->GetConfigMaxSkillValue());
                // no changes, quest can't be done for this requirement
            }
        }
        // else Skill quests can have 0 skill level, this is ok

        if (qinfo->RepObjectiveFaction &&
            !sFactionStore.LookupEntry(qinfo->RepObjectiveFaction))
        {
            logging.error(
                "Quest %u has `RepObjectiveFaction` = %u but faction template "
                "%u does not exist, quest can't be done.",
                qinfo->GetQuestId(), qinfo->RepObjectiveFaction,
                qinfo->RepObjectiveFaction);
            // no changes, quest can't be done for this requirement
        }

        if (qinfo->RequiredMinRepFaction &&
            !sFactionStore.LookupEntry(qinfo->RequiredMinRepFaction))
        {
            logging.error(
                "Quest %u has `RequiredMinRepFaction` = %u but faction "
                "template %u does not exist, quest can't be done.",
                qinfo->GetQuestId(), qinfo->RequiredMinRepFaction,
                qinfo->RequiredMinRepFaction);
            // no changes, quest can't be done for this requirement
        }

        if (qinfo->RequiredMaxRepFaction &&
            !sFactionStore.LookupEntry(qinfo->RequiredMaxRepFaction))
        {
            logging.error(
                "Quest %u has `RequiredMaxRepFaction` = %u but faction "
                "template %u does not exist, quest can't be done.",
                qinfo->GetQuestId(), qinfo->RequiredMaxRepFaction,
                qinfo->RequiredMaxRepFaction);
            // no changes, quest can't be done for this requirement
        }

        if (qinfo->RequiredMinRepValue &&
            qinfo->RequiredMinRepValue > ReputationMgr::Reputation_Cap)
        {
            logging.error(
                "Quest %u has `RequiredMinRepValue` = %d but max reputation is "
                "%u, quest can't be done.",
                qinfo->GetQuestId(), qinfo->RequiredMinRepValue,
                ReputationMgr::Reputation_Cap);
            // no changes, quest can't be done for this requirement
        }

        if (qinfo->RequiredMinRepValue && qinfo->RequiredMaxRepValue &&
            qinfo->RequiredMaxRepValue <= qinfo->RequiredMinRepValue)
        {
            logging.error(
                "Quest %u has `RequiredMaxRepValue` = %d and "
                "`RequiredMinRepValue` = %d, quest can't be done.",
                qinfo->GetQuestId(), qinfo->RequiredMaxRepValue,
                qinfo->RequiredMinRepValue);
            // no changes, quest can't be done for this requirement
        }

        if (!qinfo->RepObjectiveFaction && qinfo->RepObjectiveValue > 0)
        {
            logging.error(
                "Quest %u has `RepObjectiveValue` = %d but "
                "`RepObjectiveFaction` is 0, value has no effect",
                qinfo->GetQuestId(), qinfo->RepObjectiveValue);
            // warning
        }

        if (!qinfo->RequiredMinRepFaction && qinfo->RequiredMinRepValue > 0)
        {
            logging.error(
                "Quest %u has `RequiredMinRepValue` = %d but "
                "`RequiredMinRepFaction` is 0, value has no effect",
                qinfo->GetQuestId(), qinfo->RequiredMinRepValue);
            // warning
        }

        if (!qinfo->RequiredMaxRepFaction && qinfo->RequiredMaxRepValue > 0)
        {
            logging.error(
                "Quest %u has `RequiredMaxRepValue` = %d but "
                "`RequiredMaxRepFaction` is 0, value has no effect",
                qinfo->GetQuestId(), qinfo->RequiredMaxRepValue);
            // warning
        }

        if (qinfo->CharTitleId &&
            !sCharTitlesStore.LookupEntry(qinfo->CharTitleId))
        {
            logging.error(
                "Quest %u has `CharTitleId` = %u but CharTitle Id %u does not "
                "exist, quest can't be rewarded with title.",
                qinfo->GetQuestId(), qinfo->GetCharTitleId(),
                qinfo->GetCharTitleId());
            qinfo->CharTitleId = 0;
            // quest can't reward this title
        }

        if (qinfo->SrcItemId)
        {
            if (!sItemStorage.LookupEntry<ItemPrototype>(qinfo->SrcItemId))
            {
                logging.error(
                    "Quest %u has `SrcItemId` = %u but item with entry %u does "
                    "not exist, quest can't be done.",
                    qinfo->GetQuestId(), qinfo->SrcItemId, qinfo->SrcItemId);
                qinfo->SrcItemId =
                    0; // quest can't be done for this requirement
            }
            else if (qinfo->SrcItemCount == 0)
            {
                logging.error(
                    "Quest %u has `SrcItemId` = %u but `SrcItemCount` = 0, set "
                    "to 1 but need fix in DB.",
                    qinfo->GetQuestId(), qinfo->SrcItemId);
                qinfo->SrcItemCount = 1; // update to 1 for allow quest work for
                                         // backward compatibility with DB
            }
        }
        else if (qinfo->SrcItemCount > 0)
        {
            logging.error(
                "Quest %u has `SrcItemId` = 0 but `SrcItemCount` = %u, useless "
                "value.",
                qinfo->GetQuestId(), qinfo->SrcItemCount);
            qinfo->SrcItemCount = 0; // no quest work changes in fact
        }

        if (qinfo->SrcSpell)
        {
            SpellEntry const* spellInfo =
                sSpellStore.LookupEntry(qinfo->SrcSpell);
            if (!spellInfo)
            {
                logging.error(
                    "Quest %u has `SrcSpell` = %u but spell %u doesn't exist, "
                    "quest can't be done.",
                    qinfo->GetQuestId(), qinfo->SrcSpell, qinfo->SrcSpell);
                qinfo->SrcSpell = 0; // quest can't be done for this requirement
            }
            else if (!SpellMgr::IsSpellValid(spellInfo))
            {
                logging.error(
                    "Quest %u has `SrcSpell` = %u but spell %u is broken, "
                    "quest can't be done.",
                    qinfo->GetQuestId(), qinfo->SrcSpell, qinfo->SrcSpell);
                qinfo->SrcSpell = 0; // quest can't be done for this requirement
            }
        }

        for (int j = 0; j < QUEST_ITEM_OBJECTIVES_COUNT; ++j)
        {
            if (uint32 id = qinfo->ReqItemId[j])
            {
                if (qinfo->ReqItemCount[j] == 0)
                {
                    logging.error(
                        "Quest %u has `ReqItemId%d` = %u but `ReqItemCount%d` "
                        "= 0, quest can't be done.",
                        qinfo->GetQuestId(), j + 1, id, j + 1);
                    // no changes, quest can't be done for this requirement
                }

                qinfo->SetSpecialFlag(QUEST_SPECIAL_FLAG_DELIVER);

                if (!sItemStorage.LookupEntry<ItemPrototype>(id))
                {
                    logging.error(
                        "Quest %u has `ReqItemId%d` = %u but item with entry "
                        "%u does not exist, quest can't be done.",
                        qinfo->GetQuestId(), j + 1, id, id);
                    qinfo->ReqItemCount[j] =
                        0; // prevent incorrect work of quest
                }
            }
            else if (qinfo->ReqItemCount[j] > 0)
            {
                logging.error(
                    "Quest %u has `ReqItemId%d` = 0 but `ReqItemCount%d` = %u, "
                    "quest can't be done.",
                    qinfo->GetQuestId(), j + 1, j + 1, qinfo->ReqItemCount[j]);
                qinfo->ReqItemCount[j] = 0; // prevent incorrect work of quest
            }
        }

        for (int j = 0; j < QUEST_SOURCE_ITEM_IDS_COUNT; ++j)
        {
            if (uint32 id = qinfo->ReqSourceId[j])
            {
                if (!sItemStorage.LookupEntry<ItemPrototype>(id))
                {
                    logging.error(
                        "Quest %u has `ReqSourceId%d` = %u but item with entry "
                        "%u does not exist, quest can't be done.",
                        qinfo->GetQuestId(), j + 1, id, id);
                    // no changes, quest can't be done for this requirement
                }
            }
            else
            {
                if (qinfo->ReqSourceCount[j] > 0)
                {
                    logging.error(
                        "Quest %u has `ReqSourceId%d` = 0 but "
                        "`ReqSourceCount%d` = %u.",
                        qinfo->GetQuestId(), j + 1, j + 1,
                        qinfo->ReqSourceCount[j]);
                    // no changes, quest ignore this data
                }
            }
        }

        for (int j = 0; j < QUEST_OBJECTIVES_COUNT; ++j)
        {
            if (uint32 id = qinfo->ReqSpell[j])
            {
                SpellEntry const* spellInfo = sSpellStore.LookupEntry(id);
                if (!spellInfo)
                {
                    logging.error(
                        "Quest %u has `ReqSpellCast%d` = %u but spell %u does "
                        "not exist, quest can't be done.",
                        qinfo->GetQuestId(), j + 1, id, id);
                    continue;
                }

                if (!qinfo->ReqCreatureOrGOId[j])
                {
                    bool found = false;
                    for (int k = 0; k < MAX_EFFECT_INDEX; ++k)
                    {
                        if ((spellInfo->Effect[k] ==
                                    SPELL_EFFECT_QUEST_COMPLETE &&
                                uint32(spellInfo->EffectMiscValue[k]) ==
                                    qinfo->QuestId) ||
                            spellInfo->Effect[k] == SPELL_EFFECT_SEND_EVENT)
                        {
                            found = true;
                            break;
                        }
                    }

                    if (found)
                    {
                        if (!qinfo->HasSpecialFlag(
                                QUEST_SPECIAL_FLAG_EXPLORATION_OR_EVENT))
                        {
                            logging.error(
                                "Spell (id: %u) have "
                                "SPELL_EFFECT_QUEST_COMPLETE or "
                                "SPELL_EFFECT_SEND_EVENT for quest %u and "
                                "ReqCreatureOrGOId%d = 0, but quest not have "
                                "flag QUEST_SPECIAL_FLAG_EXPLORATION_OR_EVENT. "
                                "Quest flags or ReqCreatureOrGOId%d must be "
                                "fixed, quest modified to enable objective.",
                                spellInfo->Id, qinfo->QuestId, j + 1, j + 1);

                            // this will prevent quest completing without
                            // objective
                            const_cast<Quest*>(qinfo)->SetSpecialFlag(
                                QUEST_SPECIAL_FLAG_EXPLORATION_OR_EVENT);
                        }
                    }
                    else
                    {
                        logging.error(
                            "Quest %u has `ReqSpellCast%d` = %u and "
                            "ReqCreatureOrGOId%d = 0 but spell %u does not "
                            "have SPELL_EFFECT_QUEST_COMPLETE or "
                            "SPELL_EFFECT_SEND_EVENT effect for this quest, "
                            "quest can't be done.",
                            qinfo->GetQuestId(), j + 1, id, j + 1, id);
                        // no changes, quest can't be done for this requirement
                    }
                }
            }
        }

        for (int j = 0; j < QUEST_OBJECTIVES_COUNT; ++j)
        {
            int32 id = qinfo->ReqCreatureOrGOId[j];
            if (id < 0 && !sGOStorage.LookupEntry<GameObjectInfo>(-id))
            {
                logging.error(
                    "Quest %u has `ReqCreatureOrGOId%d` = %i but gameobject %u "
                    "does not exist, quest can't be done.",
                    qinfo->GetQuestId(), j + 1, id, uint32(-id));
                qinfo->ReqCreatureOrGOId[j] =
                    0; // quest can't be done for this requirement
            }

            if (id > 0 && !sCreatureStorage.LookupEntry<CreatureInfo>(id))
            {
                logging.error(
                    "Quest %u has `ReqCreatureOrGOId%d` = %i but creature with "
                    "entry %u does not exist, quest can't be done.",
                    qinfo->GetQuestId(), j + 1, id, uint32(id));
                qinfo->ReqCreatureOrGOId[j] =
                    0; // quest can't be done for this requirement
            }

            if (id)
            {
                // In fact SpeakTo and Kill are quite same: either you can speak
                // to mob:SpeakTo or you can't:Kill/Cast

                qinfo->SetSpecialFlag(
                    QuestSpecialFlags(QUEST_SPECIAL_FLAG_KILL_OR_CAST |
                                      QUEST_SPECIAL_FLAG_SPEAKTO));

                if (!qinfo->ReqCreatureOrGOCount[j])
                {
                    logging.error(
                        "Quest %u has `ReqCreatureOrGOId%d` = %u but "
                        "`ReqCreatureOrGOCount%d` = 0, quest can't be done.",
                        qinfo->GetQuestId(), j + 1, id, j + 1);
                    // no changes, quest can be incorrectly done, but we already
                    // report this
                }
            }
            else if (qinfo->ReqCreatureOrGOCount[j] > 0)
            {
                logging.error(
                    "Quest %u has `ReqCreatureOrGOId%d` = 0 but "
                    "`ReqCreatureOrGOCount%d` = %u.",
                    qinfo->GetQuestId(), j + 1, j + 1,
                    qinfo->ReqCreatureOrGOCount[j]);
                // no changes, quest ignore this data
            }
        }

        bool choice_found = false;
        for (int j = QUEST_REWARD_CHOICES_COUNT - 1; j >= 0; --j)
        {
            if (uint32 id = qinfo->RewChoiceItemId[j])
            {
                if (!sItemStorage.LookupEntry<ItemPrototype>(id))
                {
                    logging.error(
                        "Quest %u has `RewChoiceItemId%d` = %u but item with "
                        "entry %u does not exist, quest will not reward this "
                        "item.",
                        qinfo->GetQuestId(), j + 1, id, id);
                    qinfo->RewChoiceItemId[j] =
                        0; // no changes, quest will not reward this
                }
                else
                    choice_found = true;

                if (!qinfo->RewChoiceItemCount[j])
                {
                    logging.error(
                        "Quest %u has `RewChoiceItemId%d` = %u but "
                        "`RewChoiceItemCount%d` = 0, quest can't be done.",
                        qinfo->GetQuestId(), j + 1, id, j + 1);
                    // no changes, quest can't be done
                }
            }
            else if (choice_found) // client crash if have gap in item reward
                                   // choices
            {
                logging.error(
                    "Quest %u has `RewChoiceItemId%d` = 0 but "
                    "`RewChoiceItemId%d` = %u, client can crash at like data.",
                    qinfo->GetQuestId(), j + 1, j + 2,
                    qinfo->RewChoiceItemId[j + 1]);
                // fill gap by clone later filled choice
                qinfo->RewChoiceItemId[j] = qinfo->RewChoiceItemId[j + 1];
                qinfo->RewChoiceItemCount[j] = qinfo->RewChoiceItemCount[j + 1];
            }
            else if (qinfo->RewChoiceItemCount[j] > 0)
            {
                logging.error(
                    "Quest %u has `RewChoiceItemId%d` = 0 but "
                    "`RewChoiceItemCount%d` = %u.",
                    qinfo->GetQuestId(), j + 1, j + 1,
                    qinfo->RewChoiceItemCount[j]);
                // no changes, quest ignore this data
            }
        }

        for (int j = 0; j < QUEST_REWARDS_COUNT; ++j)
        {
            if (uint32 id = qinfo->RewItemId[j])
            {
                if (!sItemStorage.LookupEntry<ItemPrototype>(id))
                {
                    logging.error(
                        "Quest %u has `RewItemId%d` = %u but item with entry "
                        "%u does not exist, quest will not reward this item.",
                        qinfo->GetQuestId(), j + 1, id, id);
                    qinfo->RewItemId[j] =
                        0; // no changes, quest will not reward this item
                }

                if (!qinfo->RewItemCount[j])
                {
                    logging.error(
                        "Quest %u has `RewItemId%d` = %u but `RewItemCount%d` "
                        "= 0, quest will not reward this item.",
                        qinfo->GetQuestId(), j + 1, id, j + 1);
                    // no changes
                }
            }
            else if (qinfo->RewItemCount[j] > 0)
            {
                logging.error(
                    "Quest %u has `RewItemId%d` = 0 but `RewItemCount%d` = %u.",
                    qinfo->GetQuestId(), j + 1, j + 1, qinfo->RewItemCount[j]);
                // no changes, quest ignore this data
            }
        }

        for (int j = 0; j < QUEST_REPUTATIONS_COUNT; ++j)
        {
            if (qinfo->RewRepFaction[j])
            {
                if (!qinfo->RewRepValue[j])
                    logging.error(
                        "Quest %u has `RewRepFaction%d` = %u but "
                        "`RewRepValue%d` = 0, quest will not reward this "
                        "reputation.",
                        qinfo->GetQuestId(), j + 1, qinfo->RewRepValue[j],
                        j + 1);

                if (!sFactionStore.LookupEntry(qinfo->RewRepFaction[j]))
                {
                    logging.error(
                        "Quest %u has `RewRepFaction%d` = %u but raw faction "
                        "(faction.dbc) %u does not exist, quest will not "
                        "reward reputation for this faction.",
                        qinfo->GetQuestId(), j + 1, qinfo->RewRepFaction[j],
                        qinfo->RewRepFaction[j]);
                    qinfo->RewRepFaction[j] = 0; // quest will not reward this
                }
            }
            else if (qinfo->RewRepValue[j] != 0)
            {
                logging.error(
                    "Quest %u has `RewRepFaction%d` = 0 but `RewRepValue%d` = "
                    "%i.",
                    qinfo->GetQuestId(), j + 1, j + 1, qinfo->RewRepValue[j]);
                // no changes, quest ignore this data
            }
        }

        if (qinfo->RewSpell)
        {
            SpellEntry const* spellInfo =
                sSpellStore.LookupEntry(qinfo->RewSpell);

            if (!spellInfo)
            {
                logging.error(
                    "Quest %u has `RewSpell` = %u but spell %u does not exist, "
                    "spell removed as display reward.",
                    qinfo->GetQuestId(), qinfo->RewSpell, qinfo->RewSpell);
                qinfo->RewSpell =
                    0; // no spell reward will display for this quest
            }
            else if (!SpellMgr::IsSpellValid(spellInfo))
            {
                logging.error(
                    "Quest %u has `RewSpell` = %u but spell %u is broken, "
                    "quest will not have a spell reward.",
                    qinfo->GetQuestId(), qinfo->RewSpell, qinfo->RewSpell);
                qinfo->RewSpell =
                    0; // no spell reward will display for this quest
            }
            else if (GetTalentSpellCost(qinfo->RewSpell))
            {
                logging.error(
                    "Quest %u has `RewSpell` = %u but spell %u is talent, "
                    "quest will not have a spell reward.",
                    qinfo->GetQuestId(), qinfo->RewSpell, qinfo->RewSpell);
                qinfo->RewSpell =
                    0; // no spell reward will display for this quest
            }
        }

        if (qinfo->RewSpellCast)
        {
            SpellEntry const* spellInfo =
                sSpellStore.LookupEntry(qinfo->RewSpellCast);

            if (!spellInfo)
            {
                logging.error(
                    "Quest %u has `RewSpellCast` = %u but spell %u does not "
                    "exist, quest will not have a spell reward.",
                    qinfo->GetQuestId(), qinfo->RewSpellCast,
                    qinfo->RewSpellCast);
                qinfo->RewSpellCast = 0; // no spell will be casted on player
            }
            else if (!SpellMgr::IsSpellValid(spellInfo))
            {
                logging.error(
                    "Quest %u has `RewSpellCast` = %u but spell %u is broken, "
                    "quest will not have a spell reward.",
                    qinfo->GetQuestId(), qinfo->RewSpellCast,
                    qinfo->RewSpellCast);
                qinfo->RewSpellCast = 0; // no spell will be casted on player
            }
            else if (GetTalentSpellCost(qinfo->RewSpellCast))
            {
                logging.error(
                    "Quest %u has `RewSpell` = %u but spell %u is talent, "
                    "quest will not have a spell reward.",
                    qinfo->GetQuestId(), qinfo->RewSpellCast,
                    qinfo->RewSpellCast);
                qinfo->RewSpellCast = 0; // no spell will be casted on player
            }
        }

        if (qinfo->RewMailTemplateId)
        {
            if (!sMailTemplateStore.LookupEntry(qinfo->RewMailTemplateId))
            {
                logging.error(
                    "Quest %u has `RewMailTemplateId` = %u but mail template  "
                    "%u does not exist, quest will not have a mail reward.",
                    qinfo->GetQuestId(), qinfo->RewMailTemplateId,
                    qinfo->RewMailTemplateId);
                qinfo->RewMailTemplateId = 0; // no mail will send to player
                qinfo->RewMailDelaySecs = 0;  // no mail will send to player
            }
            else if (usedMailTemplates.find(qinfo->RewMailTemplateId) !=
                     usedMailTemplates.end())
            {
                std::map<uint32, uint32>::const_iterator used_mt_itr =
                    usedMailTemplates.find(qinfo->RewMailTemplateId);
                logging.error(
                    "Quest %u has `RewMailTemplateId` = %u but mail template  "
                    "%u already used for quest %u, quest will not have a mail "
                    "reward.",
                    qinfo->GetQuestId(), qinfo->RewMailTemplateId,
                    qinfo->RewMailTemplateId, used_mt_itr->second);
                qinfo->RewMailTemplateId = 0; // no mail will send to player
                qinfo->RewMailDelaySecs = 0;  // no mail will send to player
            }
            else
                usedMailTemplates[qinfo->RewMailTemplateId] =
                    qinfo->GetQuestId();
        }

        if (qinfo->NextQuestInChain)
        {
            auto qNextItr = mQuestTemplates.find(qinfo->NextQuestInChain);
            if (qNextItr == mQuestTemplates.end())
            {
                logging.error(
                    "Quest %u has `NextQuestInChain` = %u but quest %u does "
                    "not exist, quest chain will not work.",
                    qinfo->GetQuestId(), qinfo->NextQuestInChain,
                    qinfo->NextQuestInChain);
                qinfo->NextQuestInChain = 0;
            }
            else
                qNextItr->second->prevChainQuests.push_back(
                    qinfo->GetQuestId());
        }

        // fill additional data stores
        if (qinfo->PrevQuestId)
        {
            if (mQuestTemplates.find(abs(qinfo->GetPrevQuestId())) ==
                mQuestTemplates.end())
            {
                logging.error("Quest %d has PrevQuestId %i, but no such quest",
                    qinfo->GetQuestId(), qinfo->GetPrevQuestId());
            }
            else
            {
                qinfo->prevQuests.push_back(qinfo->PrevQuestId);
            }
        }

        if (qinfo->NextQuestId)
        {
            auto qNextItr = mQuestTemplates.find(abs(qinfo->GetNextQuestId()));
            if (qNextItr == mQuestTemplates.end())
            {
                logging.error("Quest %d has NextQuestId %i, but no such quest",
                    qinfo->GetQuestId(), qinfo->GetNextQuestId());
            }
            else
            {
                int32 signedQuestId = qinfo->NextQuestId < 0 ?
                                          -int32(qinfo->GetQuestId()) :
                                          int32(qinfo->GetQuestId());
                qNextItr->second->prevQuests.push_back(signedQuestId);
            }
        }

        if (qinfo->ExclusiveGroup)
            m_ExclusiveQuestGroups.insert(ExclusiveQuestGroupsMap::value_type(
                qinfo->ExclusiveGroup, qinfo->GetQuestId()));

        if (qinfo->LimitTime)
            qinfo->SetSpecialFlag(QUEST_SPECIAL_FLAG_TIMED);
    }

    // check QUEST_SPECIAL_FLAG_EXPLORATION_OR_EVENT for spell with
    // SPELL_EFFECT_QUEST_COMPLETE
    for (uint32 i = 0; i < sSpellStore.GetNumRows(); ++i)
    {
        SpellEntry const* spellInfo = sSpellStore.LookupEntry(i);
        if (!spellInfo)
            continue;

        for (int j = 0; j < MAX_EFFECT_INDEX; ++j)
        {
            if (spellInfo->Effect[j] != SPELL_EFFECT_QUEST_COMPLETE)
                continue;

            uint32 quest_id = spellInfo->EffectMiscValue[j];

            Quest const* quest = GetQuestTemplate(quest_id);

            // some quest referenced in spells not exist (outdated spells)
            if (!quest)
                continue;

            if (!quest->HasSpecialFlag(QUEST_SPECIAL_FLAG_EXPLORATION_OR_EVENT))
            {
                logging.error(
                    "Spell (id: %u) have SPELL_EFFECT_QUEST_COMPLETE for quest "
                    "%u , but quest does not have SpecialFlags "
                    "QUEST_SPECIAL_FLAG_EXPLORATION_OR_EVENT (2) set. Quest "
                    "SpecialFlags should be corrected to enable this "
                    "objective.",
                    spellInfo->Id, quest_id);

                // The below forced alteration has been disabled because of
                // spell 33824 / quest 10162.
                // A startup error will still occur with proper data in
                // quest_template, but it will be possible to sucessfully
                // complete the quest with the expected data.

                // this will prevent quest completing without objective
                // const_cast<Quest*>(quest)->SetSpecialFlag(QUEST_SPECIAL_FLAG_EXPLORATION_OR_EVENT);
            }
        }
    }

    logging.info("Loaded %lu quests definitions\n",
        (unsigned long)mQuestTemplates.size());
}

void ObjectMgr::LoadQuestLocales()
{
    mQuestLocaleMap.clear(); // need for reload case

    QueryResult* result = WorldDatabase.Query(
        "SELECT entry,"
        "Title_loc1,Details_loc1,Objectives_loc1,OfferRewardText_loc1,"
        "RequestItemsText_loc1,EndText_loc1,ObjectiveText1_loc1,ObjectiveText2_"
        "loc1,ObjectiveText3_loc1,ObjectiveText4_loc1,"
        "Title_loc2,Details_loc2,Objectives_loc2,OfferRewardText_loc2,"
        "RequestItemsText_loc2,EndText_loc2,ObjectiveText1_loc2,ObjectiveText2_"
        "loc2,ObjectiveText3_loc2,ObjectiveText4_loc2,"
        "Title_loc3,Details_loc3,Objectives_loc3,OfferRewardText_loc3,"
        "RequestItemsText_loc3,EndText_loc3,ObjectiveText1_loc3,ObjectiveText2_"
        "loc3,ObjectiveText3_loc3,ObjectiveText4_loc3,"
        "Title_loc4,Details_loc4,Objectives_loc4,OfferRewardText_loc4,"
        "RequestItemsText_loc4,EndText_loc4,ObjectiveText1_loc4,ObjectiveText2_"
        "loc4,ObjectiveText3_loc4,ObjectiveText4_loc4,"
        "Title_loc5,Details_loc5,Objectives_loc5,OfferRewardText_loc5,"
        "RequestItemsText_loc5,EndText_loc5,ObjectiveText1_loc5,ObjectiveText2_"
        "loc5,ObjectiveText3_loc5,ObjectiveText4_loc5,"
        "Title_loc6,Details_loc6,Objectives_loc6,OfferRewardText_loc6,"
        "RequestItemsText_loc6,EndText_loc6,ObjectiveText1_loc6,ObjectiveText2_"
        "loc6,ObjectiveText3_loc6,ObjectiveText4_loc6,"
        "Title_loc7,Details_loc7,Objectives_loc7,OfferRewardText_loc7,"
        "RequestItemsText_loc7,EndText_loc7,ObjectiveText1_loc7,ObjectiveText2_"
        "loc7,ObjectiveText3_loc7,ObjectiveText4_loc7,"
        "Title_loc8,Details_loc8,Objectives_loc8,OfferRewardText_loc8,"
        "RequestItemsText_loc8,EndText_loc8,ObjectiveText1_loc8,ObjectiveText2_"
        "loc8,ObjectiveText3_loc8,ObjectiveText4_loc8"
        " FROM locales_quest");

    if (!result)
    {
        logging.info(
            "Loaded 0 Quest locale strings. DB table `locales_quest` is "
            "empty.");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 entry = fields[0].GetUInt32();

        if (!GetQuestTemplate(entry))
        {
            logging.warning(
                "Table `locales_quest` has data for nonexistent quest entry "
                "%u, skipped.",
                entry);
            continue;
        }

        QuestLocale& data = mQuestLocaleMap[entry];

        for (int i = 1; i < MAX_LOCALE; ++i)
        {
            std::string str = fields[1 + 10 * (i - 1)].GetCppString();
            if (!str.empty())
            {
                int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    if ((int32)data.Title.size() <= idx)
                        data.Title.resize(idx + 1);

                    data.Title[idx] = str;
                }
            }
            str = fields[1 + 10 * (i - 1) + 1].GetCppString();
            if (!str.empty())
            {
                int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    if ((int32)data.Details.size() <= idx)
                        data.Details.resize(idx + 1);

                    data.Details[idx] = str;
                }
            }
            str = fields[1 + 10 * (i - 1) + 2].GetCppString();
            if (!str.empty())
            {
                int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    if ((int32)data.Objectives.size() <= idx)
                        data.Objectives.resize(idx + 1);

                    data.Objectives[idx] = str;
                }
            }
            str = fields[1 + 10 * (i - 1) + 3].GetCppString();
            if (!str.empty())
            {
                int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    if ((int32)data.OfferRewardText.size() <= idx)
                        data.OfferRewardText.resize(idx + 1);

                    data.OfferRewardText[idx] = str;
                }
            }
            str = fields[1 + 10 * (i - 1) + 4].GetCppString();
            if (!str.empty())
            {
                int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    if ((int32)data.RequestItemsText.size() <= idx)
                        data.RequestItemsText.resize(idx + 1);

                    data.RequestItemsText[idx] = str;
                }
            }
            str = fields[1 + 10 * (i - 1) + 5].GetCppString();
            if (!str.empty())
            {
                int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    if ((int32)data.EndText.size() <= idx)
                        data.EndText.resize(idx + 1);

                    data.EndText[idx] = str;
                }
            }
            for (int k = 0; k < 4; ++k)
            {
                str = fields[1 + 10 * (i - 1) + 6 + k].GetCppString();
                if (!str.empty())
                {
                    int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                    if (idx >= 0)
                    {
                        if ((int32)data.ObjectiveText[k].size() <= idx)
                            data.ObjectiveText[k].resize(idx + 1);

                        data.ObjectiveText[k][idx] = str;
                    }
                }
            }
        }
    } while (result->NextRow());

    delete result;

    logging.info("Loaded %lu Quest locale strings",
        (unsigned long)mQuestLocaleMap.size());
}

void ObjectMgr::LoadPetCreateSpells()
{
    QueryResult* result = WorldDatabase.Query(
        "SELECT entry, Spell1, Spell2, Spell3, Spell4, auto_cast1, auto_cast2, "
        "auto_cast3, auto_cast4 FROM petcreateinfo_spell");
    if (!result)
    {
        BarGoLink bar(1);
        bar.step();

        logging.info("Loaded 0 pet create spells\n");
        // logging.error("`petcreateinfo_spell` table is
        // empty!");
        return;
    }

    uint32 count = 0;

    BarGoLink bar(result->GetRowCount());

    mPetCreateSpell.clear();

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 creature_id = fields[0].GetUInt32();

        if (!creature_id)
        {
            logging.error(
                "Creature id %u listed in `petcreateinfo_spell` not exist.",
                creature_id);
            continue;
        }

        CreatureInfo const* cInfo =
            sCreatureStorage.LookupEntry<CreatureInfo>(creature_id);
        if (!cInfo)
        {
            logging.error(
                "Creature id %u listed in `petcreateinfo_spell` not exist.",
                creature_id);
            continue;
        }

        if (/*CreatureSpellDataEntry const* petSpellEntry = */ cInfo
                    ->PetSpellDataId ?
                sCreatureSpellDataStore.LookupEntry(cInfo->PetSpellDataId) :
                nullptr)
        {
            logging.error(
                "Creature id %u listed in `petcreateinfo_spell` have set "
                "`PetSpellDataId` field and will use its instead, skip.",
                creature_id);
            continue;
        }

        PetCreateSpellEntry PetCreateSpell;

        bool have_spell = false;
        bool have_spell_db = false;
        for (int i = 0; i < 4; ++i)
        {
            PetCreateSpell.spellid[i] = fields[i + 1].GetUInt32();
            PetCreateSpell.auto_cast[i] = fields[i + 5].GetBool();

            if (!PetCreateSpell.spellid[i])
                continue;

            have_spell_db = true;

            SpellEntry const* i_spell =
                sSpellStore.LookupEntry(PetCreateSpell.spellid[i]);
            if (!i_spell)
            {
                logging.error(
                    "Spell %u listed in `petcreateinfo_spell` does not exist",
                    PetCreateSpell.spellid[i]);
                PetCreateSpell.spellid[i] = 0;
                continue;
            }

            have_spell = true;
        }

        if (!have_spell_db)
        {
            logging.error(
                "Creature %u listed in `petcreateinfo_spell` have only 0 spell "
                "data, why it listed?",
                creature_id);
            continue;
        }

        if (!have_spell)
            continue;

        mPetCreateSpell[creature_id] = PetCreateSpell;
        ++count;
    } while (result->NextRow());

    delete result;

    // cache spell->learn spell map for use in next loop
    std::map<uint32, uint32> learnCache;
    for (uint32 spell_id = 1; spell_id < sSpellStore.GetNumRows(); ++spell_id)
    {
        SpellEntry const* spellproto = sSpellStore.LookupEntry(spell_id);
        if (!spellproto)
            continue;

        if (spellproto->Effect[0] != SPELL_EFFECT_LEARN_SPELL &&
            spellproto->Effect[0] != SPELL_EFFECT_LEARN_PET_SPELL)
            continue;

        if (!spellproto->EffectTriggerSpell[0])
            continue;

        learnCache[spellproto->EffectTriggerSpell[0]] = spellproto->Id;
    }

    // fill data from DBC as more correct source if available
    uint32 dcount = 0;
    for (uint32 cr_id = 1; cr_id < sCreatureStorage.MaxEntry; ++cr_id)
    {
        CreatureInfo const* cInfo =
            sCreatureStorage.LookupEntry<CreatureInfo>(cr_id);
        if (!cInfo)
            continue;

        CreatureSpellDataEntry const* petSpellEntry =
            cInfo->PetSpellDataId ?
                sCreatureSpellDataStore.LookupEntry(cInfo->PetSpellDataId) :
                nullptr;
        if (!petSpellEntry)
            continue;

        PetCreateSpellEntry PetCreateSpell;
        for (int i = 0; i < MAX_CREATURE_SPELL_DATA_SLOT; ++i)
        {
            uint32 petspell_id = petSpellEntry->spellId[i];
            if (petspell_id)
            {
                // in dbc stored spell for pet use, but for teaching work we
                // need learn spell ids
                std::map<uint32, uint32>::const_iterator cache_itr =
                    learnCache.find(petspell_id);
                if (cache_itr != learnCache.end())
                    petspell_id = cache_itr->second;
            }

            PetCreateSpell.spellid[i] = petspell_id;
        }

        mPetCreateSpell[cr_id] = PetCreateSpell;
        ++dcount;
    }

    logging.info("Loaded %u pet create spells from table and %u from DBC\n",
        count, dcount);
}

void ObjectMgr::LoadItemTexts()
{
    QueryResult* result =
        CharacterDatabase.Query("SELECT id, text FROM item_text");

    uint32 count = 0;

    if (!result)
    {
        logging.info("Loaded %u item pages\n", count);
        return;
    }

    BarGoLink bar(result->GetRowCount());

    Field* fields;
    do
    {
        bar.step();

        fields = result->Fetch();

        mItemTexts[fields[0].GetUInt32()] = fields[1].GetCppString();

        ++count;

    } while (result->NextRow());

    delete result;

    logging.info("Loaded %u item texts\n", count);
}

void ObjectMgr::LoadPageTexts()
{
    sPageTextStore.Free(); // for reload case

    sPageTextStore.Load();
    logging.info("Loaded %u page texts\n", sPageTextStore.RecordCount);

    for (uint32 i = 1; i < sPageTextStore.MaxEntry; ++i)
    {
        // check data correctness
        PageText const* page = sPageTextStore.LookupEntry<PageText>(i);
        if (!page)
            continue;

        if (page->Next_Page &&
            !sPageTextStore.LookupEntry<PageText>(page->Next_Page))
        {
            logging.error(
                "Page text (Id: %u) has not existing next page (Id:%u)", i,
                page->Next_Page);
            continue;
        }

        // detect circular reference
        std::set<uint32> checkedPages;
        for (PageText const* pageItr = page; pageItr;
             pageItr = sPageTextStore.LookupEntry<PageText>(pageItr->Next_Page))
        {
            if (!pageItr->Next_Page)
                break;
            checkedPages.insert(pageItr->Page_ID);
            if (checkedPages.find(pageItr->Next_Page) != checkedPages.end())
            {
                std::ostringstream ss;
                ss << "The text page(s) ";
                for (const auto& checkedPage : checkedPages)
                    ss << checkedPage << " ";
                ss << "create(s) a circular reference, which can cause the "
                      "server to freeze. Changing Next_Page of page "
                   << pageItr->Page_ID << " to 0";
                logging.error("%s", ss.str().c_str());
                const_cast<PageText*>(pageItr)->Next_Page = 0;
                break;
            }
        }
    }
}

void ObjectMgr::LoadPageTextLocales()
{
    mPageTextLocaleMap.clear(); // need for reload case

    QueryResult* result = WorldDatabase.Query(
        "SELECT "
        "entry,text_loc1,text_loc2,text_loc3,text_loc4,text_loc5,text_loc6,"
        "text_loc7,text_loc8 FROM locales_page_text");

    if (!result)
    {
        logging.info(
            "Loaded 0 PageText locale strings. DB table `locales_page_text` "
            "is empty.");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 entry = fields[0].GetUInt32();

        if (!sPageTextStore.LookupEntry<PageText>(entry))
        {
            logging.warning(
                "Table `locales_page_text` has data for nonexistent page text "
                "entry %u, skipped.",
                entry);
            continue;
        }

        PageTextLocale& data = mPageTextLocaleMap[entry];

        for (int i = 1; i < MAX_LOCALE; ++i)
        {
            std::string str = fields[i].GetCppString();
            if (str.empty())
                continue;

            int idx = GetOrNewIndexForLocale(LocaleConstant(i));
            if (idx >= 0)
            {
                if ((int32)data.Text.size() <= idx)
                    data.Text.resize(idx + 1);

                data.Text[idx] = str;
            }
        }

    } while (result->NextRow());

    delete result;

    logging.info("Loaded %lu PageText locale strings",
        (unsigned long)mPageTextLocaleMap.size());
}

struct SQLInstanceLoader : public SQLStorageLoaderBase<SQLInstanceLoader>
{
    template <class D>
    void convert_from_str(uint32 /*field_pos*/, char const* src, D& dst)
    {
        dst = D(sScriptMgr::Instance()->GetScriptId(src));
    }
};

void ObjectMgr::LoadInstanceTemplate()
{
    SQLInstanceLoader loader;
    loader.Load(sInstanceTemplate);

    for (uint32 i = 0; i < sInstanceTemplate.MaxEntry; i++)
    {
        InstanceTemplate const* temp = GetInstanceTemplate(i);
        if (!temp)
            continue;

        MapEntry const* mapEntry = sMapStore.LookupEntry(temp->map);
        if (!mapEntry)
        {
            logging.error(
                "ObjectMgr::LoadInstanceTemplate: bad mapid %d for template!",
                temp->map);
            sInstanceTemplate.EraseEntry(i);
            continue;
        }

        if (!mapEntry->Instanceable())
        {
            logging.error(
                "ObjectMgr::LoadInstanceTemplate: non-instanceable mapid %d "
                "for template!",
                temp->map);
            sInstanceTemplate.EraseEntry(i);
            continue;
        }

        if (temp->parent > 0)
        {
            // check existence
            MapEntry const* parentEntry = sMapStore.LookupEntry(temp->parent);
            if (!parentEntry)
            {
                logging.error(
                    "ObjectMgr::LoadInstanceTemplate: bad parent map id %u for "
                    "instance template %d template!",
                    parentEntry->MapID, temp->map);
                const_cast<InstanceTemplate*>(temp)->parent = 0;
                continue;
            }

            if (parentEntry->IsContinent())
            {
                logging.error(
                    "ObjectMgr::LoadInstanceTemplate: parent point to "
                    "continent map id %u for instance template %d template, "
                    "ignored, need be set only for non-continent parents!",
                    parentEntry->MapID, temp->map);
                const_cast<InstanceTemplate*>(temp)->parent = 0;
                continue;
            }
        }

        if (mapEntry->HasResetTime())
        {
            if (temp->reset_delay == 0)
            {
                // use defaults from the DBC
                if (mapEntry->SupportsHeroicMode())
                {
                    const_cast<InstanceTemplate*>(temp)->reset_delay =
                        mapEntry->resetTimeHeroic / DAY;
                }
                else if (mapEntry->resetTimeRaid &&
                         mapEntry->map_type == MAP_RAID)
                {
                    const_cast<InstanceTemplate*>(temp)->reset_delay =
                        mapEntry->resetTimeRaid / DAY;
                }
            }

            // the reset_delay must be at least one day
            const_cast<InstanceTemplate*>(temp)->reset_delay =
                std::max((uint32)1,
                    (uint32)(temp->reset_delay *
                             sWorld::Instance()->getConfig(
                                 CONFIG_FLOAT_RATE_INSTANCE_RESET_TIME)));
        }
    }

    logging.info("Loaded %u Instance Template definitions\n",
        sInstanceTemplate.RecordCount);
}

struct SQLWorldLoader : public SQLStorageLoaderBase<SQLWorldLoader>
{
    template <class D>
    void convert_from_str(uint32 /*field_pos*/, char const* src, D& dst)
    {
        dst = D(sScriptMgr::Instance()->GetScriptId(src));
    }
};

void ObjectMgr::LoadWorldTemplate()
{
    SQLWorldLoader loader;
    loader.Load(sWorldTemplate, false);

    for (uint32 i = 0; i < sWorldTemplate.MaxEntry; ++i)
    {
        WorldTemplate const* temp = GetWorldTemplate(i);
        if (!temp)
            continue;

        MapEntry const* mapEntry = sMapStore.LookupEntry(temp->map);
        if (!mapEntry)
        {
            logging.error(
                "ObjectMgr::LoadWorldTemplate: bad mapid %d for template!",
                temp->map);
            sWorldTemplate.EraseEntry(i);
            continue;
        }

        if (mapEntry->Instanceable())
        {
            logging.error(
                "ObjectMgr::LoadWorldTemplate: instanceable mapid %d for "
                "template!",
                temp->map);
            sWorldTemplate.EraseEntry(i);
            continue;
        }
    }

    logging.info(
        "Loaded %u World Template definitions\n", sWorldTemplate.RecordCount);
}

void ObjectMgr::LoadConditions()
{
    SQLWorldLoader loader;
    loader.Load(sConditionStorage, false);

    for (uint32 i = 0; i < sConditionStorage.MaxEntry; ++i)
    {
        const PlayerCondition* condition =
            sConditionStorage.LookupEntry<PlayerCondition>(i);
        if (!condition)
            continue;

        if (!condition->IsValid())
        {
            logging.error(
                "ObjectMgr::LoadConditions: invalid condition_entry %u, skip",
                i);
            sConditionStorage.EraseEntry(i);
            continue;
        }
    }

    logging.info(
        "Loaded %u Condition definitions\n", sConditionStorage.RecordCount);
}

GossipText const* ObjectMgr::GetGossipText(uint32 Text_ID) const
{
    auto itr = mGossipText.find(Text_ID);
    if (itr != mGossipText.end())
        return &itr->second;
    return nullptr;
}

void ObjectMgr::LoadGossipText()
{
    QueryResult* result = WorldDatabase.Query("SELECT * FROM npc_text");

    int count = 0;
    if (!result)
    {
        logging.info("Loaded %u npc texts\n", count);
        return;
    }

    int cic;

    BarGoLink bar(result->GetRowCount());

    do
    {
        ++count;
        cic = 0;

        Field* fields = result->Fetch();

        bar.step();

        uint32 Text_ID = fields[cic++].GetUInt32();
        if (!Text_ID)
        {
            logging.error(
                "Table `npc_text` has record wit reserved id 0, ignore.");
            continue;
        }

        GossipText& gText = mGossipText[Text_ID];

        for (int i = 0; i < MAX_GOSSIP_TEXT_OPTIONS; ++i)
        {
            gText.Options[i].Text_0 = fields[cic++].GetCppString();
            gText.Options[i].Text_1 = fields[cic++].GetCppString();

            gText.Options[i].Language = fields[cic++].GetUInt32();
            gText.Options[i].Probability = fields[cic++].GetFloat();

            for (int j = 0; j < 3; ++j)
            {
                gText.Options[i].Emotes[j]._Delay = fields[cic++].GetUInt32();
                gText.Options[i].Emotes[j]._Emote = fields[cic++].GetUInt32();
            }
        }
    } while (result->NextRow());

    logging.info("Loaded %u npc texts\n", count);
    delete result;
}

void ObjectMgr::LoadGossipTextLocales()
{
    mNpcTextLocaleMap.clear(); // need for reload case

    QueryResult* result = WorldDatabase.Query(
        "SELECT entry,"
        "Text0_0_loc1,Text0_1_loc1,Text1_0_loc1,Text1_1_loc1,Text2_0_loc1,"
        "Text2_1_loc1,Text3_0_loc1,Text3_1_loc1,Text4_0_loc1,Text4_1_loc1,"
        "Text5_0_loc1,Text5_1_loc1,Text6_0_loc1,Text6_1_loc1,Text7_0_loc1,"
        "Text7_1_loc1,"
        "Text0_0_loc2,Text0_1_loc2,Text1_0_loc2,Text1_1_loc2,Text2_0_loc2,"
        "Text2_1_loc2,Text3_0_loc2,Text3_1_loc1,Text4_0_loc2,Text4_1_loc2,"
        "Text5_0_loc2,Text5_1_loc2,Text6_0_loc2,Text6_1_loc2,Text7_0_loc2,"
        "Text7_1_loc2,"
        "Text0_0_loc3,Text0_1_loc3,Text1_0_loc3,Text1_1_loc3,Text2_0_loc3,"
        "Text2_1_loc3,Text3_0_loc3,Text3_1_loc1,Text4_0_loc3,Text4_1_loc3,"
        "Text5_0_loc3,Text5_1_loc3,Text6_0_loc3,Text6_1_loc3,Text7_0_loc3,"
        "Text7_1_loc3,"
        "Text0_0_loc4,Text0_1_loc4,Text1_0_loc4,Text1_1_loc4,Text2_0_loc4,"
        "Text2_1_loc4,Text3_0_loc4,Text3_1_loc1,Text4_0_loc4,Text4_1_loc4,"
        "Text5_0_loc4,Text5_1_loc4,Text6_0_loc4,Text6_1_loc4,Text7_0_loc4,"
        "Text7_1_loc4,"
        "Text0_0_loc5,Text0_1_loc5,Text1_0_loc5,Text1_1_loc5,Text2_0_loc5,"
        "Text2_1_loc5,Text3_0_loc5,Text3_1_loc1,Text4_0_loc5,Text4_1_loc5,"
        "Text5_0_loc5,Text5_1_loc5,Text6_0_loc5,Text6_1_loc5,Text7_0_loc5,"
        "Text7_1_loc5,"
        "Text0_0_loc6,Text0_1_loc6,Text1_0_loc6,Text1_1_loc6,Text2_0_loc6,"
        "Text2_1_loc6,Text3_0_loc6,Text3_1_loc1,Text4_0_loc6,Text4_1_loc6,"
        "Text5_0_loc6,Text5_1_loc6,Text6_0_loc6,Text6_1_loc6,Text7_0_loc6,"
        "Text7_1_loc6,"
        "Text0_0_loc7,Text0_1_loc7,Text1_0_loc7,Text1_1_loc7,Text2_0_loc7,"
        "Text2_1_loc7,Text3_0_loc7,Text3_1_loc1,Text4_0_loc7,Text4_1_loc7,"
        "Text5_0_loc7,Text5_1_loc7,Text6_0_loc7,Text6_1_loc7,Text7_0_loc7,"
        "Text7_1_loc7, "
        "Text0_0_loc8,Text0_1_loc8,Text1_0_loc8,Text1_1_loc8,Text2_0_loc8,"
        "Text2_1_loc8,Text3_0_loc8,Text3_1_loc1,Text4_0_loc8,Text4_1_loc8,"
        "Text5_0_loc8,Text5_1_loc8,Text6_0_loc8,Text6_1_loc8,Text7_0_loc8,"
        "Text7_1_loc8 "
        " FROM locales_npc_text");

    if (!result)
    {
        logging.info(
            "Loaded 0 Quest locale strings. DB table `locales_npc_text` is "
            "empty.");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 entry = fields[0].GetUInt32();

        if (!GetGossipText(entry))
        {
            logging.warning(
                "Table `locales_npc_text` has data for nonexistent gossip text "
                "entry %u, skipped.",
                entry);
            continue;
        }

        NpcTextLocale& data = mNpcTextLocaleMap[entry];

        for (int i = 1; i < MAX_LOCALE; ++i)
        {
            for (int j = 0; j < 8; ++j)
            {
                std::string str0 =
                    fields[1 + 8 * 2 * (i - 1) + 2 * j].GetCppString();
                if (!str0.empty())
                {
                    int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                    if (idx >= 0)
                    {
                        if ((int32)data.Text_0[j].size() <= idx)
                            data.Text_0[j].resize(idx + 1);

                        data.Text_0[j][idx] = str0;
                    }
                }
                std::string str1 =
                    fields[1 + 8 * 2 * (i - 1) + 2 * j + 1].GetCppString();
                if (!str1.empty())
                {
                    int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                    if (idx >= 0)
                    {
                        if ((int32)data.Text_1[j].size() <= idx)
                            data.Text_1[j].resize(idx + 1);

                        data.Text_1[j][idx] = str1;
                    }
                }
            }
        }
    } while (result->NextRow());

    delete result;

    logging.info("Loaded %lu NpcText locale strings",
        (unsigned long)mNpcTextLocaleMap.size());
}

// not very fast function but it is called only once a day, or on starting-up
void ObjectMgr::ReturnOrDeleteOldMails(bool serverUp)
{
    time_t basetime = WorldTimer::time_no_syscall();
    LOG_DEBUG(logging,
        "Returning mails current time: hour: %d, minute: %d, second: %d ",
        localtime(&basetime)->tm_hour, localtime(&basetime)->tm_min,
        localtime(&basetime)->tm_sec);
    // delete all old mails without item and without body immediately, if
    // starting server
    if (!serverUp)
        CharacterDatabase.PExecute(
            "DELETE FROM mail WHERE expire_time < '" UI64FMTD
            "' AND has_items = '0' AND itemTextId = 0",
            (uint64)basetime);

    std::unique_ptr<QueryResult> result(CharacterDatabase.PQuery(
        "SELECT "
        "id,messageType,sender,receiver,itemTextId,has_items,expire_time,cod,"
        "checked,mailTemplateId FROM mail WHERE expire_time < '" UI64FMTD "'",
        (uint64)basetime));
    if (!result)
    {
        logging.info(
            "Only expired mails (need to be return or delete) or DB table "
            "`mail` is empty.\n");
        return; // any mails need to be returned or deleted
    }

    BarGoLink bar(result->GetRowCount());
    uint32 count = 0;
    Field* fields;

    do
    {
        bar.step();

        fields = result->Fetch();
        auto m = new Mail;
        m->messageID = fields[0].GetUInt32();
        m->messageType = fields[1].GetUInt8();
        m->sender = fields[2].GetUInt32();
        m->receiverGuid = ObjectGuid(HIGHGUID_PLAYER, fields[3].GetUInt32());
        bool has_items = fields[5].GetBool();
        m->expire_time = (time_t)fields[6].GetUInt64();
        m->deliver_time = 0;
        m->COD = fields[7].GetUInt32();
        m->checked = fields[8].GetUInt32();
        m->mailTemplateId = fields[9].GetInt16();
        m->state = MAIL_STATE_UNCHANGED;

        Player* pl = nullptr;
        if (serverUp)
            pl = GetPlayer(m->receiverGuid);
        if (pl)
        { // this code will run very improbably (the time is between 4 and 5 am,
            // in game is online a player, who has old mail
            // his in mailbox and he has already listed his mails )
            delete m;
            continue;
        }
        // delete or return mail:
        if (has_items)
        {
            std::unique_ptr<QueryResult> resultItems(CharacterDatabase.PQuery(
                "SELECT item_guid,item_template FROM mail_items WHERE "
                "mail_id='%u'",
                m->messageID));
            if (resultItems)
            {
                do
                {
                    Field* fields2 = resultItems->Fetch();

                    uint32 item_guid_low = fields2[0].GetUInt32();
                    uint32 item_template = fields2[1].GetUInt32();

                    m->AddItem(item_guid_low, item_template);
                } while (resultItems->NextRow());
            }
            // if it is mail from non-player, or if it's already return mail, it
            // shouldn't be returned, but deleted
            if (m->messageType != MAIL_NORMAL ||
                (m->checked &
                    (MAIL_CHECK_MASK_COD_PAYMENT | MAIL_CHECK_MASK_RETURNED)))
            {
                // mail open and then not returned
                for (auto& elem : m->items)
                    CharacterDatabase.PExecute(
                        "DELETE FROM item_instance WHERE guid = '%u'",
                        elem.item_guid);
            }
            else
            {
                // mail will be returned:
                CharacterDatabase.PExecute(
                    "UPDATE mail SET sender = '%u', receiver = '%u', "
                    "expire_time = '" UI64FMTD "', deliver_time = '" UI64FMTD
                    "',cod = '0', checked = '%u' WHERE id = '%u'",
                    m->receiverGuid.GetCounter(), m->sender,
                    (uint64)(basetime + 30 * DAY), (uint64)basetime,
                    MAIL_CHECK_MASK_RETURNED, m->messageID);
                for (auto itr2 = m->items.begin(); itr2 != m->items.end();
                     ++itr2)
                {
                    // update receiver in mail items for its proper delivery,
                    // and in instance_item for avoid lost item at sender delete
                    CharacterDatabase.PExecute(
                        "UPDATE mail_items SET receiver = %u WHERE item_guid = "
                        "'%u'",
                        m->sender, itr2->item_guid);
                    CharacterDatabase.PExecute(
                        "UPDATE item_instance SET owner_guid = %u WHERE guid = "
                        "'%u'",
                        m->sender, itr2->item_guid);
                }
                delete m;
                continue;
            }
        }

        if (m->itemTextId)
            CharacterDatabase.PExecute(
                "DELETE FROM item_text WHERE id = '%u'", m->itemTextId);

        // deletemail = true;
        // delmails << m->messageID << ", ";
        CharacterDatabase.PExecute(
            "DELETE FROM mail WHERE id = '%u'", m->messageID);
        delete m;
        ++count;
    } while (result->NextRow());

    logging.info("Loaded %u mails\n", count);
}

void ObjectMgr::LoadQuestAreaTriggers()
{
    mQuestAreaTriggerMap.clear(); // need for reload case

    QueryResult* result = WorldDatabase.Query(
        "SELECT id,quest FROM areatrigger_involvedrelation");

    uint32 count = 0;

    if (!result)
    {
        logging.info("Loaded %u quest trigger points\n", count);
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        ++count;
        bar.step();

        Field* fields = result->Fetch();

        uint32 trigger_ID = fields[0].GetUInt32();
        uint32 quest_ID = fields[1].GetUInt32();

        AreaTriggerEntry const* atEntry =
            sAreaTriggerStore.LookupEntry(trigger_ID);
        if (!atEntry)
        {
            logging.error(
                "Table `areatrigger_involvedrelation` has area trigger (ID: "
                "%u) not listed in `AreaTrigger.dbc`.",
                trigger_ID);
            continue;
        }

        Quest const* quest = GetQuestTemplate(quest_ID);
        if (!quest)
        {
            logging.error(
                "Table `areatrigger_involvedrelation` has record (id: %u) for "
                "not existing quest %u",
                trigger_ID, quest_ID);
            continue;
        }

        if (!quest->HasSpecialFlag(QUEST_SPECIAL_FLAG_EXPLORATION_OR_EVENT))
        {
            logging.error(
                "Table `areatrigger_involvedrelation` has record (id: %u) for "
                "not quest %u, but quest not have flag "
                "QUEST_SPECIAL_FLAG_EXPLORATION_OR_EVENT. Trigger or quest "
                "flags must be fixed, quest modified to require objective.",
                trigger_ID, quest_ID);

            // this will prevent quest completing without objective
            const_cast<Quest*>(quest)->SetSpecialFlag(
                QUEST_SPECIAL_FLAG_EXPLORATION_OR_EVENT);

            // continue; - quest modified to required objective and trigger can
            // be allowed.
        }

        mQuestAreaTriggerMap[trigger_ID] = quest_ID;

    } while (result->NextRow());

    delete result;

    logging.info("Loaded %u quest trigger points\n", count);
}

void ObjectMgr::LoadTavernAreaTriggers()
{
    mTavernAreaTriggerSet.clear(); // need for reload case

    QueryResult* result =
        WorldDatabase.Query("SELECT id FROM areatrigger_tavern");

    uint32 count = 0;

    if (!result)
    {
        logging.info("Loaded %u tavern triggers\n", count);
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        ++count;
        bar.step();

        Field* fields = result->Fetch();

        uint32 Trigger_ID = fields[0].GetUInt32();

        AreaTriggerEntry const* atEntry =
            sAreaTriggerStore.LookupEntry(Trigger_ID);
        if (!atEntry)
        {
            logging.error(
                "Table `areatrigger_tavern` has area trigger (ID:%u) not "
                "listed in `AreaTrigger.dbc`.",
                Trigger_ID);
            continue;
        }

        mTavernAreaTriggerSet.insert(Trigger_ID);
    } while (result->NextRow());

    delete result;

    logging.info("Loaded %u tavern triggers\n", count);
}

uint32 ObjectMgr::GetNearestTaxiNode(
    float x, float y, float z, uint32 mapid, Team team) const
{
    bool found = false;
    float dist;
    uint32 id = 0;

    for (uint32 i = 1; i < sTaxiNodesStore.GetNumRows(); ++i)
    {
        TaxiNodesEntry const* node = sTaxiNodesStore.LookupEntry(i);
        if (!node || node->map_id != mapid ||
            !node->MountCreatureID[team == ALLIANCE ? 1 : 0])
            continue;

        uint8 field = (uint8)((i - 1) / 32);
        uint32 submask = 1 << ((i - 1) % 32);

        // skip not taxi network nodes
        if ((sTaxiNodesMask[field] & submask) == 0)
            continue;

        float dist2 = (node->x - x) * (node->x - x) +
                      (node->y - y) * (node->y - y) +
                      (node->z - z) * (node->z - z);
        if (found)
        {
            if (dist2 < dist)
            {
                dist = dist2;
                id = i;
            }
        }
        else
        {
            found = true;
            dist = dist2;
            id = i;
        }
    }

    return id;
}

void ObjectMgr::GetTaxiPath(
    uint32 source, uint32 destination, uint32& path, uint32& cost) const
{
    auto src_i = sTaxiPathSetBySource.find(source);
    if (src_i == sTaxiPathSetBySource.end())
    {
        path = 0;
        cost = 0;
        return;
    }

    TaxiPathSetForSource& pathSet = src_i->second;

    auto dest_i = pathSet.find(destination);
    if (dest_i == pathSet.end())
    {
        path = 0;
        cost = 0;
        return;
    }

    cost = dest_i->second.price;
    path = dest_i->second.ID;
}

uint32 ObjectMgr::GetTaxiMountDisplayId(
    uint32 id, Team team, bool allowed_alt_team /* = false */) const
{
    uint16 mount_entry = 0;

    // select mount creature id
    TaxiNodesEntry const* node = sTaxiNodesStore.LookupEntry(id);
    if (node)
    {
        if (team == ALLIANCE)
        {
            mount_entry = node->MountCreatureID[1];
            if (!mount_entry && allowed_alt_team)
                mount_entry = node->MountCreatureID[0];
        }
        else if (team == HORDE)
        {
            mount_entry = node->MountCreatureID[0];

            if (!mount_entry && allowed_alt_team)
                mount_entry = node->MountCreatureID[1];
        }
    }

    CreatureInfo const* mount_info = GetCreatureTemplate(mount_entry);
    if (!mount_info)
        return 0;

    uint16 mount_id = Creature::ChooseDisplayId(mount_info);
    if (!mount_id)
        return 0;

    CreatureModelInfo const* minfo = GetCreatureModelRandomGender(mount_id);
    if (minfo)
        mount_id = minfo->modelid;

    return mount_id;
}

void ObjectMgr::LoadGraveyardZones()
{
    mGraveYardMap.clear(); // need for reload case

    QueryResult* result = WorldDatabase.Query(
        "SELECT id, ghost_zone, faction, corpse_safeloc FROM "
        "game_graveyard_zone");

    uint32 count = 0;

    if (!result)
    {
        logging.info("Loaded %u graveyard-zone links\n", count);
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        ++count;
        bar.step();

        Field* fields = result->Fetch();

        uint32 safeLocId = fields[0].GetUInt32();
        uint32 zoneId = fields[1].GetUInt32();
        uint32 team = fields[2].GetUInt32();
        bool corpse_safeloc = fields[3].GetUInt8();

        WorldSafeLocsEntry const* entry =
            sWorldSafeLocsStore.LookupEntry(safeLocId);
        if (!entry)
        {
            logging.error(
                "Table `game_graveyard_zone` has record for not existing "
                "graveyard (WorldSafeLocs.dbc id) %u, skipped.",
                safeLocId);
            continue;
        }

        AreaTableEntry const* areaEntry = GetAreaEntryByAreaID(zoneId);
        if (!areaEntry)
        {
            logging.error(
                "Table `game_graveyard_zone` has record for not existing zone "
                "id (%u), skipped.",
                zoneId);
            continue;
        }

        if (team != TEAM_NONE && team != HORDE && team != ALLIANCE)
        {
            logging.error(
                "Table `game_graveyard_zone` has record for non player faction "
                "(%u), skipped.",
                team);
            continue;
        }

        if (!AddGraveYardLink(
                safeLocId, zoneId, Team(team), false, corpse_safeloc))
            logging.error(
                "Table `game_graveyard_zone` has a duplicate record for "
                "Graveyard (ID: %u) and Zone (ID: %u), skipped.",
                safeLocId, zoneId);
    } while (result->NextRow());

    delete result;

    logging.info("Loaded %u graveyard-zone links\n", count);
}

const WorldSafeLocsEntry* ObjectMgr::GetClosestGraveyard(
    float x, float y, float z, uint32 map_id, Team team)
{
    // Algorithm for finding graveyard:
    //
    // Start -> Select all graveyards in area id, goto End if found any
    // else  -> Select all graveyards in zone id, goto End if found any
    // else  -> goto Fail
    //
    // Cond  -> not a corpse_safeloc, faction 0 or matches player's faciton
    // End   -> Select first graveyard in different map if Cond
    // else  -> Select closest graveyard in same map if Cond
    // else  -> goto Fail
    //
    // Fail  -> No graveyard found
    //
    // Note: Corpse safeloc are locations only used to place your corpse if you
    // fell down somewhere. These are not "real" graveyards; ignore them.

    uint32 areaId = sTerrainMgr::Instance()->GetAreaId(map_id, x, y, z);
    uint32 zoneId = sTerrainMgr::Instance()->GetZoneId(map_id, x, y, z);

    auto bounds = mGraveYardMap.equal_range(areaId);
    if (bounds.first == bounds.second)
        bounds = mGraveYardMap.equal_range(zoneId);

    if (bounds.first == bounds.second)
    {
        logging.error(
            "Table `game_graveyard_zone` incomplete: Zone %u Team %u does not "
            "have a linked graveyard.",
            zoneId, uint32(team));
        return nullptr;
    }

    const WorldSafeLocsEntry* found = nullptr;
    float found_dist2 = 0;

    for (auto itr = bounds.first; itr != bounds.second; ++itr)
    {
        const auto& data = itr->second;

        auto* entry = sWorldSafeLocsStore.LookupEntry(data.safeLocId);
        if (!entry)
        {
            logging.error(
                "Table `game_graveyard_zone` has record for not existing "
                "graveyard (WorldSafeLocs.dbc id) %u, skipped.",
                data.safeLocId);
            continue;
        }

        if (data.team != TEAM_NONE && team != TEAM_NONE && data.team != team)
            continue;

        if (data.corpse_safeloc)
            continue;

        if (map_id != entry->map_id)
        {
            found = entry;
            break;
        }

        float dist2 = (entry->x - x) * (entry->x - x) +
                      (entry->y - y) * (entry->y - y) +
                      (entry->z - z) * (entry->z - z);

        if (found == nullptr || dist2 < found_dist2)
        {
            found = entry;
            found_dist2 = dist2;
        }
    }

    return found;
}

const WorldSafeLocsEntry* ObjectMgr::GetCorpseSafeLoc(
    float x, float y, float z, uint32 MapId, Team team)
{
    uint32 zoneId = sTerrainMgr::Instance()->GetZoneId(MapId, x, y, z);
    GraveYardMapBounds bounds = mGraveYardMap.equal_range(zoneId);
    if (bounds.first == bounds.second)
        return nullptr;

    const WorldSafeLocsEntry* entry = nullptr;

    auto itr = bounds.first;
    while (itr != bounds.second && !entry)
    {
        if (itr->second.corpse_safeloc &&
            (!itr->second.team || itr->second.team == team))
            entry = sWorldSafeLocsStore.LookupEntry(itr->second.safeLocId);
        ++itr;
    }

    if (!entry)
        return nullptr;

    for (; itr != bounds.second; ++itr)
    {
        if (!(itr->second.corpse_safeloc &&
                (!itr->second.team || itr->second.team == team)))
            continue;

        const WorldSafeLocsEntry* newer =
            sWorldSafeLocsStore.LookupEntry(itr->second.safeLocId);
        if (!newer)
            continue;

        float dist2_cur = (entry->x - x) * (entry->x - x) +
                          (entry->y - y) * (entry->y - y) +
                          (entry->z - z) * (entry->z - z);
        float dist2_new = (newer->x - x) * (newer->x - x) +
                          (newer->y - y) * (newer->y - y) +
                          (newer->z - z) * (newer->z - z);

        if (dist2_new < dist2_cur)
            entry = newer;
    }

    return entry;
}

GraveYardData const* ObjectMgr::FindGraveYardData(
    uint32 id, uint32 zoneId) const
{
    GraveYardMapBounds bounds = mGraveYardMap.equal_range(zoneId);

    for (auto itr = bounds.first; itr != bounds.second; ++itr)
    {
        if (itr->second.safeLocId == id)
            return &itr->second;
    }

    return nullptr;
}

bool ObjectMgr::AddGraveYardLink(
    uint32 id, uint32 zoneId, Team team, bool inDB, bool corpse_safeloc)
{
    if (FindGraveYardData(id, zoneId))
        return false;

    // add link to loaded data
    GraveYardData data;
    data.safeLocId = id;
    data.team = team;
    data.corpse_safeloc = corpse_safeloc;

    mGraveYardMap.insert(GraveYardMap::value_type(zoneId, data));

    // add link to DB
    if (inDB)
    {
        WorldDatabase.PExecuteLog(
            "INSERT INTO game_graveyard_zone ( id,ghost_zone,faction) "
            "VALUES ('%u', '%u','%u')",
            id, zoneId, uint32(team));
    }

    return true;
}

void ObjectMgr::SetGraveYardLinkTeam(uint32 id, uint32 zoneId, Team team)
{
    std::pair<GraveYardMap::iterator, GraveYardMap::iterator> bounds =
        mGraveYardMap.equal_range(zoneId);

    for (auto itr = bounds.first; itr != bounds.second; ++itr)
    {
        GraveYardData& data = itr->second;

        // skip not matching safezone id
        if (data.safeLocId != id)
            continue;

        data.team = team; // Validate link
        return;
    }

    if (team == TEAM_NONE)
        return;

    // Link expected but not exist.
    logging.error(
        "ObjectMgr::SetGraveYardLinkTeam called for safeLoc %u, zoneId %u, but "
        "no graveyard link for this found in database.",
        id, zoneId);
    AddGraveYardLink(id, zoneId,
        team); // Add to prevent further error message and correct mechanismn
}

void ObjectMgr::LoadAreaTriggerTeleports()
{
    mAreaTriggers.clear(); // need for reload case

    uint32 count = 0;

    //                                                0   1               2
    //                                                3           4
    //                                                5                  6 7
    QueryResult* result = WorldDatabase.Query(
        "SELECT id, required_level, required_quest_done, target_map, "
        "target_position_x, target_position_y, target_position_z, "
        "target_orientation FROM areatrigger_teleport");
    if (!result)
    {
        logging.info("Loaded %u area trigger teleport definitions\n", count);
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();

        bar.step();

        ++count;

        uint32 Trigger_ID = fields[0].GetUInt32();

        AreaTrigger at;

        at.requiredLevel = fields[1].GetUInt8();
        at.requiredQuest = fields[2].GetUInt32();
        at.target_mapId = fields[3].GetUInt32();
        at.target_X = fields[4].GetFloat();
        at.target_Y = fields[5].GetFloat();
        at.target_Z = fields[6].GetFloat();
        at.target_Orientation = fields[7].GetFloat();

        AreaTriggerEntry const* atEntry =
            sAreaTriggerStore.LookupEntry(Trigger_ID);
        if (!atEntry)
        {
            logging.error(
                "Table `areatrigger_teleport` has area trigger (ID:%u) not "
                "listed in `AreaTrigger.dbc`.",
                Trigger_ID);
            continue;
        }

        if (at.requiredQuest)
        {
            auto qReqItr = mQuestTemplates.find(at.requiredQuest);
            if (qReqItr == mQuestTemplates.end())
            {
                logging.error(
                    "Table `areatrigger_teleport` has nonexistent required "
                    "quest %u for trigger %u, remove quest done requirement.",
                    at.requiredQuest, Trigger_ID);
                at.requiredQuest = 0;
            }
        }

        MapEntry const* mapEntry = sMapStore.LookupEntry(at.target_mapId);
        if (!mapEntry)
        {
            logging.error(
                "Table `areatrigger_teleport` has nonexistent target map (ID: "
                "%u) for Area trigger (ID:%u).",
                at.target_mapId, Trigger_ID);
            continue;
        }

        if (at.target_X == 0 && at.target_Y == 0 && at.target_Z == 0)
        {
            logging.error(
                "Table `areatrigger_teleport` has area trigger (ID:%u) without "
                "target coordinates.",
                Trigger_ID);
            continue;
        }

        mAreaTriggers[Trigger_ID] = at;

    } while (result->NextRow());

    delete result;

    logging.info("Loaded %u area trigger teleport definitions\n", count);
}

/*
 * Searches for the areatrigger which teleports players out of the given map
 * (only direct to continent)
 */
AreaTrigger const* ObjectMgr::GetGoBackTrigger(uint32 map_id) const
{
    const MapEntry* mapEntry = sMapStore.LookupEntry(map_id);
    if (!mapEntry || mapEntry->ghost_entrance_map < 0)
        return nullptr;

    for (const auto& elem : mAreaTriggers)
    {
        if (elem.second.target_mapId == uint32(mapEntry->ghost_entrance_map))
        {
            AreaTriggerEntry const* atEntry =
                sAreaTriggerStore.LookupEntry(elem.first);
            if (atEntry && atEntry->mapid == map_id)
                return &elem.second;
        }
    }
    return nullptr;
}

/**
 * Searches for the areatrigger which teleports players to the given map
 */
AreaTrigger const* ObjectMgr::GetMapEntranceTrigger(uint32 Map) const
{
    for (const auto& elem : mAreaTriggers)
    {
        if (elem.second.target_mapId == Map)
        {
            AreaTriggerEntry const* atEntry =
                sAreaTriggerStore.LookupEntry(elem.first);
            if (atEntry)
                return &elem.second;
        }
    }
    return nullptr;
}

void ObjectMgr::PackGroupIds()
{
    // this routine renumbers groups in such a way so they start from 1 and go
    // up

    // obtain set of all groups
    std::set<uint32> groupIds;

    // all valid ids are in the instance table
    // any associations to ids not in this table are assumed to be
    // cleaned already in CleanupInstances
    QueryResult* result = CharacterDatabase.Query("SELECT groupId FROM groups");
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();

            uint32 id = fields[0].GetUInt32();

            if (id == 0)
            {
                CharacterDatabase.BeginTransaction();
                CharacterDatabase.PExecute(
                    "DELETE FROM groups WHERE groupId = '%u'", id);
                CharacterDatabase.PExecute(
                    "DELETE FROM group_member WHERE groupId = '%u'", id);
                CharacterDatabase.CommitTransaction();
                continue;
            }

            groupIds.insert(id);
        } while (result->NextRow());
        delete result;
    }

    BarGoLink bar(groupIds.size() + 1);
    bar.step();

    uint32 groupId = 1;
    // we do assume std::set is sorted properly on integer value
    for (const auto& groupIds_i : groupIds)
    {
        if (groupIds_i != groupId)
        {
            // remap group id
            CharacterDatabase.BeginTransaction();
            CharacterDatabase.PExecute(
                "UPDATE groups SET groupId = '%u' WHERE groupId = '%u'",
                groupId, groupIds_i);
            CharacterDatabase.PExecute(
                "UPDATE group_member SET groupId = '%u' WHERE groupId = '%u'",
                groupId, groupIds_i);
            CharacterDatabase.CommitTransaction();
        }

        ++groupId;
        bar.step();
    }

    logging.info("Group Ids remapped, next group id is %u\n", groupId);
}

void ObjectMgr::SetHighestGuids()
{
    QueryResult* result =
        CharacterDatabase.Query("SELECT MAX(guid) FROM characters");
    if (result)
    {
        m_CharGuids.Set((*result)[0].GetUInt32() + 1);
        delete result;
    }

    result = WorldDatabase.Query("SELECT MAX(guid) FROM creature");
    if (result)
    {
        m_FirstTemporaryCreatureGuid = (*result)[0].GetUInt32() + 1;
        delete result;
    }

    result = CharacterDatabase.Query("SELECT MAX(guid) FROM item_instance");
    if (result)
    {
        m_ItemGuids.Set((*result)[0].GetUInt32() + 1);
        delete result;
    }

    // Cleanup other tables from nonexistent guids (>=m_hiItemGuid)
    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute(
        "DELETE FROM character_inventory WHERE item >= '%u'",
        m_ItemGuids.GetNextAfterMaxUsed());
    CharacterDatabase.PExecute("DELETE FROM mail_items WHERE item_guid >= '%u'",
        m_ItemGuids.GetNextAfterMaxUsed());
    CharacterDatabase.PExecute("DELETE FROM auction WHERE itemguid >= '%u'",
        m_ItemGuids.GetNextAfterMaxUsed());
    CharacterDatabase.PExecute(
        "DELETE FROM guild_bank_item WHERE item_guid >= '%u'",
        m_ItemGuids.GetNextAfterMaxUsed());
    CharacterDatabase.CommitTransaction();

    result = WorldDatabase.Query("SELECT MAX(guid) FROM gameobject");
    if (result)
    {
        m_FirstTemporaryGameObjectGuid = (*result)[0].GetUInt32() + 1;
        delete result;
    }

    result = CharacterDatabase.Query("SELECT MAX(id) FROM auction");
    if (result)
    {
        m_AuctionIds.Set((*result)[0].GetUInt32() + 1);
        delete result;
    }

    result = CharacterDatabase.Query("SELECT MAX(id) FROM mail");
    if (result)
    {
        m_MailIds.Set((*result)[0].GetUInt32() + 1);
        delete result;
    }

    result = CharacterDatabase.Query("SELECT MAX(id) FROM item_text");
    if (result)
    {
        m_ItemTextIds.Set((*result)[0].GetUInt32() + 1);
        delete result;
    }

    result = CharacterDatabase.Query("SELECT MAX(guid) FROM corpse");
    if (result)
    {
        m_CorpseGuids.Set((*result)[0].GetUInt32() + 1);
        delete result;
    }

    result = CharacterDatabase.Query("SELECT MAX(arenateamid) FROM arena_team");
    if (result)
    {
        m_ArenaTeamIds.Set((*result)[0].GetUInt32() + 1);
        delete result;
    }

    result = CharacterDatabase.Query("SELECT MAX(guildid) FROM guild");
    if (result)
    {
        m_GuildIds.Set((*result)[0].GetUInt32() + 1);
        delete result;
    }

    result = CharacterDatabase.Query("SELECT MAX(groupId) FROM groups");
    if (result)
    {
        m_GroupGuids.Set((*result)[0].GetUInt32() + 1);
        delete result;
    }

    // setup reserved ranges for static guids spawn
    m_StaticCreatureGuids.Set(m_FirstTemporaryCreatureGuid);
    m_FirstTemporaryCreatureGuid +=
        sWorld::Instance()->getConfig(CONFIG_UINT32_GUID_RESERVE_SIZE_CREATURE);

    m_StaticGameObjectGuids.Set(m_FirstTemporaryGameObjectGuid);
    m_FirstTemporaryGameObjectGuid += sWorld::Instance()->getConfig(
        CONFIG_UINT32_GUID_RESERVE_SIZE_GAMEOBJECT);
}

uint32 ObjectMgr::CreateItemText(std::string text)
{
    uint32 newItemTextId = GenerateItemTextID();
    // insert new itempage to container
    mItemTexts[newItemTextId] = text;
    // save new itempage
    CharacterDatabase.escape_string(text);
    // any Delete query needed, itemTextId is maximum of all ids
    std::ostringstream query;
    query << "INSERT INTO item_text (id,text) VALUES ( '" << newItemTextId
          << "', '" << text << "')";
    CharacterDatabase.Execute(query.str().c_str()); // needs to be run this way,
                                                    // because mail body may be
                                                    // more than 1024 characters
    return newItemTextId;
}

void ObjectMgr::LoadGameObjectLocales()
{
    mGameObjectLocaleMap.clear(); // need for reload case

    QueryResult* result = WorldDatabase.Query(
        "SELECT entry,"
        "name_loc1,name_loc2,name_loc3,name_loc4,name_loc5,name_loc6,name_loc7,"
        "name_loc8,"
        "castbarcaption_loc1,castbarcaption_loc2,castbarcaption_loc3,"
        "castbarcaption_loc4,"
        "castbarcaption_loc5,castbarcaption_loc6,castbarcaption_loc7,"
        "castbarcaption_loc8 FROM locales_gameobject");

    if (!result)
    {
        logging.info(
            "Loaded 0 gameobject locale strings. DB table "
            "`locales_gameobject` is empty.");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 entry = fields[0].GetUInt32();

        if (!GetGameObjectInfo(entry))
        {
            logging.warning(
                "Table `locales_gameobject` has data for nonexistent "
                "gameobject entry %u, skipped.",
                entry);
            continue;
        }

        GameObjectLocale& data = mGameObjectLocaleMap[entry];

        for (int i = 1; i < MAX_LOCALE; ++i)
        {
            std::string str = fields[i].GetCppString();
            if (!str.empty())
            {
                int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    if ((int32)data.Name.size() <= idx)
                        data.Name.resize(idx + 1);

                    data.Name[idx] = str;
                }
            }
        }

        for (int i = 1; i < MAX_LOCALE; ++i)
        {
            std::string str = fields[i + (MAX_LOCALE - 1)].GetCppString();
            if (!str.empty())
            {
                int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    if ((int32)data.CastBarCaption.size() <= idx)
                        data.CastBarCaption.resize(idx + 1);

                    data.CastBarCaption[idx] = str;
                }
            }
        }

    } while (result->NextRow());

    delete result;

    logging.info("Loaded %lu gameobject locale strings",
        (unsigned long)mGameObjectLocaleMap.size());
}

struct SQLGameObjectLoader : public SQLStorageLoaderBase<SQLGameObjectLoader>
{
    template <class D>
    void convert_from_str(uint32 /*field_pos*/, char const* src, D& dst)
    {
        dst = D(sScriptMgr::Instance()->GetScriptId(src));
    }
};

inline void CheckGOLockId(GameObjectInfo const* goInfo, uint32 dataN, uint32 N)
{
    if (sLockStore.LookupEntry(dataN))
        return;

    logging.error(
        "Gameobject (Entry: %u GoType: %u) have data%d=%u but lock (Id: %u) "
        "not found.",
        goInfo->id, goInfo->type, N, dataN, dataN);
}

inline void CheckGOLinkedTrapId(
    GameObjectInfo const* goInfo, uint32 dataN, uint32 N)
{
    if (GameObjectInfo const* trapInfo =
            sGOStorage.LookupEntry<GameObjectInfo>(dataN))
    {
        if (trapInfo->type != GAMEOBJECT_TYPE_TRAP)
            logging.error(
                "Gameobject (Entry: %u GoType: %u) have data%d=%u but GO "
                "(Entry %u) have not GAMEOBJECT_TYPE_TRAP (%u) type.",
                goInfo->id, goInfo->type, N, dataN, dataN,
                GAMEOBJECT_TYPE_TRAP);
    }
    else
        // too many error reports about nonexistent trap templates
        logging.warning(
            "Gameobject (Entry: %u GoType: %u) have data%d=%u but trap GO "
            "(Entry %u) not exist in `gameobject_template`.",
            goInfo->id, goInfo->type, N, dataN, dataN);
}

inline void CheckGOSpellId(GameObjectInfo const* goInfo, uint32 dataN, uint32 N)
{
    if (sSpellStore.LookupEntry(dataN))
        return;

    logging.error(
        "Gameobject (Entry: %u GoType: %u) have data%d=%u but Spell (Entry %u) "
        "not exist.",
        goInfo->id, goInfo->type, N, dataN, dataN);
}

inline void CheckAndFixGOChairHeightId(
    GameObjectInfo const* goInfo, uint32 const& dataN, uint32 N)
{
    if (dataN <=
        (UNIT_STAND_STATE_SIT_HIGH_CHAIR - UNIT_STAND_STATE_SIT_LOW_CHAIR))
        return;

    logging.error(
        "Gameobject (Entry: %u GoType: %u) have data%d=%u but correct chair "
        "height in range 0..%i.",
        goInfo->id, goInfo->type, N, dataN,
        UNIT_STAND_STATE_SIT_HIGH_CHAIR - UNIT_STAND_STATE_SIT_LOW_CHAIR);

    // prevent client and server unexpected work
    const_cast<uint32&>(dataN) = 0;
}

inline void CheckGONoDamageImmuneId(
    GameObjectInfo const* goInfo, uint32 dataN, uint32 N)
{
    // 0/1 correct values
    if (dataN <= 1)
        return;

    logging.error(
        "Gameobject (Entry: %u GoType: %u) have data%d=%u but expected boolean "
        "(0/1) noDamageImmune field value.",
        goInfo->id, goInfo->type, N, dataN);
}

inline void CheckGOConsumable(
    GameObjectInfo const* goInfo, uint32 dataN, uint32 N)
{
    // 0/1 correct values
    if (dataN <= 1)
        return;

    logging.error(
        "Gameobject (Entry: %u GoType: %u) have data%d=%u but expected boolean "
        "(0/1) consumable field value.",
        goInfo->id, goInfo->type, N, dataN);
}

void ObjectMgr::LoadGameobjectInfo()
{
    SQLGameObjectLoader loader;
    loader.Load(sGOStorage);

    // some checks
    for (uint32 id = 1; id < sGOStorage.MaxEntry; id++)
    {
        GameObjectInfo const* goInfo =
            sGOStorage.LookupEntry<GameObjectInfo>(id);
        if (!goInfo)
            continue;

        if (goInfo->size <= 0.0f) // prevent use too small scales
        {
            logging.warning(
                "Gameobject (Entry: %u GoType: %u) have too small size=%f",
                goInfo->id, goInfo->type, goInfo->size);
            const_cast<GameObjectInfo*>(goInfo)->size = DEFAULT_OBJECT_SCALE;
        }

        // some GO types have unused go template, check goInfo->displayId at GO
        // spawn data loading or ignore

        switch (goInfo->type)
        {
        case GAMEOBJECT_TYPE_DOOR: // 0
        {
            if (goInfo->door.lockId)
                CheckGOLockId(goInfo, goInfo->door.lockId, 1);
            CheckGONoDamageImmuneId(goInfo, goInfo->door.noDamageImmune, 3);
            break;
        }
        case GAMEOBJECT_TYPE_BUTTON: // 1
        {
            if (goInfo->button.lockId)
                CheckGOLockId(goInfo, goInfo->button.lockId, 1);
            if (goInfo->button.linkedTrapId) // linked trap
                CheckGOLinkedTrapId(goInfo, goInfo->button.linkedTrapId, 3);
            CheckGONoDamageImmuneId(goInfo, goInfo->button.noDamageImmune, 4);
            break;
        }
        case GAMEOBJECT_TYPE_QUESTGIVER: // 2
        {
            if (goInfo->questgiver.lockId)
                CheckGOLockId(goInfo, goInfo->questgiver.lockId, 0);
            CheckGONoDamageImmuneId(
                goInfo, goInfo->questgiver.noDamageImmune, 5);
            break;
        }
        case GAMEOBJECT_TYPE_CHEST: // 3
        {
            if (goInfo->chest.lockId)
                CheckGOLockId(goInfo, goInfo->chest.lockId, 0);

            CheckGOConsumable(goInfo, goInfo->chest.consumable, 3);

            if (goInfo->chest.linkedTrapId) // linked trap
                CheckGOLinkedTrapId(goInfo, goInfo->chest.linkedTrapId, 7);
            break;
        }
        case GAMEOBJECT_TYPE_TRAP: // 6
        {
            if (goInfo->trap.lockId)
                CheckGOLockId(goInfo, goInfo->trap.lockId, 0);
            /* disable check for while, too many nonexistent spells
            if (goInfo->trap.spellId)                   // spell
                CheckGOSpellId(goInfo,goInfo->trap.spellId,3);
            */
            break;
        }
        case GAMEOBJECT_TYPE_CHAIR: // 7
            CheckAndFixGOChairHeightId(goInfo, goInfo->chair.height, 1);
            break;
        case GAMEOBJECT_TYPE_SPELL_FOCUS: // 8
        {
            if (goInfo->spellFocus.focusId)
            {
                if (!sSpellFocusObjectStore.LookupEntry(
                        goInfo->spellFocus.focusId))
                    logging.error(
                        "Gameobject (Entry: %u GoType: %u) have data0=%u but "
                        "SpellFocus (Id: %u) not exist.",
                        id, goInfo->type, goInfo->spellFocus.focusId,
                        goInfo->spellFocus.focusId);
            }

            if (goInfo->spellFocus.linkedTrapId) // linked trap
                CheckGOLinkedTrapId(goInfo, goInfo->spellFocus.linkedTrapId, 2);
            break;
        }
        case GAMEOBJECT_TYPE_GOOBER: // 10
        {
            if (goInfo->goober.lockId)
                CheckGOLockId(goInfo, goInfo->goober.lockId, 0);

            CheckGOConsumable(goInfo, goInfo->goober.consumable, 3);

            if (goInfo->goober.pageId) // pageId
            {
                if (!sPageTextStore.LookupEntry<PageText>(
                        goInfo->goober.pageId))
                    logging.error(
                        "Gameobject (Entry: %u GoType: %u) have data7=%u but "
                        "PageText (Entry %u) not exist.",
                        id, goInfo->type, goInfo->goober.pageId,
                        goInfo->goober.pageId);
            }
            /* disable check for while, too many nonexistent spells
            if (goInfo->goober.spellId)                 // spell
                CheckGOSpellId(goInfo,goInfo->goober.spellId,10);
            */
            CheckGONoDamageImmuneId(goInfo, goInfo->goober.noDamageImmune, 11);
            if (goInfo->goober.linkedTrapId) // linked trap
                CheckGOLinkedTrapId(goInfo, goInfo->goober.linkedTrapId, 12);
            break;
        }
        case GAMEOBJECT_TYPE_AREADAMAGE: // 12
        {
            if (goInfo->areadamage.lockId)
                CheckGOLockId(goInfo, goInfo->areadamage.lockId, 0);
            break;
        }
        case GAMEOBJECT_TYPE_CAMERA: // 13
        {
            if (goInfo->camera.lockId)
                CheckGOLockId(goInfo, goInfo->camera.lockId, 0);
            break;
        }
        case GAMEOBJECT_TYPE_MO_TRANSPORT: // 15
        {
            if (goInfo->moTransport.taxiPathId)
            {
                if (goInfo->moTransport.taxiPathId >=
                        sTaxiPathNodesByPath.size() ||
                    sTaxiPathNodesByPath[goInfo->moTransport.taxiPathId]
                        .empty())
                    logging.error(
                        "Gameobject (Entry: %u GoType: %u) have data0=%u but "
                        "TaxiPath (Id: %u) not exist.",
                        id, goInfo->type, goInfo->moTransport.taxiPathId,
                        goInfo->moTransport.taxiPathId);
            }
            break;
        }
        case GAMEOBJECT_TYPE_SUMMONING_RITUAL: // 18
        {
            /* disable check for while, too many nonexistent spells
            // always must have spell
            CheckGOSpellId(goInfo,goInfo->summoningRitual.spellId,1);
            */
            break;
        }
        case GAMEOBJECT_TYPE_SPELLCASTER: // 22
        {
            // always must have spell
            CheckGOSpellId(goInfo, goInfo->spellcaster.spellId, 0);
            break;
        }
        case GAMEOBJECT_TYPE_FLAGSTAND: // 24
        {
            if (goInfo->flagstand.lockId)
                CheckGOLockId(goInfo, goInfo->flagstand.lockId, 0);
            CheckGONoDamageImmuneId(
                goInfo, goInfo->flagstand.noDamageImmune, 5);
            break;
        }
        case GAMEOBJECT_TYPE_FISHINGHOLE: // 25
        {
            if (goInfo->fishinghole.lockId)
                CheckGOLockId(goInfo, goInfo->fishinghole.lockId, 4);
            break;
        }
        case GAMEOBJECT_TYPE_FLAGDROP: // 26
        {
            if (goInfo->flagdrop.lockId)
                CheckGOLockId(goInfo, goInfo->flagdrop.lockId, 0);
            CheckGONoDamageImmuneId(goInfo, goInfo->flagdrop.noDamageImmune, 3);
            break;
        }
        }
    }

    logging.info("Loaded %u game object templates\n", sGOStorage.RecordCount);
}

void ObjectMgr::LoadExplorationBaseXP()
{
    uint32 count = 0;
    QueryResult* result =
        WorldDatabase.Query("SELECT level,basexp FROM exploration_basexp");

    if (!result)
    {
        logging.info("Loaded %u BaseXP definitions\n", count);
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        bar.step();

        Field* fields = result->Fetch();
        uint32 level = fields[0].GetUInt32();
        uint32 basexp = fields[1].GetUInt32();
        mBaseXPTable[level] = basexp;
        ++count;
    } while (result->NextRow());

    delete result;

    logging.info("Loaded %u BaseXP definitions\n", count);
}

uint32 ObjectMgr::GetBaseXP(uint32 level) const
{
    auto itr = mBaseXPTable.find(level);
    return itr != mBaseXPTable.end() ? itr->second : 0;
}

uint32 ObjectMgr::GetXPForLevel(uint32 level) const
{
    if (level < mPlayerXPperLevel.size())
        return mPlayerXPperLevel[level];
    return 0;
}

void ObjectMgr::LoadPetNames()
{
    uint32 count = 0;
    QueryResult* result =
        WorldDatabase.Query("SELECT word,entry,half FROM pet_name_generation");

    if (!result)
    {
        logging.info("Loaded %u pet name parts\n", count);
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        bar.step();

        Field* fields = result->Fetch();
        std::string word = fields[0].GetString();
        uint32 entry = fields[1].GetUInt32();
        bool half = fields[2].GetBool();
        if (half)
            PetHalfName1[entry].push_back(word);
        else
            PetHalfName0[entry].push_back(word);
        ++count;
    } while (result->NextRow());
    delete result;

    logging.info("Loaded %u pet name parts\n", count);
}

void ObjectMgr::LoadPetNumber()
{
    QueryResult* result =
        CharacterDatabase.Query("SELECT MAX(id) FROM character_pet");
    if (result)
    {
        Field* fields = result->Fetch();
        m_PetNumbers.Set(fields[0].GetUInt32() + 1);
        delete result;
    }

    logging.info("Loaded the max pet number: %d\n",
        m_PetNumbers.GetNextAfterMaxUsed() - 1);
}

std::string ObjectMgr::GeneratePetName(uint32 entry)
{
    std::vector<std::string>& list0 = PetHalfName0[entry];
    std::vector<std::string>& list1 = PetHalfName1[entry];

    if (list0.empty() || list1.empty())
    {
        CreatureInfo const* cinfo = GetCreatureTemplate(entry);
        char const* petname = GetPetName(
            cinfo->family, sWorld::Instance()->GetDefaultDbcLocale());
        if (!petname)
            petname = cinfo->Name;
        return std::string(petname);
    }

    return *(list0.begin() + urand(0, list0.size() - 1)) +
           *(list1.begin() + urand(0, list1.size() - 1));
}

void ObjectMgr::LoadCorpses()
{
    uint32 count = 0;
    //                                                    0            1       2
    //                                                    3                  4
    //                                                    5                   6
    QueryResult* result = CharacterDatabase.Query(
        "SELECT corpse.guid, player, corpse.position_x, corpse.position_y, "
        "corpse.position_z, corpse.orientation, corpse.map, "
        //   7     8            9         10      11    12     13           14
        //   15              16       17
        "time, corpse_type, instance, gender, race, class, playerBytes, "
        "playerBytes2, equipmentCache, guildId, playerFlags FROM corpse "
        "JOIN characters ON player = characters.guid "
        "LEFT JOIN guild_member ON player=guild_member.guid WHERE corpse_type "
        "<> 0");

    if (!result)
    {
        logging.info("Loaded %u corpses\n", count);
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        bar.step();

        Field* fields = result->Fetch();

        uint32 guid = fields[0].GetUInt32();

        auto corpse = new Corpse;
        if (!corpse->LoadFromDB(guid, fields))
        {
            delete corpse;
            continue;
        }

        sObjectAccessor::Instance()->AddCorpse(corpse);

        ++count;
    } while (result->NextRow());
    delete result;

    logging.info("Loaded %u corpses\n", count);
}

void ObjectMgr::LoadReputationRewardRate()
{
    m_RepRewardRateMap.clear(); // for reload case

    uint32 count = 0;
    QueryResult* result = WorldDatabase.Query(
        "SELECT faction, quest_rate, creature_rate, spell_rate FROM "
        "reputation_reward_rate");

    if (!result)
    {
        logging.error("Loaded `reputation_reward_rate`, table is empty!\n");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        bar.step();

        Field* fields = result->Fetch();

        uint32 factionId = fields[0].GetUInt32();

        RepRewardRate repRate;

        repRate.quest_rate = fields[1].GetFloat();
        repRate.creature_rate = fields[2].GetFloat();
        repRate.spell_rate = fields[3].GetFloat();

        FactionEntry const* factionEntry = sFactionStore.LookupEntry(factionId);
        if (!factionEntry)
        {
            logging.error(
                "Faction (faction.dbc) %u does not exist but is used in "
                "`reputation_reward_rate`",
                factionId);
            continue;
        }

        if (repRate.quest_rate < 0.0f)
        {
            logging.error(
                "Table reputation_reward_rate has quest_rate with invalid rate "
                "%f, skipping data for faction %u",
                repRate.quest_rate, factionId);
            continue;
        }

        if (repRate.creature_rate < 0.0f)
        {
            logging.error(
                "Table reputation_reward_rate has creature_rate with invalid "
                "rate %f, skipping data for faction %u",
                repRate.creature_rate, factionId);
            continue;
        }

        if (repRate.spell_rate < 0.0f)
        {
            logging.error(
                "Table reputation_reward_rate has spell_rate with invalid rate "
                "%f, skipping data for faction %u",
                repRate.spell_rate, factionId);
            continue;
        }

        m_RepRewardRateMap[factionId] = repRate;

        ++count;
    } while (result->NextRow());

    delete result;

    logging.info("Loaded %u reputation_reward_rate\n", count);
}

void ObjectMgr::LoadReputationOnKill()
{
    uint32 count = 0;

    //                                                0            1 2
    QueryResult* result = WorldDatabase.Query(
        "SELECT creature_id, RewOnKillRepFaction1, RewOnKillRepFaction2,"
        //   3             4             5                   6             7
        //   8                   9
        "IsTeamAward1, MaxStanding1, RewOnKillRepValue1, IsTeamAward2, "
        "MaxStanding2, RewOnKillRepValue2, TeamDependent "
        "FROM creature_onkill_reputation");

    if (!result)
    {
        logging.error(
            "Loaded 0 creature award reputation definitions. DB table "
            "`creature_onkill_reputation` is empty.\n");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 creature_id = fields[0].GetUInt32();

        ReputationOnKillEntry repOnKill;
        repOnKill.repfaction1 = fields[1].GetUInt32();
        repOnKill.repfaction2 = fields[2].GetUInt32();
        repOnKill.is_teamaward1 = fields[3].GetBool();
        repOnKill.reputation_max_cap1 = fields[4].GetUInt32();
        repOnKill.repvalue1 = fields[5].GetInt32();
        repOnKill.is_teamaward2 = fields[6].GetBool();
        repOnKill.reputation_max_cap2 = fields[7].GetUInt32();
        repOnKill.repvalue2 = fields[8].GetInt32();
        repOnKill.team_dependent = fields[9].GetUInt8();

        if (!GetCreatureTemplate(creature_id))
        {
            logging.error(
                "Table `creature_onkill_reputation` have data for nonexistent "
                "creature entry (%u), skipped",
                creature_id);
            continue;
        }

        if (repOnKill.repfaction1)
        {
            FactionEntry const* factionEntry1 =
                sFactionStore.LookupEntry(repOnKill.repfaction1);
            if (!factionEntry1)
            {
                logging.error(
                    "Faction (faction.dbc) %u does not exist but is used in "
                    "`creature_onkill_reputation`",
                    repOnKill.repfaction1);
                continue;
            }
        }

        if (repOnKill.repfaction2)
        {
            FactionEntry const* factionEntry2 =
                sFactionStore.LookupEntry(repOnKill.repfaction2);
            if (!factionEntry2)
            {
                logging.error(
                    "Faction (faction.dbc) %u does not exist but is used in "
                    "`creature_onkill_reputation`",
                    repOnKill.repfaction2);
                continue;
            }
        }

        mRepOnKill[creature_id] = repOnKill;

        ++count;
    } while (result->NextRow());

    delete result;

    logging.info("Loaded %u creature award reputation definitions\n", count);
}

void ObjectMgr::LoadReputationSpilloverTemplate()
{
    m_RepSpilloverTemplateMap.clear(); // for reload case

    uint32 count = 0;
    QueryResult* result = WorldDatabase.Query(
        "SELECT faction, faction1, rate_1, rank_1, faction2, rate_2, rank_2, "
        "faction3, rate_3, rank_3, faction4, rate_4, rank_4 FROM "
        "reputation_spillover_template");

    if (!result)
    {
        logging.info(
            "Loaded `reputation_spillover_template`, table is empty.\n");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        bar.step();

        Field* fields = result->Fetch();

        uint32 factionId = fields[0].GetUInt32();

        RepSpilloverTemplate repTemplate;

        repTemplate.faction[0] = fields[1].GetUInt32();
        repTemplate.faction_rate[0] = fields[2].GetFloat();
        repTemplate.faction_rank[0] = fields[3].GetUInt32();
        repTemplate.faction[1] = fields[4].GetUInt32();
        repTemplate.faction_rate[1] = fields[5].GetFloat();
        repTemplate.faction_rank[1] = fields[6].GetUInt32();
        repTemplate.faction[2] = fields[7].GetUInt32();
        repTemplate.faction_rate[2] = fields[8].GetFloat();
        repTemplate.faction_rank[2] = fields[9].GetUInt32();
        repTemplate.faction[3] = fields[10].GetUInt32();
        repTemplate.faction_rate[3] = fields[11].GetFloat();
        repTemplate.faction_rank[3] = fields[12].GetUInt32();

        FactionEntry const* factionEntry = sFactionStore.LookupEntry(factionId);

        if (!factionEntry)
        {
            logging.error(
                "Faction (faction.dbc) %u does not exist but is used in "
                "`reputation_spillover_template`",
                factionId);
            continue;
        }

        if (factionEntry->team == 0)
        {
            logging.error(
                "Faction (faction.dbc) %u in `reputation_spillover_template` "
                "does not belong to any team, skipping",
                factionId);
            continue;
        }

        for (uint32 i = 0; i < MAX_SPILLOVER_FACTIONS; ++i)
        {
            if (repTemplate.faction[i])
            {
                FactionEntry const* factionSpillover =
                    sFactionStore.LookupEntry(repTemplate.faction[i]);

                if (!factionSpillover)
                {
                    logging.error(
                        "Spillover faction (faction.dbc) %u does not exist but "
                        "is used in `reputation_spillover_template` for "
                        "faction %u, skipping",
                        repTemplate.faction[i], factionId);
                    continue;
                }

                if (factionSpillover->reputationListID < 0)
                {
                    logging.error(
                        "Spillover faction (faction.dbc) %u for faction %u in "
                        "`reputation_spillover_template` can not be listed for "
                        "client, and then useless, skipping",
                        repTemplate.faction[i], factionId);
                    continue;
                }

                if (repTemplate.faction_rank[i] >= MAX_REPUTATION_RANK)
                {
                    logging.error(
                        "Rank %u used in `reputation_spillover_template` for "
                        "spillover faction %u is not valid, skipping",
                        repTemplate.faction_rank[i], repTemplate.faction[i]);
                    continue;
                }
            }
        }

        FactionEntry const* factionEntry0 =
            sFactionStore.LookupEntry(repTemplate.faction[0]);
        if (repTemplate.faction[0] && !factionEntry0)
        {
            logging.error(
                "Faction (faction.dbc) %u does not exist but is used in "
                "`reputation_spillover_template`",
                repTemplate.faction[0]);
            continue;
        }
        FactionEntry const* factionEntry1 =
            sFactionStore.LookupEntry(repTemplate.faction[1]);
        if (repTemplate.faction[1] && !factionEntry1)
        {
            logging.error(
                "Faction (faction.dbc) %u does not exist but is used in "
                "`reputation_spillover_template`",
                repTemplate.faction[1]);
            continue;
        }
        FactionEntry const* factionEntry2 =
            sFactionStore.LookupEntry(repTemplate.faction[2]);
        if (repTemplate.faction[2] && !factionEntry2)
        {
            logging.error(
                "Faction (faction.dbc) %u does not exist but is used in "
                "`reputation_spillover_template`",
                repTemplate.faction[2]);
            continue;
        }
        FactionEntry const* factionEntry3 =
            sFactionStore.LookupEntry(repTemplate.faction[3]);
        if (repTemplate.faction[3] && !factionEntry3)
        {
            logging.error(
                "Faction (faction.dbc) %u does not exist but is used in "
                "`reputation_spillover_template`",
                repTemplate.faction[3]);
            continue;
        }

        m_RepSpilloverTemplateMap[factionId] = repTemplate;

        ++count;
    } while (result->NextRow());

    delete result;

    logging.info("Loaded %u reputation_spillover_template\n", count);
}

void ObjectMgr::LoadPointsOfInterest()
{
    mPointsOfInterest.clear(); // need for reload case

    uint32 count = 0;

    //                                                0      1  2  3      4 5
    QueryResult* result = WorldDatabase.Query(
        "SELECT entry, x, y, icon, flags, data, icon_name FROM "
        "points_of_interest");

    if (!result)
    {
        logging.error(
            "Loaded 0 Points of Interest definitions. DB table "
            "`points_of_interest` is empty.\n");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 point_id = fields[0].GetUInt32();

        PointOfInterest POI;
        POI.x = fields[1].GetFloat();
        POI.y = fields[2].GetFloat();
        POI.icon = fields[3].GetUInt32();
        POI.flags = fields[4].GetUInt32();
        POI.data = fields[5].GetUInt32();
        POI.icon_name = fields[6].GetCppString();

        if (!maps::verify_coords(POI.x, POI.y))
        {
            logging.error(
                "Table `points_of_interest` (Entry: %u) have invalid "
                "coordinates (X: %f Y: %f), ignored.",
                point_id, POI.x, POI.y);
            continue;
        }

        mPointsOfInterest[point_id] = POI;

        ++count;
    } while (result->NextRow());

    delete result;

    logging.info("Loaded %u Points of Interest definitions\n", count);
}

void ObjectMgr::LoadWeatherZoneChances()
{
    uint32 count = 0;

    //                                                0     1
    //                                                2                   3
    //                                                4                   5
    //                                                6                    7
    //                                                8                 9
    //                                                10                  11 12
    QueryResult* result = WorldDatabase.Query(
        "SELECT zone, spring_rain_chance, spring_snow_chance, "
        "spring_storm_chance, summer_rain_chance, summer_snow_chance, "
        "summer_storm_chance, fall_rain_chance, fall_snow_chance, "
        "fall_storm_chance, winter_rain_chance, winter_snow_chance, "
        "winter_storm_chance FROM game_weather");

    if (!result)
    {
        logging.error(
            "Loaded 0 weather definitions. DB table `game_weather` is "
            "empty.\n");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 zone_id = fields[0].GetUInt32();

        WeatherZoneChances& wzc = mWeatherZoneMap[zone_id];

        for (int season = 0; season < WEATHER_SEASONS; ++season)
        {
            wzc.data[season].rainChance =
                fields[season * (MAX_WEATHER_TYPE - 1) + 1].GetUInt32();
            wzc.data[season].snowChance =
                fields[season * (MAX_WEATHER_TYPE - 1) + 2].GetUInt32();
            wzc.data[season].stormChance =
                fields[season * (MAX_WEATHER_TYPE - 1) + 3].GetUInt32();

            if (wzc.data[season].rainChance > 100)
            {
                wzc.data[season].rainChance = 25;
                logging.error(
                    "Weather for zone %u season %u has wrong rain chance > "
                    "100%%",
                    zone_id, season);
            }

            if (wzc.data[season].snowChance > 100)
            {
                wzc.data[season].snowChance = 25;
                logging.error(
                    "Weather for zone %u season %u has wrong snow chance > "
                    "100%%",
                    zone_id, season);
            }

            if (wzc.data[season].stormChance > 100)
            {
                wzc.data[season].stormChance = 25;
                logging.error(
                    "Weather for zone %u season %u has wrong storm chance > "
                    "100%%",
                    zone_id, season);
            }
        }

        ++count;
    } while (result->NextRow());

    delete result;

    logging.info("Loaded %u weather definitions\n", count);
}

void ObjectMgr::DeleteCreatureData(uint32 guid)
{
    // remove mapid*cellid -> guid_set map
    CreatureData const* data = GetCreatureData(guid);
    if (data)
        remove_static_creature(data);

    mCreatureDataMap.erase(guid);
}

void ObjectMgr::DeleteGOData(uint32 guid)
{
    // remove mapid*cellid -> guid_set map
    GameObjectData const* data = GetGOData(guid);
    if (data)
        remove_static_game_object(data);

    mGameObjectDataMap.erase(guid);
}

void ObjectMgr::LoadQuestRelationsHelper(
    QuestRelationsMap& map, char const* table)
{
    map.clear(); // need for reload case

    uint32 count = 0;

    std::unique_ptr<QueryResult> result(
        WorldDatabase.PQuery("SELECT id,quest FROM %s", table));

    if (!result)
    {
        logging.info(
            "Loaded 0 quest relations from %s. DB table `%s` is empty.\n",
            table, table);
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 id = fields[0].GetUInt32();
        uint32 quest = fields[1].GetUInt32();

        if (mQuestTemplates.find(quest) == mQuestTemplates.end())
        {
            logging.error(
                "Table `%s: Quest %u listed for entry %u does not exist.",
                table, quest, id);
            continue;
        }

        map.insert(QuestRelationsMap::value_type(id, quest));

        ++count;
    } while (result->NextRow());

    logging.info("Loaded %u quest relations from %s\n", count, table);
}

void ObjectMgr::LoadGameobjectQuestRelations()
{
    LoadQuestRelationsHelper(m_GOQuestRelations, "gameobject_questrelation");

    for (auto& elem : m_GOQuestRelations)
    {
        GameObjectInfo const* goInfo = GetGameObjectInfo(elem.first);
        if (!goInfo)
            logging.error(
                "Table `gameobject_questrelation` have data for nonexistent "
                "gameobject entry (%u) and existing quest %u",
                elem.first, elem.second);
        else if (goInfo->type != GAMEOBJECT_TYPE_QUESTGIVER)
            logging.error(
                "Table `gameobject_questrelation` have data gameobject entry "
                "(%u) for quest %u, but GO is not GAMEOBJECT_TYPE_QUESTGIVER",
                elem.first, elem.second);
    }
}

void ObjectMgr::LoadGameobjectInvolvedRelations()
{
    LoadQuestRelationsHelper(
        m_GOQuestInvolvedRelations, "gameobject_involvedrelation");

    for (auto& elem : m_GOQuestInvolvedRelations)
    {
        GameObjectInfo const* goInfo = GetGameObjectInfo(elem.first);
        if (!goInfo)
            logging.error(
                "Table `gameobject_involvedrelation` have data for nonexistent "
                "gameobject entry (%u) and existing quest %u",
                elem.first, elem.second);
        else if (goInfo->type != GAMEOBJECT_TYPE_QUESTGIVER)
            logging.error(
                "Table `gameobject_involvedrelation` have data gameobject "
                "entry (%u) for quest %u, but GO is not "
                "GAMEOBJECT_TYPE_QUESTGIVER",
                elem.first, elem.second);
    }
}

void ObjectMgr::LoadCreatureQuestRelations()
{
    LoadQuestRelationsHelper(
        m_CreatureQuestRelations, "creature_questrelation");

    for (auto& elem : m_CreatureQuestRelations)
    {
        CreatureInfo const* cInfo = GetCreatureTemplate(elem.first);
        if (!cInfo)
            logging.error(
                "Table `creature_questrelation` have data for nonexistent "
                "creature entry (%u) and existing quest %u",
                elem.first, elem.second);
        else if (!(cInfo->npcflag & UNIT_NPC_FLAG_QUESTGIVER))
            logging.error(
                "Table `creature_questrelation` has creature entry (%u) for "
                "quest %u, but npcflag does not include "
                "UNIT_NPC_FLAG_QUESTGIVER",
                elem.first, elem.second);
    }
}

void ObjectMgr::LoadCreatureInvolvedRelations()
{
    LoadQuestRelationsHelper(
        m_CreatureQuestInvolvedRelations, "creature_involvedrelation");

    for (auto& elem : m_CreatureQuestInvolvedRelations)
    {
        CreatureInfo const* cInfo = GetCreatureTemplate(elem.first);
        if (!cInfo)
            logging.error(
                "Table `creature_involvedrelation` have data for nonexistent "
                "creature entry (%u) and existing quest %u",
                elem.first, elem.second);
        else if (!(cInfo->npcflag & UNIT_NPC_FLAG_QUESTGIVER))
            logging.error(
                "Table `creature_involvedrelation` has creature entry (%u) for "
                "quest %u, but npcflag does not include "
                "UNIT_NPC_FLAG_QUESTGIVER",
                elem.first, elem.second);
    }
}

void ObjectMgr::add_static_creature(const CreatureData* data)
{
    auto id = uint64(data->mapid) << uint64(32) |
              maps::cell_id(data->posX, data->posY);
    auto itr = static_creatures_.find(id);

    if (unlikely(itr == static_creatures_.end()))
    {
        itr = static_creatures_
                  .insert(std::make_pair(
                      id, google::dense_hash_set<const CreatureData*>{}))
                  .first;
        itr->second.set_empty_key(nullptr);
        itr->second.set_deleted_key(reinterpret_cast<CreatureData*>(1));
    }

    itr->second.insert(data);
}

void ObjectMgr::remove_static_creature(const CreatureData* data)
{
    auto id = uint64(data->mapid) << uint64(32) |
              maps::cell_id(data->posX, data->posY);
    auto itr = static_creatures_.find(id);

    if (unlikely(itr == static_creatures_.end()))
        return;

    itr->second.erase(data);
}

void ObjectMgr::add_static_game_object(const GameObjectData* data)
{
    auto info = GetGameObjectInfo(data->id);
    if (unlikely(!info))
    {
        logging.error(
            "ObjectMgr::add_static_game_object: Attempted to add GO with no "
            "Info");
        return;
    }

    if (unlikely(info->type == GAMEOBJECT_TYPE_TRANSPORT))
    {
        auto itr = static_elevators_.find(data->mapid);

        if (unlikely(itr == static_elevators_.end()))
        {
            itr = static_elevators_
                      .insert(std::make_pair((int)data->mapid,
                          google::dense_hash_set<const GameObjectData*>{}))
                      .first;
            itr->second.set_empty_key(nullptr);
            itr->second.set_deleted_key(reinterpret_cast<GameObjectData*>(1));
        }

        itr->second.insert(data);
        return;
    }

    auto id = uint64(data->mapid) << uint64(32) |
              maps::cell_id(data->posX, data->posY);
    auto itr = static_game_objects_.find(id);

    if (unlikely(itr == static_game_objects_.end()))
    {
        itr = static_game_objects_
                  .insert(std::make_pair(
                      id, google::dense_hash_set<const GameObjectData*>{}))
                  .first;
        itr->second.set_empty_key(nullptr);
        itr->second.set_deleted_key(reinterpret_cast<GameObjectData*>(1));
    }

    itr->second.insert(data);
}

void ObjectMgr::remove_static_game_object(const GameObjectData* data)
{
    auto info = GetGameObjectInfo(data->id);
    if (unlikely(!info))
    {
        logging.error(
            "ObjectMgr::remove_static_game_object: Attempted to remove GO with "
            "no Info");
        return;
    }

    if (unlikely(info->type == GAMEOBJECT_TYPE_TRANSPORT))
    {
        auto itr = static_game_objects_.find(data->mapid);
        if (likely(itr != static_game_objects_.end()))
            itr->second.erase(data);
        return;
    }

    auto id = uint64(data->mapid) << uint64(32) |
              maps::cell_id(data->posX, data->posY);
    auto itr = static_game_objects_.find(id);

    if (unlikely(itr == static_game_objects_.end()))
        return;

    itr->second.erase(data);
}

void ObjectMgr::add_static_corpse(Corpse* corpse)
{
    if (unlikely(corpse->GetOwnerGuid().IsEmpty()))
        return;

    auto id = uint64(corpse->GetMapId()) << uint64(32) |
              maps::cell_id(corpse->GetX(), corpse->GetY());
    auto itr = static_corpses_.find(id);

    if (unlikely(itr == static_corpses_.end()))
    {
        itr = static_corpses_.insert(
                                  std::make_pair(id,
                                      google::dense_hash_map<uint32, uint32>{}))
                  .first;
        itr->second.set_empty_key(0);
        itr->second.set_deleted_key(0xFFFFFFFF);
    }

    itr->second[corpse->GetOwnerGuid().GetCounter()] = corpse->GetInstanceId();
}

void ObjectMgr::remove_static_corpse(Corpse* corpse)
{
    if (unlikely(corpse->GetOwnerGuid().IsEmpty()))
        return;

    auto id = uint64(corpse->GetMapId()) << uint64(32) |
              maps::cell_id(corpse->GetX(), corpse->GetY());
    auto itr = static_corpses_.find(id);

    if (unlikely(itr == static_corpses_.end()))
        return;

    itr->second.erase(corpse->GetOwnerGuid().GetCounter());
}

const google::dense_hash_set<const CreatureData*>*
ObjectMgr::get_static_creatures(int map_id, int x, int y) const
{
    auto id = uint64(map_id) << uint64(32) | maps::cell_id(x, y);
    auto itr = static_creatures_.find(id);

    if (unlikely(itr == static_creatures_.end()))
        return nullptr;

    return &itr->second;
}

const google::dense_hash_set<const GameObjectData*>*
ObjectMgr::get_static_game_objects(int map_id, int x, int y) const
{
    auto id = uint64(map_id) << uint64(32) | maps::cell_id(x, y);
    auto itr = static_game_objects_.find(id);

    if (unlikely(itr == static_game_objects_.end()))
        return nullptr;

    return &itr->second;
}

const google::dense_hash_map<uint32, uint32>* ObjectMgr::get_static_corpses(
    int map_id, int x, int y) const
{
    auto id = uint64(map_id) << uint64(32) | maps::cell_id(x, y);
    auto itr = static_corpses_.find(id);

    if (unlikely(itr == static_corpses_.end()))
        return nullptr;

    return &itr->second;
}

const google::dense_hash_set<const GameObjectData*>*
ObjectMgr::get_static_elevators(int map_id) const
{
    auto itr = static_elevators_.find(map_id);

    if (unlikely(itr == static_elevators_.end()))
        return nullptr;

    return &itr->second;
}

void ObjectMgr::LoadReservedPlayersNames()
{
    m_ReservedNames.clear(); // need for reload case

    QueryResult* result = WorldDatabase.Query("SELECT name FROM reserved_name");

    uint32 count = 0;

    if (!result)
    {
        logging.info("Loaded %u reserved player names\n", count);
        return;
    }

    BarGoLink bar(result->GetRowCount());

    Field* fields;
    do
    {
        bar.step();
        fields = result->Fetch();
        std::string name = fields[0].GetCppString();

        std::wstring wstr;
        if (!Utf8toWStr(name, wstr))
        {
            logging.error(
                "Table `reserved_name` have invalid name: %s", name.c_str());
            continue;
        }

        wstrToLower(wstr);

        m_ReservedNames.insert(wstr);
        ++count;
    } while (result->NextRow());

    delete result;

    logging.info("Loaded %u reserved player names\n", count);
}

bool ObjectMgr::IsReservedName(const std::string& name) const
{
    std::wstring wstr;
    if (!Utf8toWStr(name, wstr))
        return false;

    wstrToLower(wstr);

    return m_ReservedNames.find(wstr) != m_ReservedNames.end();
}

enum LanguageType
{
    LT_BASIC_LATIN = 0x0000,
    LT_EXTENDEN_LATIN = 0x0001,
    LT_CYRILLIC = 0x0002,
    LT_EAST_ASIA = 0x0004,
    LT_ANY = 0xFFFF
};

static LanguageType GetRealmLanguageType(bool create)
{
    switch (sWorld::Instance()->getConfig(CONFIG_UINT32_REALM_ZONE))
    {
    case REALM_ZONE_UNKNOWN: // any language
    case REALM_ZONE_DEVELOPMENT:
    case REALM_ZONE_TEST_SERVER:
    case REALM_ZONE_QA_SERVER:
        return LT_ANY;
    case REALM_ZONE_UNITED_STATES: // extended-Latin
    case REALM_ZONE_OCEANIC:
    case REALM_ZONE_LATIN_AMERICA:
    case REALM_ZONE_ENGLISH:
    case REALM_ZONE_GERMAN:
    case REALM_ZONE_FRENCH:
    case REALM_ZONE_SPANISH:
        return LT_EXTENDEN_LATIN;
    case REALM_ZONE_KOREA: // East-Asian
    case REALM_ZONE_TAIWAN:
    case REALM_ZONE_CHINA:
        return LT_EAST_ASIA;
    case REALM_ZONE_RUSSIAN: // Cyrillic
        return LT_CYRILLIC;
    default:
        return create ? LT_BASIC_LATIN :
                        LT_ANY; // basic-Latin at create, any at login
    }
}

bool isValidString(std::wstring wstr, uint32 strictMask, bool numericOrSpace,
    bool create = false)
{
    if (strictMask == 0) // any language, ignore realm
    {
        if (isExtendedLatinString(wstr, numericOrSpace))
            return true;
        if (isCyrillicString(wstr, numericOrSpace))
            return true;
        if (isEastAsianString(wstr, numericOrSpace))
            return true;
        return false;
    }

    if (strictMask & 0x2) // realm zone specific
    {
        LanguageType lt = GetRealmLanguageType(create);
        if (lt & LT_EXTENDEN_LATIN)
            if (isExtendedLatinString(wstr, numericOrSpace))
                return true;
        if (lt & LT_CYRILLIC)
            if (isCyrillicString(wstr, numericOrSpace))
                return true;
        if (lt & LT_EAST_ASIA)
            if (isEastAsianString(wstr, numericOrSpace))
                return true;
    }

    if (strictMask & 0x1) // basic Latin
    {
        if (isBasicLatinString(wstr, numericOrSpace))
            return true;
    }

    return false;
}

uint8 ObjectMgr::CheckPlayerName(const std::string& name, bool create)
{
    std::wstring wname;
    if (!Utf8toWStr(name, wname))
        return CHAR_NAME_INVALID_CHARACTER;

    if (wname.size() > MAX_PLAYER_NAME)
        return CHAR_NAME_TOO_LONG;

    uint32 minName =
        sWorld::Instance()->getConfig(CONFIG_UINT32_MIN_PLAYER_NAME);
    if (wname.size() < minName)
        return CHAR_NAME_TOO_SHORT;

    uint32 strictMask =
        sWorld::Instance()->getConfig(CONFIG_UINT32_STRICT_PLAYER_NAMES);
    if (!isValidString(wname, strictMask, false, create))
        return CHAR_NAME_MIXED_LANGUAGES;

    return CHAR_NAME_SUCCESS;
}

bool ObjectMgr::IsValidCharterName(const std::string& name)
{
    std::wstring wname;
    if (!Utf8toWStr(name, wname))
        return false;

    if (wname.size() > MAX_CHARTER_NAME)
        return false;

    uint32 minName =
        sWorld::Instance()->getConfig(CONFIG_UINT32_MIN_CHARTER_NAME);
    if (wname.size() < minName)
        return false;

    uint32 strictMask =
        sWorld::Instance()->getConfig(CONFIG_UINT32_STRICT_CHARTER_NAMES);

    return isValidString(wname, strictMask, true);
}

PetNameInvalidReason ObjectMgr::CheckPetName(const std::string& name)
{
    std::wstring wname;
    if (!Utf8toWStr(name, wname))
        return PET_NAME_INVALID;

    if (wname.size() > MAX_PET_NAME)
        return PET_NAME_TOO_LONG;

    uint32 minName = sWorld::Instance()->getConfig(CONFIG_UINT32_MIN_PET_NAME);
    if (wname.size() < minName)
        return PET_NAME_TOO_SHORT;

    uint32 strictMask =
        sWorld::Instance()->getConfig(CONFIG_UINT32_STRICT_PET_NAMES);
    if (!isValidString(wname, strictMask, false))
        return PET_NAME_MIXED_LANGUAGES;

    return PET_NAME_SUCCESS;
}

int ObjectMgr::GetIndexForLocale(LocaleConstant loc)
{
    if (loc == LOCALE_enUS)
        return -1;

    for (size_t i = 0; i < m_LocalForIndex.size(); ++i)
        if (m_LocalForIndex[i] == loc)
            return i;

    return -1;
}

LocaleConstant ObjectMgr::GetLocaleForIndex(int i)
{
    if (i < 0 || i >= (int32)m_LocalForIndex.size())
        return LOCALE_enUS;

    return m_LocalForIndex[i];
}

int ObjectMgr::GetOrNewIndexForLocale(LocaleConstant loc)
{
    if (loc == LOCALE_enUS)
        return -1;

    for (size_t i = 0; i < m_LocalForIndex.size(); ++i)
        if (m_LocalForIndex[i] == loc)
            return i;

    m_LocalForIndex.push_back(loc);
    return m_LocalForIndex.size() - 1;
}

void ObjectMgr::LoadGameObjectForQuests()
{
    mGameObjectForQuestSet.clear(); // need for reload case

    if (!sGOStorage.MaxEntry)
    {
        logging.info("Loaded 0 GameObjects for quests\n");
        return;
    }

    BarGoLink bar(sGOStorage.MaxEntry - 1);
    uint32 count = 0;

    // collect GO entries for GO that must activated
    for (uint32 go_entry = 1; go_entry < sGOStorage.MaxEntry; ++go_entry)
    {
        bar.step();
        GameObjectInfo const* goInfo = GetGameObjectInfo(go_entry);
        if (!goInfo)
            continue;

        switch (goInfo->type)
        {
        case GAMEOBJECT_TYPE_QUESTGIVER:
        {
            if (m_GOQuestRelations.find(go_entry) != m_GOQuestRelations.end() ||
                m_GOQuestInvolvedRelations.find(go_entry) !=
                    m_GOQuestInvolvedRelations.end())
            {
                mGameObjectForQuestSet.insert(go_entry);
                ++count;
            }

            break;
        }
        case GAMEOBJECT_TYPE_CHEST:
        {
            // scan GO chest with loot including quest items
            uint32 loot_id = goInfo->GetLootId();

            // always activate to quest, GO may not have loot, OR find if GO has
            // loot for quest.
            if (goInfo->chest.questId ||
                LootTemplates_Gameobject.HaveQuestLootFor(loot_id))
            {
                mGameObjectForQuestSet.insert(go_entry);
                ++count;
            }
            break;
        }
        case GAMEOBJECT_TYPE_GENERIC:
        {
            if (goInfo->_generic
                    .questID) // quest related objects, has visual effects
            {
                mGameObjectForQuestSet.insert(go_entry);
                count++;
            }
            break;
        }
        case GAMEOBJECT_TYPE_SPELL_FOCUS:
        {
            if (goInfo->spellFocus
                    .questID) // quest related objects, has visual effect
            {
                mGameObjectForQuestSet.insert(go_entry);
                count++;
            }
            break;
        }
        case GAMEOBJECT_TYPE_GOOBER:
        {
            if (goInfo->goober.questId) // quests objects
            {
                mGameObjectForQuestSet.insert(go_entry);
                count++;
            }
            break;
        }
        default:
            break;
        }
    }

    logging.info("Loaded %u GameObjects for quests\n", count);
}

bool ObjectMgr::LoadMangosStrings(
    DatabaseType& db, char const* table, int32 min_value, int32 max_value)
{
    int32 start_value = min_value;
    int32 end_value = max_value;
    // some string can have negative indexes range
    if (start_value < 0)
    {
        if (end_value >= start_value)
        {
            logging.error(
                "Table '%s' attempt loaded with invalid range (%d - %d), "
                "strings not loaded.",
                table, min_value, max_value);
            return false;
        }

        // real range (max+1,min+1) exaple: (-10,-1000) -> -999...-10+1
        std::swap(start_value, end_value);
        ++start_value;
        ++end_value;
    }
    else
    {
        if (start_value >= end_value)
        {
            logging.error(
                "Table '%s' attempt loaded with invalid range (%d - %d), "
                "strings not loaded.",
                table, min_value, max_value);
            return false;
        }
    }

    // cleanup affected map part for reloading case
    for (auto itr = mMangosStringLocaleMap.begin();
         itr != mMangosStringLocaleMap.end();)
    {
        if (itr->first >= start_value && itr->first < end_value)
            mMangosStringLocaleMap.erase(itr++);
        else
            ++itr;
    }

    std::unique_ptr<QueryResult> result(db.PQuery(
        "SELECT "
        "entry,content_default,content_loc1,content_loc2,content_loc3,content_"
        "loc4,content_loc5,content_loc6,content_loc7,content_loc8 FROM %s",
        table));

    if (!result)
    {
        if (min_value ==
            MIN_MANGOS_STRING_ID) // error only in case internal strings
            logging.error(
                "Loaded 0 mangos strings. DB table `%s` is empty. Cannot "
                "continue.\n",
                table);
        else
            logging.info(
                "Loaded 0 string templates. DB table `%s` is empty.\n", table);
        return false;
    }

    uint32 count = 0;

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        int32 entry = fields[0].GetInt32();

        if (entry == 0)
        {
            logging.error(
                "Table `%s` contain reserved entry 0, ignored.", table);
            continue;
        }
        else if (entry < start_value || entry >= end_value)
        {
            logging.error(
                "Table `%s` contain entry %i out of allowed range (%d - %d), "
                "ignored.",
                table, entry, min_value, max_value);
            continue;
        }

        MangosStringLocale& data = mMangosStringLocaleMap[entry];

        if (data.Content.size() > 0)
        {
            logging.error(
                "Table `%s` contain data for already loaded entry  %i (from "
                "another table?), ignored.",
                table, entry);
            continue;
        }

        data.Content.resize(1);
        ++count;

        // 0 -> default, idx in to idx+1
        data.Content[0] = fields[1].GetCppString();

        for (int i = 1; i < MAX_LOCALE; ++i)
        {
            std::string str = fields[i + 1].GetCppString();
            if (!str.empty())
            {
                int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    // 0 -> default, idx in to idx+1
                    if ((int32)data.Content.size() <= idx + 1)
                        data.Content.resize(idx + 2);

                    data.Content[idx + 1] = str;
                }
            }
        }
    } while (result->NextRow());

    if (min_value == MIN_MANGOS_STRING_ID)
        logging.info("Loaded %u MaNGOS strings from table %s\n", count, table);
    else
        logging.info("Loaded %u string templates from %s\n", count, table);

    return true;
}

const char* ObjectMgr::GetMangosString(int32 entry, int locale_idx) const
{
    // locale_idx==-1 -> default, locale_idx >= 0 in to idx+1
    // Content[0] always exist if exist MangosStringLocale
    if (MangosStringLocale const* msl = GetMangosStringLocale(entry))
    {
        if ((int32)msl->Content.size() > locale_idx + 1 &&
            !msl->Content[locale_idx + 1].empty())
            return msl->Content[locale_idx + 1].c_str();
        else
            return msl->Content[0].c_str();
    }

    if (entry > MIN_DB_SCRIPT_STRING_ID)
        logging.error("Entry %i not found in `db_script_string` table.", entry);
    else if (entry > 0)
        logging.error("Entry %i not found in `mangos_string` table.", entry);
    else if (entry > MAX_CREATURE_AI_TEXT_STRING_ID)
        logging.error(
            "Entry %i not found in `creature_ai_texts` table.", entry);
    else
        logging.error("Mangos string entry %i not found in DB.", entry);
    return "<error>";
}

void ObjectMgr::LoadFishingBaseSkillLevel()
{
    mFishingBaseForArea.clear(); // for reload case

    uint32 count = 0;
    QueryResult* result =
        WorldDatabase.Query("SELECT entry,skill FROM skill_fishing_base_level");

    if (!result)
    {
        logging.error("Loaded `skill_fishing_base_level`, table is empty!\n");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        bar.step();

        Field* fields = result->Fetch();
        uint32 entry = fields[0].GetUInt32();
        int32 skill = fields[1].GetInt32();

        AreaTableEntry const* fArea = GetAreaEntryByAreaID(entry);
        if (!fArea)
        {
            logging.error(
                "AreaId %u defined in `skill_fishing_base_level` does not "
                "exist",
                entry);
            continue;
        }

        mFishingBaseForArea[entry] = skill;
        ++count;
    } while (result->NextRow());

    delete result;

    logging.info("Loaded %u areas for fishing base skill level\n", count);
}

// Searches for the same condition already in Conditions store
// Returns Id if found, else adds it to Conditions and returns Id
uint16 ObjectMgr::GetConditionId(
    ConditionType condition, uint32 value1, uint32 value2)
{
    PlayerCondition lc = PlayerCondition(0, condition, value1, value2);
    for (uint16 i = 0; i < mConditions.size(); ++i)
    {
        if (lc == mConditions[i])
            return i;
    }

    mConditions.push_back(lc);

    if (mConditions.size() > 0xFFFF)
    {
        logging.error(
            "Conditions store overflow! Current and later loaded conditions "
            "will ignored!");
        return 0;
    }

    return mConditions.size() - 1;
}

bool ObjectMgr::CheckDeclinedNames(
    std::wstring mainpart, DeclinedName const& names)
{
    for (int i = 0; i < MAX_DECLINED_NAME_CASES; ++i)
    {
        std::wstring wname;
        if (!Utf8toWStr(names.name[i], wname))
            return false;

        if (mainpart != GetMainPartOfName(wname, i + 1))
            return false;
    }
    return true;
}

// Checks if player meets the condition
bool PlayerCondition::Meets(Player const* player) const
{
    if (!player)
        return false; // player not present, return false

    switch (m_condition)
    {
    case CONDITION_OR:
        // Checked on load
        return sConditionStorage.LookupEntry<PlayerCondition>(m_value1)->Meets(
                   player) ||
               sConditionStorage.LookupEntry<PlayerCondition>(m_value2)->Meets(
                   player);
    case CONDITION_AND:
        // Checked on load
        return sConditionStorage.LookupEntry<PlayerCondition>(m_value1)->Meets(
                   player) &&
               sConditionStorage.LookupEntry<PlayerCondition>(m_value2)->Meets(
                   player);
    case CONDITION_NONE:
        return true; // empty condition, always met
    case CONDITION_AURA:
        return player->has_aura(m_value1);
    case CONDITION_ITEM:
        return player->HasItemCount(m_value1, m_value2);
    case CONDITION_ITEM_EQUIPPED:
        return player->has_item_equipped(m_value1);
    case CONDITION_AREAID:
    {
        uint32 zone, area;
        player->GetZoneAndAreaId(zone, area);
        return (zone == m_value1 || area == m_value1) == (m_value2 == 0);
    }
    case CONDITION_REPUTATION_RANK:
    {
        FactionEntry const* faction = sFactionStore.LookupEntry(m_value1);
        return faction &&
               player->GetReputationMgr().GetRank(faction) >=
                   ReputationRank(m_value2);
    }
    case CONDITION_TEAM:
        return uint32(player->GetTeam()) == m_value1;
    case CONDITION_SKILL:
        return player->HasSkill(m_value1) &&
               player->GetBaseSkillValue(m_value1) >= m_value2;
    case CONDITION_QUESTREWARDED:
        return player->GetQuestRewardStatus(m_value1);
    case CONDITION_QUESTTAKEN:
    {
        return player->IsCurrentQuest(m_value1, m_value2);
    }
    case CONDITION_AD_COMMISSION_AURA:
    {
        bool found = false;
        player->loop_auras([&found](AuraHolder* holder)
            {
                if ((holder->GetSpellProto()->HasAttribute(
                         SPELL_ATTR_CASTABLE_WHILE_MOUNTED) ||
                        holder->GetSpellProto()->HasAttribute(
                            SPELL_ATTR_ABILITY)) &&
                    holder->GetSpellProto()->SpellVisual == 3580)
                    found = true;
                return !found; // break when found is true
            });
        return found;
    }
    case CONDITION_NO_AURA:
        return !player->has_aura(m_value1);
    case CONDITION_ACTIVE_GAME_EVENT:
        return sGameEventMgr::Instance()->IsActiveEvent(m_value1);
    case CONDITION_AREA_FLAG:
    {
        if (AreaTableEntry const* pAreaEntry =
                GetAreaEntryByAreaID(player->GetAreaId()))
        {
            if ((!m_value1 || (pAreaEntry->flags & m_value1)) &&
                (!m_value2 || !(pAreaEntry->flags & m_value2)))
                return true;
        }
        return false;
    }
    case CONDITION_RACE_CLASS:
        if ((!m_value1 || (player->getRaceMask() & m_value1)) &&
            (!m_value2 || (player->getClassMask() & m_value2)))
            return true;
        return false;
    case CONDITION_LEVEL:
    {
        switch (m_value2)
        {
        case 0:
            return player->getLevel() == m_value1;
        case 1:
            return player->getLevel() >= m_value1;
        case 2:
            return player->getLevel() <= m_value1;
        }
        return false;
    }
    case CONDITION_NOITEM:
        return !player->HasItemCount(m_value1, m_value2);
    case CONDITION_SPELL:
    {
        switch (m_value2)
        {
        case 0:
            return player->HasSpell(m_value1);
        case 1:
            return !player->HasSpell(m_value1);
        }
        return false;
    }
    case CONDITION_INSTANCE_SCRIPT:
    {
        // have meaning only for specific map instance script so ignore other
        // maps
        if (player->GetMapId() != m_value1)
            return false;
        if (InstanceData* data = player->GetInstanceData())
            return data->CheckConditionCriteriaMeet(player, m_value1, m_value2);
        return false;
    }
    case CONDITION_QUESTAVAILABLE:
    {
        if (Quest const* quest =
                sObjectMgr::Instance()->GetQuestTemplate(m_value1))
            return player->CanTakeQuest(quest, false, nullptr);
        else
            return false;
    }
    case CONDITION_RESERVED_1:
    case CONDITION_RESERVED_2:
        return false;
    case CONDITION_QUEST_NONE:
    {
        if (!player->IsCurrentQuest(m_value1) &&
            !player->GetQuestRewardStatus(m_value1))
            return true;
        return false;
    }
    case CONDITION_ITEM_WITH_BANK:
        return player->HasItemCount(m_value1, m_value2, true);
    case CONDITION_NOITEM_WITH_BANK:
        return !player->HasItemCount(m_value1, m_value2, true);
    case CONDITION_NOT_ACTIVE_GAME_EVENT:
        return !sGameEventMgr::Instance()->IsActiveEvent(m_value1);
    case CONDITION_ACTIVE_HOLIDAY:
        return sGameEventMgr::Instance()->IsActiveHoliday(HolidayIds(m_value1));
    case CONDITION_NOT_ACTIVE_HOLIDAY:
        return !sGameEventMgr::Instance()->IsActiveHoliday(
            HolidayIds(m_value1));
    case CONDITION_LEARNABLE_ABILITY:
    {
        // Already know the spell
        if (player->HasSpell(m_value1))
            return false;

        // If item defined, check if player has the item already.
        if (m_value2)
        {
            // Hard coded item count. This should be ok, since the intention
            // with this condition is to have
            // a all-in-one check regarding items that learn some ability
            // (primary/secondary tradeskills).
            // Commonly, items like this is unique and/or are not expected to be
            // obtained more than once.
            if (player->HasItemCount(m_value2, 1, true))
                return false;
        }

        bool isSkillOk = false;

        SkillLineAbilityMapBounds bounds =
            sSpellMgr::Instance()->GetSkillLineAbilityMapBounds(m_value1);

        for (auto itr = bounds.first; itr != bounds.second; ++itr)
        {
            const SkillLineAbilityEntry* skillInfo = itr->second;

            if (!skillInfo)
                continue;

            // doesn't have skill
            if (!player->HasSkill(skillInfo->skillId))
                return false;

            // doesn't match class
            if (skillInfo->classmask &&
                (skillInfo->classmask & player->getClassMask()) == 0)
                return false;

            // doesn't match race
            if (skillInfo->racemask &&
                (skillInfo->racemask & player->getRaceMask()) == 0)
                return false;

            // skill level too low
            if (skillInfo->min_value >
                player->GetSkillValue(skillInfo->skillId))
                return false;

            isSkillOk = true;
            break;
        }

        if (isSkillOk)
            return true;

        return false;
    }
    case CONDITION_SKILL_BELOW:
        if (m_value2 == 1)
            return !player->HasSkill(m_value1);
        else
            return player->HasSkill(m_value1) &&
                   player->GetBaseSkillValue(m_value1) < m_value2;
    case CONDITION_OBJECTIVE_NOT_COMPLETE:
        if (m_value2 <= 3)
            return !player->IsQuestObjectiveComplete(m_value1, m_value2);
        return false;
    case CONDITION_OBJECTIVE_COMPLETE:
        if (m_value2 <= 3)
            return player->IsQuestObjectiveComplete(m_value1, m_value2);
        return false;
    default:
        return false;
    }
}

// Verification of condition values validity
bool PlayerCondition::IsValid(
    uint16 entry, ConditionType condition, uint32 value1, uint32 value2)
{
    switch (condition)
    {
    case CONDITION_OR:
    case CONDITION_AND:
    {
        if (value1 >= entry)
        {
            logging.error(
                "And or Or condition (entry %u, type %d) has invalid value1 "
                "%u, must be lower than entry, skipped",
                entry, condition, value1);
            return false;
        }
        if (value2 >= entry)
        {
            logging.error(
                "And or Or condition (entry %u, type %d) has invalid value2 "
                "%u, must be lower than entry, skipped",
                entry, condition, value2);
            return false;
        }
        const PlayerCondition* condition1 =
            sConditionStorage.LookupEntry<PlayerCondition>(value1);
        if (!condition1)
        {
            logging.error(
                "And or Or condition (entry %u, type %d) has value1 %u without "
                "proper condition, skipped",
                entry, condition, value1);
            return false;
        }
        const PlayerCondition* condition2 =
            sConditionStorage.LookupEntry<PlayerCondition>(value2);
        if (!condition2)
        {
            logging.error(
                "And or Or condition (entry %u, type %d) has value2 %u without "
                "proper condition, skipped",
                entry, condition, value2);
            return false;
        }
        break;
    }
    case CONDITION_AURA:
    {
        if (!sSpellStore.LookupEntry(value1))
        {
            logging.error(
                "Aura condition (entry %u, type %u) requires to have non "
                "existing spell (Id: %d), skipped",
                entry, condition, value1);
            return false;
        }
        if (value2 >= MAX_EFFECT_INDEX)
        {
            logging.error(
                "Aura condition (entry %u, type %u) requires to have non "
                "existing effect index (%u) (must be 0..%u), skipped",
                entry, condition, value2, MAX_EFFECT_INDEX - 1);
            return false;
        }
        break;
    }
    case CONDITION_ITEM:
    case CONDITION_NOITEM:
    case CONDITION_ITEM_WITH_BANK:
    case CONDITION_NOITEM_WITH_BANK:
    {
        ItemPrototype const* proto = ObjectMgr::GetItemPrototype(value1);
        if (!proto)
        {
            logging.error(
                "Item condition (entry %u, type %u) requires to have non "
                "existing item (%u), skipped",
                entry, condition, value1);
            return false;
        }

        if (value2 < 1)
        {
            logging.error(
                "Item condition (entry %u, type %u) useless with count < 1, "
                "skipped",
                entry, condition);
            return false;
        }
        break;
    }
    case CONDITION_ITEM_EQUIPPED:
    {
        ItemPrototype const* proto = ObjectMgr::GetItemPrototype(value1);
        if (!proto)
        {
            logging.error(
                "ItemEquipped condition (entry %u, type %u) requires to have "
                "non existing item (%u) equipped, skipped",
                entry, condition, value1);
            return false;
        }
        break;
    }
    case CONDITION_AREAID:
    {
        AreaTableEntry const* areaEntry = GetAreaEntryByAreaID(value1);
        if (!areaEntry)
        {
            logging.error(
                "Zone condition (entry %u, type %u) requires to be in non "
                "existing area (%u), skipped",
                entry, condition, value1);
            return false;
        }

        if (value2 > 1)
        {
            logging.error(
                "Zone condition (entry %u, type %u) has invalid argument %u "
                "(must be 0..1), skipped",
                entry, condition, value2);
            return false;
        }
        break;
    }
    case CONDITION_REPUTATION_RANK:
    {
        FactionEntry const* factionEntry = sFactionStore.LookupEntry(value1);
        if (!factionEntry)
        {
            logging.error(
                "Reputation condition (entry %u, type %u) requires to have "
                "reputation non existing faction (%u), skipped",
                entry, condition, value1);
            return false;
        }
        break;
    }
    case CONDITION_TEAM:
    {
        if (value1 != ALLIANCE && value1 != HORDE)
        {
            logging.error(
                "Team condition (entry %u, type %u) specifies unknown team "
                "(%u), skipped",
                entry, condition, value1);
            return false;
        }
        break;
    }
    case CONDITION_SKILL:
    case CONDITION_SKILL_BELOW:
    {
        SkillLineEntry const* pSkill = sSkillLineStore.LookupEntry(value1);
        if (!pSkill)
        {
            logging.error(
                "Skill condition (entry %u, type %u) specifies non-existing "
                "skill (%u), skipped",
                entry, condition, value1);
            return false;
        }
        if (value2 < 1 || value2 > sWorld::Instance()->GetConfigMaxSkillValue())
        {
            logging.error(
                "Skill condition (entry %u, type %u) specifies invalid skill "
                "value (%u), skipped",
                entry, condition, value2);
            return false;
        }
        break;
    }
    case CONDITION_QUESTREWARDED:
    case CONDITION_QUESTTAKEN:
    case CONDITION_QUESTAVAILABLE:
    case CONDITION_QUEST_NONE:
    {
        Quest const* Quest = sObjectMgr::Instance()->GetQuestTemplate(value1);
        if (!Quest)
        {
            logging.error(
                "Quest condition (entry %u, type %u) specifies non-existing "
                "quest (%u), skipped",
                entry, condition, value1);
            return false;
        }

        if (value2 && condition != CONDITION_QUESTTAKEN)
            logging.error(
                "Quest condition (entry %u, type %u) has useless data in "
                "value2 (%u)!",
                entry, condition, value2);
        break;
    }
    case CONDITION_AD_COMMISSION_AURA:
    {
        if (value1)
            logging.error(
                "Quest condition (entry %u, type %u) has useless data in "
                "value1 (%u)!",
                entry, condition, value1);
        if (value2)
            logging.error(
                "Quest condition (entry %u, type %u) has useless data in "
                "value2 (%u)!",
                entry, condition, value2);
        break;
    }
    case CONDITION_NO_AURA:
    {
        if (!sSpellStore.LookupEntry(value1))
        {
            logging.error(
                "Aura condition (entry %u, type %u) requires to have non "
                "existing spell (Id: %d), skipped",
                entry, condition, value1);
            return false;
        }
        if (value2 > MAX_EFFECT_INDEX)
        {
            logging.error(
                "Aura condition (entry %u, type %u) requires to have non "
                "existing effect index (%u) (must be 0..%u), skipped",
                entry, condition, value2, MAX_EFFECT_INDEX - 1);
            return false;
        }
        break;
    }
    case CONDITION_ACTIVE_GAME_EVENT:
    case CONDITION_NOT_ACTIVE_GAME_EVENT:
    {
        if (!sGameEventMgr::Instance()->IsValidEvent(value1))
        {
            logging.error(
                "(Not)Active event condition (entry %u, type %u) requires "
                "existing event id (%u), skipped",
                entry, condition, value1);
            return false;
        }
        break;
    }
    case CONDITION_AREA_FLAG:
    {
        if (!value1 && !value2)
        {
            logging.error(
                "Area flag condition (entry %u, type %u) has both values like "
                "0, skipped",
                entry, condition);
            return false;
        }
        break;
    }
    case CONDITION_RACE_CLASS:
    {
        if (!value1 && !value2)
        {
            logging.error(
                "Race_class condition (entry %u, type %u) has both values like "
                "0, skipped",
                entry, condition);
            return false;
        }

        if (value1 && !(value1 & RACEMASK_ALL_PLAYABLE))
        {
            logging.error(
                "Race_class condition (entry %u, type %u) has invalid player "
                "class %u, skipped",
                entry, condition, value1);
            return false;
        }

        if (value2 && !(value2 & CLASSMASK_ALL_PLAYABLE))
        {
            logging.error(
                "Race_class condition (entry %u, type %u) has invalid race "
                "mask %u, skipped",
                entry, condition, value2);
            return false;
        }
        break;
    }
    case CONDITION_LEVEL:
    {
        if (!value1 ||
            value1 >
                sWorld::Instance()->getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
        {
            logging.error(
                "Level condition (entry %u, type %u)has invalid level %u, "
                "skipped",
                entry, condition, value1);
            return false;
        }

        if (value2 > 2)
        {
            logging.error(
                "Level condition (entry %u, type %u) has invalid argument %u "
                "(must be 0..2), skipped",
                entry, condition, value2);
            return false;
        }

        break;
    }
    case CONDITION_SPELL:
    {
        if (!sSpellStore.LookupEntry(value1))
        {
            logging.error(
                "Spell condition (entry %u, type %u) requires to have non "
                "existing spell (Id: %d), skipped",
                entry, condition, value1);
            return false;
        }

        if (value2 > 1)
        {
            logging.error(
                "Spell condition (entry %u, type %u) has invalid argument %u "
                "(must be 0..1), skipped",
                entry, condition, value2);
            return false;
        }

        break;
    }
    case CONDITION_INSTANCE_SCRIPT:
    {
        MapEntry const* mapEntry = sMapStore.LookupEntry(value1);
        if (!mapEntry || !mapEntry->IsDungeon())
        {
            logging.error(
                "Instance script condition (entry %u, type %u) has nonexistent "
                "map id %u as first arg, skipped",
                entry, condition, value1);
            return false;
        }

        break;
    }
    case CONDITION_RESERVED_1:
    case CONDITION_RESERVED_2:
    {
        logging.error(
            "Condition (%u) reserved for later versions, skipped", condition);
        return false;
    }
    case CONDITION_ACTIVE_HOLIDAY:
    case CONDITION_NOT_ACTIVE_HOLIDAY:
        // no way check holidays in pre-3.x
        break;
    case CONDITION_LEARNABLE_ABILITY:
    {
        SkillLineAbilityMapBounds bounds =
            sSpellMgr::Instance()->GetSkillLineAbilityMapBounds(value1);

        if (bounds.first == bounds.second)
        {
            logging.error(
                "Learnable ability conditon (entry %u, type %u) has spell id "
                "%u defined, but this spell is not listed in SkillLineAbility "
                "and can not be used, skipping.",
                entry, condition, value1);
            return false;
        }

        if (value2)
        {
            ItemPrototype const* proto = ObjectMgr::GetItemPrototype(value2);
            if (!proto)
            {
                logging.error(
                    "Learnable ability conditon (entry %u, type %u) has item "
                    "entry %u defined but item does not exist, skipping.",
                    entry, condition, value2);
                return false;
            }
        }

        break;
    }
    case CONDITION_NONE:
        break;
    default:
        logging.error("Condition entry %u has bad type of %d, skipped ", entry,
            condition);
        return false;
    }
    return true;
}

SkillRangeType GetSkillRangeType(SkillLineEntry const* pSkill, bool racial)
{
    switch (pSkill->categoryId)
    {
    case SKILL_CATEGORY_LANGUAGES:
        return SKILL_RANGE_LANGUAGE;
    case SKILL_CATEGORY_WEAPON:
        if (pSkill->id != SKILL_FIST_WEAPONS)
            return SKILL_RANGE_LEVEL;
        else
            return SKILL_RANGE_MONO;
    case SKILL_CATEGORY_ARMOR:
    case SKILL_CATEGORY_CLASS:
        if (pSkill->id != SKILL_POISONS && pSkill->id != SKILL_LOCKPICKING)
            return SKILL_RANGE_MONO;
        else
            return SKILL_RANGE_LEVEL;
    case SKILL_CATEGORY_SECONDARY:
    case SKILL_CATEGORY_PROFESSION:
        // not set skills for professions and racial abilities
        if (IsProfessionSkill(pSkill->id))
            return SKILL_RANGE_RANK;
        else if (racial)
            return SKILL_RANGE_NONE;
        else
            return SKILL_RANGE_MONO;
    default:
    case SKILL_CATEGORY_ATTRIBUTES: // not found in dbc
    case SKILL_CATEGORY_GENERIC:    // only GENERIC(DND)
        return SKILL_RANGE_NONE;
    }
}

void ObjectMgr::LoadGameTele()
{
    m_GameTeleMap.clear(); // for reload case

    uint32 count = 0;
    QueryResult* result = WorldDatabase.Query(
        "SELECT id, position_x, position_y, position_z, orientation, map, name "
        "FROM game_tele");

    if (!result)
    {
        logging.error("Loaded `game_tele`, table is empty!\n");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        bar.step();

        Field* fields = result->Fetch();

        uint32 id = fields[0].GetUInt32();

        GameTele gt;

        gt.position_x = fields[1].GetFloat();
        gt.position_y = fields[2].GetFloat();
        gt.position_z = fields[3].GetFloat();
        gt.orientation = fields[4].GetFloat();
        gt.mapId = fields[5].GetUInt32();
        gt.name = fields[6].GetCppString();

        if (!maps::verify_coords(gt.position_x, gt.position_y))
        {
            logging.error(
                "Wrong position for id %u (name: %s) in `game_tele` table, "
                "ignoring.",
                id, gt.name.c_str());
            continue;
        }

        if (!Utf8toWStr(gt.name, gt.wnameLow))
        {
            logging.error(
                "Wrong UTF8 name for id %u in `game_tele` table, ignoring.",
                id);
            continue;
        }

        wstrToLower(gt.wnameLow);

        m_GameTeleMap[id] = gt;

        ++count;
    } while (result->NextRow());
    delete result;

    logging.info("Loaded %u GameTeleports\n", count);
}

GameTele const* ObjectMgr::GetGameTele(
    const std::string& name, bool partialMatch) const
{
    // explicit name case
    std::wstring wname;
    if (!Utf8toWStr(name, wname))
        return nullptr;

    // converting string that we try to find to lower case
    wstrToLower(wname);

    // Alternative first GameTele what contains wnameLow as substring in case no
    // GameTele location found
    const GameTele* alt = nullptr;
    for (const auto& elem : m_GameTeleMap)
        if (elem.second.wnameLow == wname)
            return &elem.second;
        else if (partialMatch && alt == nullptr &&
                 elem.second.wnameLow.find(wname) != std::wstring::npos)
            alt = &elem.second;

    return alt;
}

bool ObjectMgr::AddGameTele(GameTele& tele)
{
    // find max id
    uint32 new_id = 0;
    for (GameTeleMap::const_iterator itr = m_GameTeleMap.begin();
         itr != m_GameTeleMap.end(); ++itr)
        if (itr->first > new_id)
            new_id = itr->first;

    // use next
    ++new_id;

    if (!Utf8toWStr(tele.name, tele.wnameLow))
        return false;

    wstrToLower(tele.wnameLow);

    m_GameTeleMap[new_id] = tele;
    std::string safeName(tele.name);
    WorldDatabase.escape_string(safeName);

    return WorldDatabase.PExecuteLog(
        "INSERT INTO game_tele "
        "(id,position_x,position_y,position_z,orientation,map,name) "
        "VALUES (%u,%f,%f,%f,%f,%u,'%s')",
        new_id, tele.position_x, tele.position_y, tele.position_z,
        tele.orientation, tele.mapId, safeName.c_str());
}

bool ObjectMgr::DeleteGameTele(const std::string& name)
{
    // explicit name case
    std::wstring wname;
    if (!Utf8toWStr(name, wname))
        return false;

    // converting string that we try to find to lower case
    wstrToLower(wname);

    for (auto itr = m_GameTeleMap.begin(); itr != m_GameTeleMap.end(); ++itr)
    {
        if (itr->second.wnameLow == wname)
        {
            WorldDatabase.PExecuteLog("DELETE FROM game_tele WHERE name = '%s'",
                itr->second.name.c_str());
            m_GameTeleMap.erase(itr);
            return true;
        }
    }

    return false;
}

void ObjectMgr::LoadMailLevelRewards()
{
    m_mailLevelRewardMap.clear(); // for reload case

    uint32 count = 0;
    QueryResult* result = WorldDatabase.Query(
        "SELECT level, raceMask, mailTemplateId, senderEntry FROM "
        "mail_level_reward");

    if (!result)
    {
        logging.error("Loaded `mail_level_reward`, table is empty!\n");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        bar.step();

        Field* fields = result->Fetch();

        uint8 level = fields[0].GetUInt8();
        uint32 raceMask = fields[1].GetUInt32();
        uint32 mailTemplateId = fields[2].GetUInt32();
        uint32 senderEntry = fields[3].GetUInt32();

        if (level > MAX_LEVEL)
        {
            logging.error(
                "Table `mail_level_reward` have data for level %u that more "
                "supported by client (%u), ignoring.",
                level, MAX_LEVEL);
            continue;
        }

        if (!(raceMask & RACEMASK_ALL_PLAYABLE))
        {
            logging.error(
                "Table `mail_level_reward` have raceMask (%u) for level %u "
                "that not include any player races, ignoring.",
                raceMask, level);
            continue;
        }

        if (!sMailTemplateStore.LookupEntry(mailTemplateId))
        {
            logging.error(
                "Table `mail_level_reward` have invalid mailTemplateId (%u) "
                "for level %u that invalid not include any player races, "
                "ignoring.",
                mailTemplateId, level);
            continue;
        }

        if (!GetCreatureTemplate(senderEntry))
        {
            logging.error(
                "Table `mail_level_reward` have nonexistent sender creature "
                "entry (%u) for level %u that invalid not include any player "
                "races, ignoring.",
                senderEntry, level);
            continue;
        }

        m_mailLevelRewardMap[level].push_back(
            MailLevelReward(raceMask, mailTemplateId, senderEntry));

        ++count;
    } while (result->NextRow());
    delete result;

    logging.info("Loaded %u level dependent mail rewards,\n", count);
}

void ObjectMgr::LoadTrainers(char const* tableName, bool isTemplates)
{
    CacheTrainerSpellMap& trainerList =
        isTemplates ? m_mCacheTrainerTemplateSpellMap : m_mCacheTrainerSpellMap;

    // For reload case
    for (auto& elem : trainerList)
        elem.second.Clear();
    trainerList.clear();

    std::set<uint32> skip_trainers;

    std::unique_ptr<QueryResult> result(WorldDatabase.PQuery(
        "SELECT entry, spell,spellcost,reqskill,reqskillvalue,reqlevel FROM %s",
        tableName));

    if (!result)
    {
        logging.info("Loaded `%s`, table is empty!\n", tableName);
        return;
    }

    BarGoLink bar(result->GetRowCount());

    std::set<uint32> talentIds;

    uint32 count = 0;
    do
    {
        bar.step();

        Field* fields = result->Fetch();

        uint32 entry = fields[0].GetUInt32();
        uint32 spell = fields[1].GetUInt32();

        SpellEntry const* spellinfo = sSpellStore.LookupEntry(spell);
        if (!spellinfo)
        {
            logging.error(
                "Table `%s` (Entry: %u ) has non existing spell %u, ignore",
                tableName, entry, spell);
            continue;
        }

        if (!SpellMgr::IsSpellValid(spellinfo))
        {
            logging.error(
                "Table `%s` (Entry: %u) has broken learning spell %u, ignore",
                tableName, entry, spell);
            continue;
        }

        if (GetTalentSpellCost(spell))
        {
            if (talentIds.find(spell) == talentIds.end())
            {
                logging.error(
                    "Table `%s` has talent as learning spell %u, ignore",
                    tableName, spell);
                talentIds.insert(spell);
            }
            continue;
        }

        if (!isTemplates)
        {
            CreatureInfo const* cInfo = GetCreatureTemplate(entry);

            if (!cInfo)
            {
                logging.error(
                    "Table `%s` have entry for nonexistent creature template "
                    "(Entry: %u), ignore",
                    tableName, entry);
                continue;
            }

            if (!(cInfo->npcflag & UNIT_NPC_FLAG_TRAINER))
            {
                if (skip_trainers.find(entry) == skip_trainers.end())
                {
                    logging.error(
                        "Table `%s` have data for creature (Entry: %u) without "
                        "trainer flag, ignore",
                        tableName, entry);
                    skip_trainers.insert(entry);
                }
                continue;
            }

            if (TrainerSpellData const* tSpells =
                    cInfo->trainerId ?
                        GetNpcTrainerTemplateSpells(cInfo->trainerId) :
                        nullptr)
            {
                if (tSpells->spellList.find(spell) != tSpells->spellList.end())
                {
                    logging.error(
                        "Table `%s` (Entry: %u) has spell %u listed in trainer "
                        "template %u, ignore",
                        tableName, entry, spell, cInfo->trainerId);
                    continue;
                }
            }
        }

        TrainerSpellData& data = trainerList[entry];

        TrainerSpell& trainerSpell = data.spellList[spell];
        trainerSpell.spell = spell;
        trainerSpell.spellCost = fields[2].GetUInt32();
        trainerSpell.reqSkill = fields[3].GetUInt32();
        trainerSpell.reqSkillValue = fields[4].GetUInt32();
        trainerSpell.reqLevel = fields[5].GetUInt32();

        trainerSpell.isProvidedReqLevel = trainerSpell.reqLevel > 0;

        if (!trainerSpell.reqLevel)
            trainerSpell.reqLevel = spellinfo->spellLevel;

        if (SpellMgr::IsProfessionSpell(spell))
            data.trainerType = 2;

        ++count;

    } while (result->NextRow());

    logging.info(
        "Loaded %d trainer %sspells\n", count, isTemplates ? "template " : "");
}

void ObjectMgr::LoadTrainerTemplates()
{
    LoadTrainers("npc_trainer_template", true);

    // post loading check
    std::set<uint32> trainer_ids;

    for (CacheTrainerSpellMap::const_iterator tItr =
             m_mCacheTrainerTemplateSpellMap.begin();
         tItr != m_mCacheTrainerTemplateSpellMap.end(); ++tItr)
        trainer_ids.insert(tItr->first);

    for (uint32 i = 1; i < sCreatureStorage.MaxEntry; ++i)
    {
        if (CreatureInfo const* cInfo =
                sCreatureStorage.LookupEntry<CreatureInfo>(i))
        {
            if (cInfo->trainerId)
            {
                if (m_mCacheTrainerTemplateSpellMap.find(cInfo->trainerId) !=
                    m_mCacheTrainerTemplateSpellMap.end())
                    trainer_ids.erase(cInfo->trainerId);
                else
                    logging.error(
                        "Creature (Entry: %u) has trainer_id = %u for "
                        "nonexistent trainer template",
                        cInfo->Entry, cInfo->trainerId);
            }
        }
    }

    for (const auto& trainer_id : trainer_ids)
        logging.error(
            "Table `npc_trainer_template` has trainer template %u not used by "
            "any trainers ",
            trainer_id);
}

void ObjectMgr::LoadVendors(char const* tableName, bool isTemplates)
{
    CacheVendorItemMap& vendorList =
        isTemplates ? m_mCacheVendorTemplateItemMap : m_mCacheVendorItemMap;

    // For reload case
    for (auto& elem : vendorList)
        elem.second.Clear();
    vendorList.clear();

    std::set<uint32> skip_vendors;

    std::unique_ptr<QueryResult> result(WorldDatabase.PQuery(
        "SELECT entry, item, maxcount, incrtime, ExtendedCost, weight FROM %s",
        tableName));
    if (!result)
    {
        logging.info("Loaded `%s`, table is empty!\n", tableName);
        return;
    }

    BarGoLink bar(result->GetRowCount());

    uint32 count = 0;
    do
    {
        bar.step();
        Field* fields = result->Fetch();

        uint32 entry = fields[0].GetUInt32();
        uint32 item_id = fields[1].GetUInt32();
        uint32 maxcount = fields[2].GetUInt32();
        uint32 incrtime = fields[3].GetUInt32();
        uint32 ExtendedCost = fields[4].GetUInt32();
        uint32 weight = fields[5].GetInt32();

        if (!IsVendorItemValid(isTemplates, tableName, entry, item_id, maxcount,
                incrtime, ExtendedCost, nullptr, &skip_vendors))
            continue;

        VendorItemData& vList = vendorList[entry];

        vList.AddItem(item_id, maxcount, incrtime, ExtendedCost, weight);
        ++count;

    } while (result->NextRow());

    logging.info(
        "Loaded %u vendor %sitems\n", count, isTemplates ? "template " : "");
}

void ObjectMgr::LoadVendorTemplates()
{
    LoadVendors("npc_vendor_template", true);

    // post loading check
    std::set<uint32> vendor_ids;

    for (CacheVendorItemMap::const_iterator vItr =
             m_mCacheVendorTemplateItemMap.begin();
         vItr != m_mCacheVendorTemplateItemMap.end(); ++vItr)
        vendor_ids.insert(vItr->first);

    for (uint32 i = 1; i < sCreatureStorage.MaxEntry; ++i)
    {
        if (CreatureInfo const* cInfo =
                sCreatureStorage.LookupEntry<CreatureInfo>(i))
        {
            if (cInfo->vendorId)
            {
                if (m_mCacheVendorTemplateItemMap.find(cInfo->vendorId) !=
                    m_mCacheVendorTemplateItemMap.end())
                    vendor_ids.erase(cInfo->vendorId);
                else
                    logging.error(
                        "Creature (Entry: %u) has vendor_id = %u for "
                        "nonexistent vendor template",
                        cInfo->Entry, cInfo->vendorId);
            }
        }
    }

    for (const auto& vendor_id : vendor_ids)
        logging.error(
            "Table `npc_vendor_template` has vendor template %u not used by "
            "any vendors ",
            vendor_id);
}

void ObjectMgr::LoadNpcGossips()
{
    m_mCacheNpcTextIdMap.clear();

    QueryResult* result =
        WorldDatabase.Query("SELECT npc_guid, textid FROM npc_gossip");
    if (!result)
    {
        logging.error("Loaded `npc_gossip`, table is empty!\n");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    uint32 count = 0;
    uint32 guid, textid;
    do
    {
        bar.step();

        Field* fields = result->Fetch();

        guid = fields[0].GetUInt32();
        textid = fields[1].GetUInt32();

        if (!GetCreatureData(guid))
        {
            logging.error(
                "Table `npc_gossip` have nonexistent creature (GUID: %u) "
                "entry, ignore. ",
                guid);
            continue;
        }
        if (!GetGossipText(textid))
        {
            logging.error(
                "Table `npc_gossip` for creature (GUID: %u) have wrong Textid "
                "(%u), ignore. ",
                guid, textid);
            continue;
        }

        m_mCacheNpcTextIdMap[guid] = textid;
        ++count;

    } while (result->NextRow());
    delete result;

    logging.info("Loaded %d NpcTextId \n", count);
}

void ObjectMgr::LoadGossipMenu(std::set<uint32>& gossipScriptSet)
{
    m_mGossipMenusMap.clear();
    //                                                0      1        2
    QueryResult* result = WorldDatabase.Query(
        "SELECT entry, text_id, script_id, "
        //   3       4             5             6       7             8
        //   9              10
        "cond_1, cond_1_val_1, cond_1_val_2, cond_2, cond_2_val_1, "
        "cond_2_val_2, condition_id, ordering FROM gossip_menu");

    if (!result)
    {
        logging.error("Loaded gossip_menu, table is empty!\n");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    uint32 count = 0;

    do
    {
        bar.step();

        Field* fields = result->Fetch();

        GossipMenus gMenu;

        gMenu.entry = fields[0].GetUInt32();
        gMenu.text_id = fields[1].GetUInt32();
        gMenu.script_id = fields[2].GetUInt32();

        ConditionType cond_1 = (ConditionType)fields[3].GetUInt32();
        uint32 cond_1_val_1 = fields[4].GetUInt32();
        uint32 cond_1_val_2 = fields[5].GetUInt32();
        ConditionType cond_2 = (ConditionType)fields[6].GetUInt32();
        uint32 cond_2_val_1 = fields[7].GetUInt32();
        uint32 cond_2_val_2 = fields[8].GetUInt32();

        gMenu.conditionId = fields[9].GetUInt16();

        gMenu.ordering = fields[10].GetUInt32();

        if (!GetGossipText(gMenu.text_id))
        {
            logging.error(
                "Table gossip_menu entry %u are using non-existing text_id %u",
                gMenu.entry, gMenu.text_id);
            continue;
        }

        // Check script-id
        if (gMenu.script_id)
        {
            if (sGossipScripts.second.find(gMenu.script_id) ==
                sGossipScripts.second.end())
            {
                logging.error(
                    "Table gossip_menu for menu %u, text-id %u have script_id "
                    "%u that does not exist in `gossip_scripts`, ignoring",
                    gMenu.entry, gMenu.text_id, gMenu.script_id);
                continue;
            }

            // Remove used script id
            gossipScriptSet.erase(gMenu.script_id);
        }

        if (!PlayerCondition::IsValid(0, cond_1, cond_1_val_1, cond_1_val_2))
        {
            logging.error(
                "Table gossip_menu entry %u, invalid condition 1 for id %u",
                gMenu.entry, gMenu.text_id);
            continue;
        }

        if (!PlayerCondition::IsValid(0, cond_2, cond_2_val_1, cond_2_val_2))
        {
            logging.error(
                "Table gossip_menu entry %u, invalid condition 2 for id %u",
                gMenu.entry, gMenu.text_id);
            continue;
        }

        gMenu.cond_1 = GetConditionId(cond_1, cond_1_val_1, cond_1_val_2);
        gMenu.cond_2 = GetConditionId(cond_2, cond_2_val_1, cond_2_val_2);

        if (gMenu.conditionId)
        {
            const PlayerCondition* condition =
                sConditionStorage.LookupEntry<PlayerCondition>(
                    gMenu.conditionId);
            if (!condition)
            {
                logging.error(
                    "Table gossip_menu for menu %u, text-id %u has "
                    "condition_id %u that does not exist in `conditions`, "
                    "ignoring",
                    gMenu.entry, gMenu.text_id, gMenu.conditionId);
                gMenu.conditionId = 0;
            }
        }

        m_mGossipMenusMap.insert(
            GossipMenusMap::value_type(gMenu.entry, gMenu));

        ++count;
    } while (result->NextRow());

    delete result;

    logging.info("Loaded %u gossip_menu entries\n", count);

    // post loading tests
    for (uint32 i = 1; i < sCreatureStorage.MaxEntry; ++i)
        if (CreatureInfo const* cInfo =
                sCreatureStorage.LookupEntry<CreatureInfo>(i))
            if (cInfo->GossipMenuId)
                if (m_mGossipMenusMap.find(cInfo->GossipMenuId) ==
                    m_mGossipMenusMap.end())
                    logging.error(
                        "Creature (Entry: %u) has gossip_menu_id = %u for "
                        "nonexistent menu",
                        cInfo->Entry, cInfo->GossipMenuId);

    for (uint32 i = 1; i < sGOStorage.MaxEntry; ++i)
        if (GameObjectInfo const* gInfo =
                sGOStorage.LookupEntry<GameObjectInfo>(i))
            if (uint32 menuid = gInfo->GetGossipMenuId())
                if (m_mGossipMenusMap.find(menuid) == m_mGossipMenusMap.end())
                    logging.warning(
                        "Gameobject (Entry: %u) has gossip_menu_id = %u for "
                        "nonexistent menu",
                        gInfo->id, menuid);
}

void ObjectMgr::LoadGossipMenuItems(std::set<uint32>& gossipScriptSet)
{
    m_mGossipMenuItemsMap.clear();

    QueryResult* result = WorldDatabase.Query(
        "SELECT menu_id, id, option_icon, option_text, option_id, "
        "npc_option_npcflag, "
        "action_menu_id, action_poi_id, action_script_id, box_coded, "
        "box_money, box_text, "
        "cond_1, cond_1_val_1, cond_1_val_2, "
        "cond_2, cond_2_val_1, cond_2_val_2, "
        "cond_3, cond_3_val_1, cond_3_val_2, condition_id "
        "FROM gossip_menu_option");

    if (!result)
    {
        logging.error("Loaded gossip_menu_option, table is empty!\n");
        return;
    }

    // prepare data for unused menu ids
    std::set<uint32> menu_ids; // for later integrity check
    for (GossipMenusMap::const_iterator itr = m_mGossipMenusMap.begin();
         itr != m_mGossipMenusMap.end(); ++itr)
        if (itr->first)
            menu_ids.insert(itr->first);

    for (uint32 i = 1; i < sGOStorage.MaxEntry; ++i)
        if (GameObjectInfo const* gInfo =
                sGOStorage.LookupEntry<GameObjectInfo>(i))
            if (uint32 menuid = gInfo->GetGossipMenuId())
                menu_ids.erase(menuid);

    // loading
    BarGoLink bar(result->GetRowCount());

    uint32 count = 0;

    // prepare menuid -> CreatureInfo map for fast access
    typedef std::multimap<uint32, const CreatureInfo*> Menu2CInfoMap;
    Menu2CInfoMap menu2CInfoMap;
    for (uint32 i = 1; i < sCreatureStorage.MaxEntry; ++i)
        if (CreatureInfo const* cInfo =
                sCreatureStorage.LookupEntry<CreatureInfo>(i))
            if (cInfo->GossipMenuId)
                menu2CInfoMap.insert(
                    Menu2CInfoMap::value_type(cInfo->GossipMenuId, cInfo));

    do
    {
        bar.step();

        Field* fields = result->Fetch();

        GossipMenuItems gMenuItem;

        gMenuItem.menu_id = fields[0].GetUInt32();
        gMenuItem.id = fields[1].GetUInt32();
        gMenuItem.option_icon = fields[2].GetUInt8();
        gMenuItem.option_text = fields[3].GetCppString();
        gMenuItem.option_id = fields[4].GetUInt32();
        gMenuItem.npc_option_npcflag = fields[5].GetUInt32();
        gMenuItem.action_menu_id = fields[6].GetInt32();
        gMenuItem.action_poi_id = fields[7].GetUInt32();
        gMenuItem.action_script_id = fields[8].GetUInt32();
        gMenuItem.box_coded = fields[9].GetUInt8() != 0;
        gMenuItem.box_money = fields[10].GetUInt32();
        gMenuItem.box_text = fields[11].GetCppString();

        ConditionType cond_1 = (ConditionType)fields[12].GetUInt32();
        uint32 cond_1_val_1 = fields[13].GetUInt32();
        uint32 cond_1_val_2 = fields[14].GetUInt32();
        ConditionType cond_2 = (ConditionType)fields[15].GetUInt32();
        uint32 cond_2_val_1 = fields[16].GetUInt32();
        uint32 cond_2_val_2 = fields[17].GetUInt32();
        ConditionType cond_3 = (ConditionType)fields[18].GetUInt32();
        uint32 cond_3_val_1 = fields[19].GetUInt32();
        uint32 cond_3_val_2 = fields[20].GetUInt32();
        gMenuItem.conditionId = fields[21].GetUInt16();

        if (gMenuItem.menu_id) // == 0 id is special and not have menu_id data
        {
            if (m_mGossipMenusMap.find(gMenuItem.menu_id) ==
                m_mGossipMenusMap.end())
            {
                logging.error(
                    "Gossip menu option (MenuId: %u) for nonexistent menu",
                    gMenuItem.menu_id);
                continue;
            }
        }

        if (!PlayerCondition::IsValid(0, cond_1, cond_1_val_1, cond_1_val_2))
        {
            logging.error(
                "Table gossip_menu_option menu %u, invalid condition 1 for id "
                "%u",
                gMenuItem.menu_id, gMenuItem.id);
            continue;
        }
        if (!PlayerCondition::IsValid(0, cond_2, cond_2_val_1, cond_2_val_2))
        {
            logging.error(
                "Table gossip_menu_option menu %u, invalid condition 2 for id "
                "%u",
                gMenuItem.menu_id, gMenuItem.id);
            continue;
        }
        if (!PlayerCondition::IsValid(0, cond_3, cond_3_val_1, cond_3_val_2))
        {
            logging.error(
                "Table gossip_menu_option menu %u, invalid condition 3 for id "
                "%u",
                gMenuItem.menu_id, gMenuItem.id);
            continue;
        }

        if (gMenuItem.action_menu_id > 0)
        {
            if (m_mGossipMenusMap.find(gMenuItem.action_menu_id) ==
                m_mGossipMenusMap.end())
                logging.error(
                    "Gossip menu option (MenuId: %u Id: %u) have "
                    "action_menu_id = %u for nonexistent menu",
                    gMenuItem.menu_id, gMenuItem.id, gMenuItem.action_menu_id);
            else
                menu_ids.erase(gMenuItem.action_menu_id);
        }

        if (gMenuItem.option_icon >= GOSSIP_ICON_MAX)
        {
            logging.error(
                "Table gossip_menu_option for menu %u, id %u has unknown icon "
                "id %u. Replacing with GOSSIP_ICON_CHAT",
                gMenuItem.menu_id, gMenuItem.id, gMenuItem.option_icon);
            gMenuItem.option_icon = GOSSIP_ICON_CHAT;
        }

        if (gMenuItem.option_id == GOSSIP_OPTION_NONE)
            logging.error(
                "Table gossip_menu_option for menu %u, id %u use option id "
                "GOSSIP_OPTION_NONE. Option will never be used",
                gMenuItem.menu_id, gMenuItem.id);

        if (gMenuItem.option_id >= GOSSIP_OPTION_MAX)
            logging.error(
                "Table gossip_menu_option for menu %u, id %u has unknown "
                "option id %u. Option will not be used",
                gMenuItem.menu_id, gMenuItem.id, gMenuItem.option_id);

        if (gMenuItem.menu_id && gMenuItem.npc_option_npcflag)

        {
            bool found_menu_uses = false;
            bool found_flags_uses = false;

            std::pair<Menu2CInfoMap::const_iterator,
                Menu2CInfoMap::const_iterator> tm_bounds =
                menu2CInfoMap.equal_range(gMenuItem.menu_id);
            for (auto it2 = tm_bounds.first;
                 !found_flags_uses && it2 != tm_bounds.second; ++it2)
            {
                CreatureInfo const* cInfo = it2->second;

                found_menu_uses = true;

                // some from creatures with gossip menu can use gossip option
                // base at npc_flags
                if (gMenuItem.npc_option_npcflag & cInfo->npcflag)
                    found_flags_uses = true;

                menu_ids.erase(gMenuItem.menu_id);
            }

            if (found_menu_uses && !found_flags_uses)
                logging.error(
                    "Table gossip_menu_option for menu %u, id %u has "
                    "`npc_option_npcflag` = %u but creatures using this menu "
                    "does not have corresponding`npcflag`. Option will not "
                    "accessible in game.",
                    gMenuItem.menu_id, gMenuItem.id,
                    gMenuItem.npc_option_npcflag);
        }

        if (gMenuItem.action_poi_id &&
            !GetPointOfInterest(gMenuItem.action_poi_id))
        {
            logging.error(
                "Table gossip_menu_option for menu %u, id %u use non-existing "
                "action_poi_id %u, ignoring",
                gMenuItem.menu_id, gMenuItem.id, gMenuItem.action_poi_id);
            gMenuItem.action_poi_id = 0;
        }

        if (gMenuItem.action_script_id)
        {
            if (sGossipScripts.second.find(gMenuItem.action_script_id) ==
                sGossipScripts.second.end())
            {
                logging.error(
                    "Table gossip_menu_option for menu %u, id %u have "
                    "action_script_id %u that does not exist in "
                    "`gossip_scripts`, ignoring",
                    gMenuItem.menu_id, gMenuItem.id,
                    gMenuItem.action_script_id);
                continue;
            }

            // Remove used script id
            gossipScriptSet.erase(gMenuItem.action_script_id);
        }

        gMenuItem.cond_1 = GetConditionId(cond_1, cond_1_val_1, cond_1_val_2);
        gMenuItem.cond_2 = GetConditionId(cond_2, cond_2_val_1, cond_2_val_2);
        gMenuItem.cond_3 = GetConditionId(cond_3, cond_3_val_1, cond_3_val_2);

        if (gMenuItem.conditionId)
        {
            const PlayerCondition* condition =
                sConditionStorage.LookupEntry<PlayerCondition>(
                    gMenuItem.conditionId);
            if (!condition)
            {
                logging.error(
                    "Table gossip_menu_option for menu %u, id %u has "
                    "condition_id %u that does not exist in `conditions`, "
                    "ignoring",
                    gMenuItem.menu_id, gMenuItem.id, gMenuItem.conditionId);
                gMenuItem.conditionId = 0;
            }
        }

        m_mGossipMenuItemsMap.insert(
            GossipMenuItemsMap::value_type(gMenuItem.menu_id, gMenuItem));

        ++count;

    } while (result->NextRow());

    delete result;

    for (const auto& menu_id : menu_ids)
        logging.error(
            "Table `gossip_menu` contain unused (in creature or GO or menu "
            "options) menu id %u.",
            menu_id);

    logging.info("Loaded %u gossip_menu_option entries\n", count);
}

void ObjectMgr::LoadGossipMenus()
{
    // Check which script-ids in gossip_scripts are not used
    std::set<uint32> gossipScriptSet;
    for (ScriptMapMap::const_iterator itr = sGossipScripts.second.begin();
         itr != sGossipScripts.second.end(); ++itr)
        gossipScriptSet.insert(itr->first);

    // Load gossip_menu and gossip_menu_option data
    logging.info("(Re)Loading Gossip menus...");
    LoadGossipMenu(gossipScriptSet);
    logging.info("(Re)Loading Gossip menu options...");
    LoadGossipMenuItems(gossipScriptSet);

    for (const auto& elem : gossipScriptSet)
        logging.error(
            "Table `gossip_scripts` contains unused script, id %u.", elem);
}

void ObjectMgr::AddVendorItem(uint32 entry, uint32 item, uint32 maxcount,
    uint32 incrtime, uint32 extendedcost, uint32 weight)
{
    VendorItemData& vList = m_mCacheVendorItemMap[entry];
    vList.AddItem(item, maxcount, incrtime, extendedcost, weight);

    WorldDatabase.PExecuteLog(
        "INSERT INTO npc_vendor "
        "(entry,item,maxcount,incrtime,extendedcost,weight) "
        "VALUES('%u','%u','%u','%u','%u','%u')",
        entry, item, maxcount, incrtime, extendedcost, weight);
}

bool ObjectMgr::RemoveVendorItem(uint32 entry, uint32 item)
{
    auto iter = m_mCacheVendorItemMap.find(entry);
    if (iter == m_mCacheVendorItemMap.end())
        return false;

    if (!iter->second.RemoveItem(item))
        return false;

    WorldDatabase.PExecuteLog(
        "DELETE FROM npc_vendor WHERE entry='%u' AND item='%u'", entry, item);
    return true;
}

bool ObjectMgr::IsVendorItemValid(bool isTemplate, char const* tableName,
    uint32 vendor_entry, uint32 item_id, uint32 maxcount, uint32 incrtime,
    uint32 ExtendedCost, Player* pl, std::set<uint32>* skip_vendors) const
{
    char const* idStr = isTemplate ? "vendor template" : "vendor";
    CreatureInfo const* cInfo = nullptr;

    if (!isTemplate)
    {
        cInfo = GetCreatureTemplate(vendor_entry);
        if (!cInfo)
        {
            if (pl)
                ChatHandler(pl).SendSysMessage(LANG_COMMAND_VENDORSELECTION);
            else
                logging.error(
                    "Table `%s` has data for nonexistent creature (Entry: %u), "
                    "ignoring",
                    tableName, vendor_entry);
            return false;
        }

        if (!(cInfo->npcflag & UNIT_NPC_FLAG_VENDOR))
        {
            if (!skip_vendors || skip_vendors->count(vendor_entry) == 0)
            {
                if (pl)
                    ChatHandler(pl).SendSysMessage(
                        LANG_COMMAND_VENDORSELECTION);
                else
                    logging.error(
                        "Table `%s` has data for creature (Entry: %u) without "
                        "vendor flag, ignoring",
                        tableName, vendor_entry);

                if (skip_vendors)
                    skip_vendors->insert(vendor_entry);
            }
            return false;
        }
    }

    if (!GetItemPrototype(item_id))
    {
        if (pl)
            ChatHandler(pl).PSendSysMessage(LANG_ITEM_NOT_FOUND, item_id);
        else
            logging.error(
                "Table `%s` for %s %u contain nonexistent item (%u), ignoring",
                tableName, idStr, vendor_entry, item_id);
        return false;
    }

    if (ExtendedCost && !sItemExtendedCostStore.LookupEntry(ExtendedCost) &&
        !GetExtendedCostOverride(ExtendedCost))
    {
        if (pl)
            ChatHandler(pl).PSendSysMessage(
                LANG_EXTENDED_COST_NOT_EXIST, ExtendedCost);
        else
            logging.error(
                "Table `%s` contain item (Entry: %u) with wrong ExtendedCost "
                "(%u) for %s %u, ignoring",
                tableName, item_id, ExtendedCost, idStr, vendor_entry);
        return false;
    }

    if (maxcount > 0 && incrtime == 0)
    {
        if (pl)
            ChatHandler(pl).PSendSysMessage(
                "MaxCount!=0 (%u) but IncrTime==0", maxcount);
        else
            logging.error(
                "Table `%s` has `maxcount` (%u) for item %u of %s %u but "
                "`incrtime`=0, ignoring",
                tableName, maxcount, item_id, idStr, vendor_entry);
        return false;
    }
    else if (maxcount == 0 && incrtime > 0)
    {
        if (pl)
            ChatHandler(pl).PSendSysMessage("MaxCount==0 but IncrTime<>=0");
        else
            logging.error(
                "Table `%s` has `maxcount`=0 for item %u of %s %u but "
                "`incrtime`<>0, ignoring",
                tableName, item_id, idStr, vendor_entry);
        return false;
    }

    VendorItemData const* vItems =
        isTemplate ? GetNpcVendorTemplateItemList(vendor_entry) :
                     GetNpcVendorItemList(vendor_entry);
    VendorItemData const* tItems =
        isTemplate ? nullptr : GetNpcVendorTemplateItemList(vendor_entry);

    if (!vItems && !tItems)
        return true; // later checks for non-empty lists

    if (vItems && vItems->FindItem(item_id))
    {
        if (pl)
            ChatHandler(pl).PSendSysMessage(LANG_ITEM_ALREADY_IN_LIST, item_id);
        else
            logging.error(
                "Table `%s` has duplicate items %u for %s %u, ignoring",
                tableName, item_id, idStr, vendor_entry);
        return false;
    }

    if (!isTemplate)
    {
        if (tItems && tItems->GetItem(item_id))
        {
            if (pl)
                ChatHandler(pl).PSendSysMessage(
                    LANG_ITEM_ALREADY_IN_LIST, item_id);
            else
            {
                if (!cInfo->vendorId)
                    logging.error(
                        "Table `%s` has duplicate items %u for %s %u, ignoring",
                        tableName, item_id, idStr, vendor_entry);
                else
                    logging.error(
                        "Table `%s` has duplicate items %u for %s %u (or "
                        "possible in vendor template %u), ignoring",
                        tableName, item_id, idStr, vendor_entry,
                        cInfo->vendorId);
            }
            return false;
        }
    }

    uint32 countItems = vItems ? vItems->GetItemCount() : 0;
    countItems += tItems ? tItems->GetItemCount() : 0;

    if (countItems >= MAX_VENDOR_ITEMS)
    {
        if (pl)
            ChatHandler(pl).SendSysMessage(LANG_COMMAND_ADDVENDORITEMITEMS);
        else
            logging.error(
                "Table `%s` has too many items (%u >= %i) for %s %u, ignoring",
                tableName, countItems, MAX_VENDOR_ITEMS, idStr, vendor_entry);
        return false;
    }

    return true;
}

void ObjectMgr::AddGroup(Group* group)
{
    mGroupMap[group->GetId()] = group;
}

void ObjectMgr::RemoveGroup(Group* group)
{
    mGroupMap.erase(group->GetId());
}

void ObjectMgr::AddOfflineLeaderGroup(Group* group)
{
    mOfflineLeaderGroups.emplace_back(
        group->GetId(), WorldTimer::time_no_syscall() + 180);
}

bool ObjectMgr::IsOfflineLeaderGroup(Group* group) const
{
    for (auto& pair : mOfflineLeaderGroups)
        if (group->GetId() == pair.first)
            return true;
    return false;
}

void ObjectMgr::UpdateGroupsWithOfflineLeader()
{
    while (!mOfflineLeaderGroups.empty() &&
           mOfflineLeaderGroups[0].second < WorldTimer::time_no_syscall())
    {
        uint32 id = mOfflineLeaderGroups[0].first;
        mOfflineLeaderGroups.erase(mOfflineLeaderGroups.begin());

        auto group = GetGroupById(id);
        if (!group)
            continue;

        auto guid = group->GetLeaderGuid();
        if (GetPlayer(guid, false) != nullptr)
            continue;

        group->PassLeaderOnward();
    }
}

namespace
{
bool arena_rating_cmp(ArenaTeam* a, ArenaTeam* b)
{
    return a->GetRating() < b->GetRating();
}
}

std::vector<ArenaTeam*>::const_iterator ObjectMgr::get_arena_team_rank_itr(
    ArenaTeam* team) const
{
    auto range = std::equal_range(
        arena_rank_map_.begin(), arena_rank_map_.end(), team, arena_rating_cmp);

    for (auto itr = range.first; itr != range.second; ++itr)
        if (*itr == team)
            return itr;

    return arena_rank_map_.end();
}

void ObjectMgr::AddArenaTeam(ArenaTeam* team)
{
    mArenaTeamMap[team->GetId()] = team;

    auto itr = std::lower_bound(
        arena_rank_map_.begin(), arena_rank_map_.end(), team, arena_rating_cmp);
    assert(itr == arena_rank_map_.end() || *itr != team);

    arena_rank_map_.insert(itr, team);
}

void ObjectMgr::RemoveArenaTeam(ArenaTeam* team)
{
    mArenaTeamMap.erase(team->GetId());

    auto itr = get_arena_team_rank_itr(team);
    assert(itr != arena_rank_map_.end());
    arena_rank_map_.erase(itr);
}

uint32 ObjectMgr::get_arena_team_rank(ArenaTeam* team) const
{
    auto itr = get_arena_team_rank_itr(team);
    assert(itr != arena_rank_map_.end());
    return arena_rank_map_.end() - itr;
}

void ObjectMgr::update_arena_rankings()
{
    std::sort(arena_rank_map_.begin(), arena_rank_map_.end(), arena_rating_cmp);
}

void ObjectMgr::GetCreatureLocaleStrings(uint32 entry, int32 loc_idx,
    char const** namePtr, char const** subnamePtr) const
{
    if (loc_idx >= 0)
    {
        if (CreatureLocale const* il = GetCreatureLocale(entry))
        {
            if (namePtr && il->Name.size() > size_t(loc_idx) &&
                !il->Name[loc_idx].empty())
                *namePtr = il->Name[loc_idx].c_str();

            if (subnamePtr && il->SubName.size() > size_t(loc_idx) &&
                !il->SubName[loc_idx].empty())
                *subnamePtr = il->SubName[loc_idx].c_str();
        }
    }
}

void ObjectMgr::GetItemLocaleStrings(uint32 entry, int32 loc_idx,
    std::string* namePtr, std::string* descriptionPtr) const
{
    if (loc_idx >= 0)
    {
        if (ItemLocale const* il = GetItemLocale(entry))
        {
            if (namePtr && il->Name.size() > size_t(loc_idx) &&
                !il->Name[loc_idx].empty())
                *namePtr = il->Name[loc_idx];

            if (descriptionPtr && il->Description.size() > size_t(loc_idx) &&
                !il->Description[loc_idx].empty())
                *descriptionPtr = il->Description[loc_idx];
        }
    }
}

void ObjectMgr::GetQuestLocaleStrings(
    uint32 entry, int32 loc_idx, std::string* titlePtr) const
{
    if (loc_idx >= 0)
    {
        if (QuestLocale const* il = GetQuestLocale(entry))
        {
            if (titlePtr && il->Title.size() > size_t(loc_idx) &&
                !il->Title[loc_idx].empty())
                *titlePtr = il->Title[loc_idx];
        }
    }
}

void ObjectMgr::GetNpcTextLocaleStringsAll(uint32 entry, int32 loc_idx,
    ObjectMgr::NpcTextArray* text0_Ptr,
    ObjectMgr::NpcTextArray* text1_Ptr) const
{
    if (loc_idx >= 0)
    {
        if (NpcTextLocale const* nl = GetNpcTextLocale(entry))
        {
            if (text0_Ptr)
                for (int i = 0; i < MAX_GOSSIP_TEXT_OPTIONS; ++i)
                    if (nl->Text_0[i].size() > (size_t)loc_idx &&
                        !nl->Text_0[i][loc_idx].empty())
                        (*text0_Ptr)[i] = nl->Text_0[i][loc_idx];

            if (text1_Ptr)
                for (int i = 0; i < MAX_GOSSIP_TEXT_OPTIONS; ++i)
                    if (nl->Text_1[i].size() > (size_t)loc_idx &&
                        !nl->Text_1[i][loc_idx].empty())
                        (*text1_Ptr)[i] = nl->Text_1[i][loc_idx];
        }
    }
}

void ObjectMgr::GetNpcTextLocaleStrings0(uint32 entry, int32 loc_idx,
    std::string* text0_0_Ptr, std::string* text1_0_Ptr) const
{
    if (loc_idx >= 0)
    {
        if (NpcTextLocale const* nl = GetNpcTextLocale(entry))
        {
            if (text0_0_Ptr)
                if (nl->Text_0[0].size() > (size_t)loc_idx &&
                    !nl->Text_0[0][loc_idx].empty())
                    *text0_0_Ptr = nl->Text_0[0][loc_idx];

            if (text1_0_Ptr)
                if (nl->Text_1[0].size() > (size_t)loc_idx &&
                    !nl->Text_1[0][loc_idx].empty())
                    *text1_0_Ptr = nl->Text_1[0][loc_idx];
        }
    }
}

// Functions for scripting access
bool LoadMangosStrings(
    DatabaseType& db, char const* table, int32 start_value, int32 end_value)
{
    // MAX_DB_SCRIPT_STRING_ID is max allowed negative value for scripts (scrpts
    // can use only more deep negative values
    // start/end reversed for negative values
    if (start_value > MAX_DB_SCRIPT_STRING_ID || end_value >= start_value)
    {
        logging.error(
            "Table '%s' attempt loaded with reserved by mangos range (%d - "
            "%d), strings not loaded.",
            table, start_value, end_value + 1);
        return false;
    }

    return sObjectMgr::Instance()->LoadMangosStrings(
        db, table, start_value, end_value);
}

CreatureInfo const* GetCreatureTemplateStore(uint32 entry)
{
    return sCreatureStorage.LookupEntry<CreatureInfo>(entry);
}

Quest const* GetQuestTemplateStore(uint32 entry)
{
    return sObjectMgr::Instance()->GetQuestTemplate(entry);
}

bool FindCreatureData::operator()(CreatureDataPair const& dataPair)
{
    // skip wrong entry ids
    if (i_id && dataPair.second.id != i_id)
        return false;

    if (!i_anyData)
        i_anyData = &dataPair;

    // without player we can't find more stricted cases, so use fouded
    if (!i_player)
        return true;

    // skip diff. map cases
    if (dataPair.second.mapid != i_player->GetMapId())
        return false;

    float new_dist =
        i_player->GetDistance2d(dataPair.second.posX, dataPair.second.posY);

    if (!i_mapData || new_dist < i_mapDist)
    {
        i_mapData = &dataPair;
        i_mapDist = new_dist;
    }

    // skip not spawned (in any state),
    auto pool_id =
        sPoolMgr::Instance()->IsPartOfAPool<Creature>(dataPair.first);
    if (pool_id && (!i_player->GetMap()->GetPersistentState() ||
                       !i_player->GetMap()
                            ->GetPersistentState()
                            ->IsSpawnedPoolObject<Creature>(dataPair.first)))
        return false;

    if (!i_spawnedData || new_dist < i_spawnedDist)
    {
        i_spawnedData = &dataPair;
        i_spawnedDist = new_dist;
    }

    return false;
}

CreatureDataPair const* FindCreatureData::GetResult() const
{
    if (i_spawnedData)
        return i_spawnedData;

    if (i_mapData)
        return i_mapData;

    return i_anyData;
}

bool FindGOData::operator()(GameObjectDataPair const& dataPair)
{
    // skip wrong entry ids
    if (i_id && dataPair.second.id != i_id)
        return false;

    if (!i_anyData)
        i_anyData = &dataPair;

    // without player we can't find more stricted cases, so use fouded
    if (!i_player)
        return true;

    // skip diff. map cases
    if (dataPair.second.mapid != i_player->GetMapId())
        return false;

    float new_dist =
        i_player->GetDistance2d(dataPair.second.posX, dataPair.second.posY);

    if (!i_mapData || new_dist < i_mapDist)
    {
        i_mapData = &dataPair;
        i_mapDist = new_dist;
    }

    // skip not spawned (in any state)
    auto pool_id =
        sPoolMgr::Instance()->IsPartOfAPool<GameObject>(dataPair.first);
    if (pool_id && (!i_player->GetMap()->GetPersistentState() ||
                       !i_player->GetMap()
                            ->GetPersistentState()
                            ->IsSpawnedPoolObject<GameObject>(dataPair.first)))
        return false;

    if (!i_spawnedData || new_dist < i_spawnedDist)
    {
        i_spawnedData = &dataPair;
        i_spawnedDist = new_dist;
    }

    return false;
}

GameObjectDataPair const* FindGOData::GetResult() const
{
    if (i_mapData)
        return i_mapData;

    if (i_spawnedData)
        return i_spawnedData;

    return i_anyData;
}

void ObjectMgr::LoadNpcAggroLink()
{
    aggro_links_.clear();

    std::unique_ptr<QueryResult> result(WorldDatabase.PQuery(
        "SELECT boss_entry, boss_guid, defender_entry, defender_guid FROM "
        "npc_aggro_link"));
    if (!result)
    {
        logging.info("Loaded npc_aggro_link, table is empty!\n");
        return;
    }

    BarGoLink bar(result->GetRowCount());
    int count = 0;

    do
    {
        bar.step();
        Field* fields = result->Fetch();

        ObjectGuid boss(
            HIGHGUID_UNIT, fields[0].GetUInt32(), fields[1].GetUInt32());
        ObjectGuid defender(
            HIGHGUID_UNIT, fields[2].GetUInt32(), fields[3].GetUInt32());

        aggro_links_[boss].push_back(defender);

        ++count;

    } while (result->NextRow());

    logging.info("Loaded npc_aggro_link, %d entries!\n", count);
}

static std::string read_req_item_str(
    std::vector<MapEntryRequirements::req_entry>& out, const std::string& str)
{
    // valid entries of format:
    // a|h|b n|h itemid[ "failed text"]

    if (str.empty())
        return "";
    std::stringstream ss(str);

    while (ss)
    {
        MapEntryRequirements::req_entry entry;

        char c = 0;
        ss >> c;
        if (!ss || (c != 'a' && c != 'b' && c != 'h'))
            return "expected \"a|h|b\"; a = alliance, h = horde, b = both";

        switch (c)
        {
        case 'a':
            entry.team = ALLIANCE;
            break;
        case 'h':
            entry.team = HORDE;
            break;
        default:
            entry.team = TEAM_NONE;
            break;
        }

        c = 0;
        ss >> c;
        if (!ss || (c != 'n' && c != 'h'))
            return "expected \"n|h\"; n = normal, h = heroic";

        entry.difficulty =
            c == 'n' ? DUNGEON_DIFFICULTY_NORMAL : DUNGEON_DIFFICULTY_HEROIC;

        uint32 id;
        ss >> id;
        if (!ss)
            return "expected identifier";

        entry.id = id;

        c = 0;
        ss >> c;
        if (ss && c != '"')
            ss.unget();
        else if (c == '"')
        {
            std::string err;
            while ((c = ss.get()) != EOF && c != '"')
                err.push_back(c);
            if (c == EOF)
                return "expected \" to close error string";
            entry.failed_text = err;

            // check if there's more input after this entry
            ss >> c;
            if (ss)
                ss.unget();
        }

        out.push_back(std::move(entry));
    }

    return "";
}

void ObjectMgr::LoadMapEntryRequirements()
{
    map_entry_req_.clear();

    std::unique_ptr<QueryResult> result(WorldDatabase.PQuery(
        "SELECT map_id, required_level, required_items, required_quests FROM "
        "map_entry_requirements"));
    if (!result)
    {
        logging.info("Loaded map_entry_requirements, table is empty!\n");
        return;
    }

    BarGoLink bar(result->GetRowCount());
    int count = 0;

    do
    {
        bar.step();

        Field* fields = result->Fetch();
        MapEntryRequirements req;
        uint32 map = fields[0].GetUInt32();
        req.level = fields[1].GetUInt32();
        std::string items_str = fields[2].GetCppString();
        std::string quests_str = fields[3].GetCppString();

        if (!sMapStore.LookupEntry(map))
        {
            logging.error(
                "Table `map_entry_requirements` contains invalid entry with "
                "`map_id`=%u. Error: No such map.",
                map);
            continue;
        }

        auto err = read_req_item_str(req.items, items_str);
        if (!err.empty())
        {
            logging.error(
                "Table `map_entry_requirements` contains invalid entry with "
                "`map_id`=%u. Error in `required_items` syntax: \"%s\".",
                map, err.c_str());
            continue;
        }

        bool invalid = false;
        for (auto& item : req.items)
            if (!ObjectMgr::GetItemPrototype(item.id))
            {
                logging.error(
                    "Table `map_entry_requirements` contains invalid entry "
                    "with `map_id`=%u. Error in `required_items`: item with id "
                    "%u does not exist.",
                    map, item.id);
                invalid = true;
                break;
            }
        if (invalid)
            continue;

        err = read_req_item_str(req.quests, quests_str);
        if (!err.empty())
        {
            logging.error(
                "Table `map_entry_requirements` contains invalid entry with "
                "`map_id`=%u. Error in `required_quests` syntax: \"%s\".",
                map, err.c_str());
            continue;
        }

        for (auto& item : req.quests)
            if (!ObjectMgr::GetQuestTemplate(item.id))
            {
                logging.error(
                    "Table `map_entry_requirements` contains invalid entry "
                    "with `map_id`=%u. Error in `required_quests`: quest with "
                    "id %u does not exist.",
                    map, item.id);
                invalid = true;
                break;
            }
        if (invalid)
            continue;

        map_entry_req_[map] = req;
        ++count;

    } while (result->NextRow());

    logging.info("Loaded map_entry_requirements, %d entries!\n", count);
}

void ObjectMgr::LoadExtendedItemCost()
{
    extended_cost_.clear();

    std::unique_ptr<QueryResult> result(WorldDatabase.PQuery(
        "SELECT id, honor, arena, item0, item_count0, item1, item_count1, "
        "item2, item_count2, item3, item_count3, item4, item_count4, "
        "req_rating FROM item_extended_cost"));
    if (!result)
    {
        logging.info("Loaded item_extended_cost, table is empty!\n");
        return;
    }

    BarGoLink bar(result->GetRowCount());
    int count = 0;

    do
    {
        bar.step();

        Field* fields = result->Fetch();
        ItemExtendedCostEntry entry;
        entry.ID = fields[0].GetUInt32();
        entry.reqhonorpoints = fields[1].GetUInt32();
        entry.reqarenapoints = fields[2].GetUInt32();

        bool valid = true;
        for (int i = 0; i < 5; ++i)
        {
            entry.reqitem[i] = fields[3 + i * 2].GetUInt32();
            entry.reqitemcount[i] = fields[4 + i * 2].GetUInt32();
            if (entry.reqitem[i] &&
                sItemStorage.LookupEntry<ItemPrototype>(entry.reqitem[i]) ==
                    nullptr)
            {
                valid = false;
                logging.error(
                    "item_extended_cost %u ignored, invalid item in item_%u",
                    entry.ID, i);
            }
        }
        if (!valid)
            continue;

        entry.reqpersonalarenarating = fields[13].GetUInt32();

        extended_cost_[entry.ID] = entry;
    } while (result->NextRow());

    logging.info("Loaded item_extended_cost, %d entries!\n", count);
}
