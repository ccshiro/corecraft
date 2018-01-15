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

#include "SkillDiscovery.h"
#include "logging.h"
#include "Player.h"
#include "ProgressBar.h"
#include "SpellMgr.h"
#include "Util.h"
#include "World.h"
#include "Database/DatabaseEnv.h"
#include "Policies/Singleton.h"
#include <map>
#include <unordered_map>

struct SkillDiscoveryEntry
{
    uint32 spellId; // discavered spell
    float chance;   // chance

    SkillDiscoveryEntry() : spellId(0), chance(0) {}

    SkillDiscoveryEntry(uint32 _spellId, float _chance)
      : spellId(_spellId), chance(_chance)
    {
    }
};

typedef std::list<SkillDiscoveryEntry> SkillDiscoveryList;
typedef std::unordered_map<int32, SkillDiscoveryList> SkillDiscoveryMap;

static SkillDiscoveryMap SkillDiscoveryStore;

void LoadSkillDiscoveryTable()
{
    SkillDiscoveryStore.clear(); // need for reload

    uint32 count = 0;

    QueryResult* result = WorldDatabase.Query(
        "SELECT spellId, reqSpell, chance FROM skill_discovery_template");

    if (!result)
    {
        logging.info(
            "Loaded 0 skill discovery definitions. DB table "
            "`skill_discovery_template` is empty.\n");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    std::ostringstream ssNonDiscoverableEntries;
    std::set<uint32> reportedReqSpells;

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 spellId = fields[0].GetUInt32();
        int32 reqSkillOrSpell = fields[1].GetInt32();
        float chance = fields[2].GetFloat();

        if (chance <= 0) // chance
        {
            ssNonDiscoverableEntries << "spellId = " << spellId
                                     << " reqSkillOrSpell = " << reqSkillOrSpell
                                     << " chance = " << chance << "\n";
            continue;
        }

        if (reqSkillOrSpell > 0) // spell case
        {
            SpellEntry const* reqSpellEntry =
                sSpellStore.LookupEntry(reqSkillOrSpell);
            if (!reqSpellEntry)
            {
                if (reportedReqSpells.find(reqSkillOrSpell) ==
                    reportedReqSpells.end())
                {
                    logging.error(
                        "Spell (ID: %u) have nonexistent spell (ID: %i) in "
                        "`reqSpell` field in `skill_discovery_template` table",
                        spellId, reqSkillOrSpell);
                    reportedReqSpells.insert(reqSkillOrSpell);
                }
                continue;
            }

            if (reqSpellEntry->Mechanic != MECHANIC_DISCOVERY)
            {
                if (reportedReqSpells.find(reqSkillOrSpell) ==
                    reportedReqSpells.end())
                {
                    logging.error(
                        "Spell (ID: %u) not have MECHANIC_DISCOVERY (28) value "
                        "in Mechanic field in spell.dbc but listed for spellId "
                        "%u (and maybe more) in `skill_discovery_template` "
                        "table",
                        reqSkillOrSpell, spellId);
                    reportedReqSpells.insert(reqSkillOrSpell);
                }
                continue;
            }

            SkillDiscoveryStore[reqSkillOrSpell].push_back(
                SkillDiscoveryEntry(spellId, chance));
        }
        else if (reqSkillOrSpell == 0) // skill case
        {
            SkillLineAbilityMapBounds bounds =
                sSpellMgr::Instance()->GetSkillLineAbilityMapBounds(spellId);

            if (bounds.first == bounds.second)
            {
                logging.error(
                    "Spell (ID: %u) not listed in `SkillLineAbility.dbc` but "
                    "listed with `reqSpell`=0 in `skill_discovery_template` "
                    "table",
                    spellId);
                continue;
            }

            for (auto _spell_idx = bounds.first; _spell_idx != bounds.second;
                 ++_spell_idx)
                SkillDiscoveryStore[-int32(_spell_idx->second->skillId)]
                    .push_back(SkillDiscoveryEntry(spellId, chance));
        }
        else
        {
            logging.error(
                "Spell (ID: %u) have negative value in `reqSpell` field in "
                "`skill_discovery_template` table",
                spellId);
            continue;
        }

        ++count;
    } while (result->NextRow());

    delete result;

    logging.info("Loaded %u skill discovery definitions\n", count);
    if (!ssNonDiscoverableEntries.str().empty())
        logging.error(
            "Some items can't be successfully discovered: have in chance field "
            "value < 0.000001 in `skill_discovery_template` DB table . "
            "List:\n%s",
            ssNonDiscoverableEntries.str().c_str());
}

uint32 GetSkillDiscoverySpell(uint32 skillId, uint32 spellId, Player* player)
{
    // check spell case
    SkillDiscoveryMap::const_iterator tab = SkillDiscoveryStore.find(spellId);

    if (tab != SkillDiscoveryStore.end())
    {
        for (const auto& elem : tab->second)
        {
            if (roll_chance_f(elem.chance *
                              sWorld::Instance()->getConfig(
                                  CONFIG_FLOAT_RATE_SKILL_DISCOVERY)) &&
                !player->HasSpell(elem.spellId))
                return elem.spellId;
        }

        return 0;
    }

    if (!skillId)
        return 0;

    // check skill line case
    tab = SkillDiscoveryStore.find(-(int32)skillId);
    if (tab != SkillDiscoveryStore.end())
    {
        for (const auto& elem : tab->second)
        {
            if (roll_chance_f(elem.chance *
                              sWorld::Instance()->getConfig(
                                  CONFIG_FLOAT_RATE_SKILL_DISCOVERY)) &&
                !player->HasSpell(elem.spellId))
                return elem.spellId;
        }

        return 0;
    }

    return 0;
}
