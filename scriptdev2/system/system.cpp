/* Copyright (C) 2006 - 2012 ScriptDev2 <http://www.scriptdev2.com/>
* This program is free software licensed under GPL version 2
* Please see the included DOCS/LICENSE.TXT for more information */

#include "system.h"
#include "ObjectMgr.h"
#include "ProgressBar.h"
#include "precompiled.h"
#include "../config.h"

DatabaseType SD2Database;

SystemMgr::SystemMgr()
{
}

SystemMgr& SystemMgr::Instance()
{
    static SystemMgr pSysMgr;
    return pSysMgr;
}

void SystemMgr::LoadScriptTexts()
{
    logging.info("SD2: Loading Script Texts...");
    LoadMangosStrings(SD2Database, "script_texts", TEXT_SOURCE_TEXT_START,
        TEXT_SOURCE_TEXT_END);

    QueryResult* pResult = SD2Database.PQuery(
        "SELECT entry, sound, type, language, emote FROM script_texts WHERE "
        "entry BETWEEN %i AND %i",
        TEXT_SOURCE_GOSSIP_END, TEXT_SOURCE_TEXT_START);

    logging.info("SD2: Loading Script Texts additional data...");

    uint32 uiCount = 0;
    if (pResult)
    {
        BarGoLink bar(pResult->GetRowCount());

        do
        {
            bar.step();
            Field* pFields = pResult->Fetch();
            StringTextData pTemp;

            int32 iId = pFields[0].GetInt32();
            pTemp.uiSoundId = pFields[1].GetUInt32();
            pTemp.uiType = pFields[2].GetUInt32();
            pTemp.uiLanguage = pFields[3].GetUInt32();
            pTemp.uiEmote = pFields[4].GetUInt32();

            if (iId >= 0)
            {
                logging.error(
                    "SD2: Entry %i in table `script_texts` is not a negative "
                    "value.",
                    iId);
                continue;
            }

            if (pTemp.uiSoundId)
            {
                if (!GetSoundEntriesStore()->LookupEntry(pTemp.uiSoundId))
                    logging.error(
                        "SD2: Entry %i in table `script_texts` has soundId %u "
                        "but sound does not exist.",
                        iId, pTemp.uiSoundId);
            }

            if (!GetLanguageDescByID(pTemp.uiLanguage))
                logging.error(
                    "SD2: Entry %i in table `script_texts` using Language %u "
                    "but Language does not exist.",
                    iId, pTemp.uiLanguage);

            if (pTemp.uiType > CHAT_TYPE_ZONE_YELL)
                logging.error(
                    "SD2: Entry %i in table `script_texts` has Type %u but "
                    "this Chat Type does not exist.",
                    iId, pTemp.uiType);

            m_mTextDataMap[iId] = pTemp;
            ++uiCount;
        } while (pResult->NextRow());

        delete pResult;
    }
    logging.info("Loaded %u additional Script Texts data.\n", uiCount);
}

void SystemMgr::LoadScriptTextsCustom()
{
    logging.info("SD2: Loading Custom Texts...");
    LoadMangosStrings(SD2Database, "custom_texts", TEXT_SOURCE_CUSTOM_START,
        TEXT_SOURCE_CUSTOM_END);

    QueryResult* pResult = SD2Database.PQuery(
        "SELECT entry, sound, type, language, emote FROM custom_texts WHERE "
        "entry BETWEEN %i AND %i",
        TEXT_SOURCE_CUSTOM_END, TEXT_SOURCE_CUSTOM_START);

    logging.info("SD2: Loading Custom Texts additional data...");

    uint32 uiCount = 0;
    if (pResult)
    {
        BarGoLink bar(pResult->GetRowCount());

        do
        {
            bar.step();
            Field* pFields = pResult->Fetch();
            StringTextData pTemp;

            int32 iId = pFields[0].GetInt32();
            pTemp.uiSoundId = pFields[1].GetUInt32();
            pTemp.uiType = pFields[2].GetUInt32();
            pTemp.uiLanguage = pFields[3].GetUInt32();
            pTemp.uiEmote = pFields[4].GetUInt32();

            if (iId >= 0)
            {
                logging.error(
                    "SD2: Entry %i in table `custom_texts` is not a negative "
                    "value.",
                    iId);
                continue;
            }

            if (pTemp.uiSoundId)
            {
                if (!GetSoundEntriesStore()->LookupEntry(pTemp.uiSoundId))
                    logging.error(
                        "SD2: Entry %i in table `custom_texts` has soundId %u "
                        "but sound does not exist.",
                        iId, pTemp.uiSoundId);
            }

            if (!GetLanguageDescByID(pTemp.uiLanguage))
                logging.error(
                    "SD2: Entry %i in table `custom_texts` using Language %u "
                    "but Language does not exist.",
                    iId, pTemp.uiLanguage);

            if (pTemp.uiType > CHAT_TYPE_ZONE_YELL)
                logging.error(
                    "SD2: Entry %i in table `custom_texts` has Type %u but "
                    "this Chat Type does not exist.",
                    iId, pTemp.uiType);

            m_mTextDataMap[iId] = pTemp;
            ++uiCount;
        } while (pResult->NextRow());

        delete pResult;
    }
    logging.info("Loaded %u additional Custom Texts data.\n", uiCount);
}

void SystemMgr::LoadScriptGossipTexts()
{
    logging.info("SD2: Loading Gossip Texts...");
    LoadMangosStrings(SD2Database, "gossip_texts", TEXT_SOURCE_GOSSIP_START,
        TEXT_SOURCE_GOSSIP_END);
}

void SystemMgr::LoadScriptWaypoints()
{
    // Drop Existing Waypoint list
    m_mPointMoveMap.clear();

    uint64 uiCreatureCount = 0;

    // Load Waypoints
    QueryResult* pResult = SD2Database.PQuery(
        "SELECT COUNT(entry) FROM script_waypoint GROUP BY entry");
    if (pResult)
    {
        uiCreatureCount = pResult->GetRowCount();
        delete pResult;
    }

    logging.info("SD2: Loading Script Waypoints for " UI64FMTD
                 " creature(s)...",
        uiCreatureCount);

    pResult = SD2Database.PQuery(
        "SELECT entry, pointid, location_x, location_y, location_z, waittime "
        "FROM script_waypoint ORDER BY pointid");

    uint32 uiNodeCount = 0;
    if (pResult)
    {
        BarGoLink bar(pResult->GetRowCount());

        do
        {
            bar.step();
            Field* pFields = pResult->Fetch();
            ScriptPointMove pTemp;

            pTemp.uiCreatureEntry = pFields[0].GetUInt32();
            uint32 uiEntry = pTemp.uiCreatureEntry;
            pTemp.uiPointId = pFields[1].GetUInt32();
            pTemp.fX = pFields[2].GetFloat();
            pTemp.fY = pFields[3].GetFloat();
            pTemp.fZ = pFields[4].GetFloat();
            pTemp.uiWaitTime = pFields[5].GetUInt32();

            CreatureInfo const* pCInfo =
                GetCreatureTemplateStore(pTemp.uiCreatureEntry);

            if (!pCInfo)
            {
                logging.error(
                    "SD2: DB table script_waypoint has waypoint for "
                    "nonexistent creature entry %u",
                    pTemp.uiCreatureEntry);
                continue;
            }

            if (!pCInfo->ScriptID)
                logging.error(
                    "SD2: DB table script_waypoint has waypoint for creature "
                    "entry %u, but creature does not have ScriptName defined "
                    "and then useless.",
                    pTemp.uiCreatureEntry);

            m_mPointMoveMap[uiEntry].push_back(pTemp);
            ++uiNodeCount;
        } while (pResult->NextRow());

        delete pResult;
    }
    logging.info("Loaded %u Script Waypoint nodes.\n", uiNodeCount);
}
