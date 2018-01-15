#include "loot_selection.h"
#include "Creature.h"
#include "GameObject.h"
#include "Item.h"
#include "ObjectMgr.h"
#include "ProgressBar.h"
#include "Util.h"
#include "World.h"
#include "Database/Database.h"
#include <algorithm>

auto& log_slot = logging.get_logger("loot.slot");
auto& log_zone = logging.get_logger("loot.zone");

void loot_selection::load_from_db()
{
    load_loot_slots();
    load_zone_drops();
}

std::vector<std::pair<uint32, uint32>> loot_selection::select_loot(
    loot_selection_type type, Object* object) const
{
    std::vector<std::pair<uint32, uint32>> drops;

    uint32 level = 0;
    switch (type)
    {
    case LOOT_SELECTION_TYPE_CREATURE:
    case LOOT_SELECTION_TYPE_RARE_CREATURE:
    case LOOT_SELECTION_TYPE_PICKPOCKET:
        assert(object->GetTypeId() == TYPEID_UNIT);
        level = static_cast<Creature*>(object)->getLevel();
        if (level > 50 &&
            static_cast<Creature*>(object)->expansion_level() == 1)
            level += 10;
        break;
    case LOOT_SELECTION_TYPE_CHEST:
        assert(object->GetTypeId() == TYPEID_GAMEOBJECT);
        level =
            static_cast<GameObject*>(object)->GetGOInfo()->LootSelectionLevel;
        break;
    case LOOT_SELECTION_TYPE_LOCKBOX:
        assert(object->GetTypeId() == TYPEID_ITEM);
        level = static_cast<Item*>(object)->GetProto()->LootSelectionLevel;
        break;
    }

    // Does this object make use of loot selection?
    if (level == 0)
        return drops;

    uint32 ctype = 0, cfamily = 0;

    // Zone drops are creatures only
    if (type == LOOT_SELECTION_TYPE_CREATURE ||
        type == LOOT_SELECTION_TYPE_RARE_CREATURE)
    {
        drop_zone(static_cast<Creature*>(object), drops);
        ctype = static_cast<Creature*>(object)->GetCreatureInfo()->type;
        cfamily = static_cast<Creature*>(object)->GetCreatureInfo()->family;
    }

    bool elite = (type == LOOT_SELECTION_TYPE_CREATURE &&
                     static_cast<Creature*>(object)->IsElite()) ?
                     true :
                     false;

    // Drop items according to slot rules
    drop_slots(type, elite, ctype, cfamily, level, drops);

    return drops;
}

void loot_selection::load_loot_slots()
{
    std::unique_ptr<QueryResult> result(
        WorldDatabase.Query("SELECT id, name, has_quality FROM loot_slots"));
    if (!result)
    {
        logging.info("Loot Selection loaded 0 items.\n");
        return;
    }

    uint32 loaded_slots = 0, loaded_rules = 0, loaded_items = 0;

    do
    {
        Field* fields = result->Fetch();
        uint32 id = fields[0].GetUInt32();

        slot s;
        s.name = fields[1].GetCppString();
        s.has_quality = fields[2].GetUInt8();

        slots_[id] = std::move(s);
        ++loaded_slots;
    } while (result->NextRow());

    result.reset(WorldDatabase.Query(
        "SELECT id, drop_type, slot, chance, creature_types, "
        "creature_families, common_chance, uncommon_chance, rare_chance, "
        "epic_chance, common_chance_elite, uncommon_chance_elite, "
        "rare_chance_elite, epic_chance_elite FROM loot_slot_rules"));
    if (!result)
    {
        logging.info("Loot Selection loaded 0 items.\n");
        return;
    }

    do
    {
        Field* fields = result->Fetch();

        rule r;
        r.id = fields[0].GetUInt32();
        uint32 drop_type = fields[1].GetUInt32();
        r.slot = fields[2].GetUInt32();
        r.chance = fields[3].GetFloat();

        std::stringstream ss(fields[4].GetCppString());
        uint32 t;
        while (ss >> t)
            r.creature_types.push_back(t);

        std::stringstream ss2(fields[5].GetCppString());
        uint32 f;
        while (ss2 >> f)
            r.creature_families.push_back(f);

        r.common_chance = fields[6].GetFloat();
        r.uncommon_chance = fields[7].GetFloat();
        r.rare_chance = fields[8].GetFloat();
        r.epic_chance = fields[9].GetFloat();
        r.common_chance_elite = fields[10].GetFloat();
        r.uncommon_chance_elite = fields[11].GetFloat();
        r.rare_chance_elite = fields[12].GetFloat();
        r.epic_chance_elite = fields[13].GetFloat();

        if (slots_.find(r.slot) == slots_.end())
        {
            logging.error(
                "loot_slot_rules: %u has a slot not in loot_slots", r.id);
            continue;
        }

        if (r.chance == 0 || r.chance > 1.0f || r.common_chance > 1.0f ||
            r.uncommon_chance > 1.0f || r.rare_chance > 1.0f ||
            r.epic_chance > 1.0f || r.common_chance_elite > 1.0f ||
            r.uncommon_chance_elite > 1.0f || r.rare_chance_elite > 1.0f ||
            r.epic_chance_elite > 1.0f)
        {
            logging.error("loot_slot_rules: %u has invalid chance", r.id);
            continue;
        }

        if (drop_type == 0 || drop_type >= MAX_LOOT_SELECTION_TYPE)
        {
            logging.error("loot_slot_rules: %u has invalid drop_type", r.id);
            continue;
        }

        if ((drop_type != LOOT_SELECTION_TYPE_CREATURE &&
                drop_type != LOOT_SELECTION_TYPE_RARE_CREATURE) &&
            !(r.creature_types.empty() && r.creature_families.empty()))
        {
            logging.error(
                "loot_slot_rules: %u has creature type/family but isn't a "
                "creature rule",
                r.id);
            continue;
        }

        bool invalid = false;
        for (auto t : r.creature_types)
            if (t > MAX_CREATURE_TYPE)
            {
                logging.error(
                    "loot_slot_rules: %u has invalid creature_types", r.id);
                invalid = true;
                break;
            }
        for (auto f : r.creature_families)
            if (f > MAX_CREATURE_FAMILIES)
            {
                logging.error(
                    "loot_slot_rules: %u has invalid creature_families", r.id);
                invalid = true;
                break;
            }

        if (invalid)
            continue;

        rules_[(loot_selection_type)drop_type].push_back(std::move(r));
        ++loaded_rules;
    } while (result->NextRow());

    result.reset(WorldDatabase.Query(
        "SELECT item_id, slot, min_count, max_count, min_level, max_level, "
        "lowered_chance FROM loot_slot_items"));
    if (!result)
    {
        logging.info("Loot Selection loaded 0 items.");
        return;
    }

    BarGoLink bar(result->GetRowCount());
    do
    {
        bar.step();

        Field* field = result->Fetch();
        uint32 item_id = field[0].GetUInt32();
        uint32 slot = field[1].GetUInt32();
        uint16 min_count = field[2].GetUInt16();
        uint16 max_count = field[3].GetUInt16();
        uint32 min_level = field[4].GetUInt32();
        uint32 max_level = field[5].GetUInt32();
        float lowered = field[6].GetFloat();

        const ItemPrototype* proto =
            sObjectMgr::Instance()->GetItemPrototype(item_id);
        if (!proto)
        {
            logging.error(
                "loot_slot_items: Item with id %u has no corresponding item "
                "prototype. Ignored item as a result.",
                item_id);
            continue;
        }

        if (min_count == 0 || min_count > max_count)
        {
            logging.error(
                "loot_slot_items: Item with id %u has invalid min/max count.",
                item_id);
            continue;
        }

        if (slots_.find(slot) == slots_.end())
        {
            logging.error(
                "loot_slot_items: Item with id %u has invalid slot %u.",
                item_id, slot);
            continue;
        }

        if (static_cast<ItemQualities>(proto->Quality) > ITEM_QUALITY_EPIC)
        {
            logging.error(
                "loot_slot_items: Item with id %u has a quality above epic. "
                "Ignored item as a result.",
                item_id);
            continue;
        }

        if (min_level == 0)
        {
            if (max_level != 0)
            {
                logging.error(
                    "loot_slot_items: Item with id %u has an invalid level "
                    "range.",
                    item_id);
                continue;
            }

            auto p = calc_lvl_range(proto);
            min_level = p.first;
            max_level = p.second;
        }

        ++loaded_items;

        item i;
        i.item_id = item_id;
        i.min_count = min_count;
        i.max_count = max_count;
        i.lowered_chance = lowered;

        for (uint32 lvl = min_level; lvl <= max_level; ++lvl)
        {
            if (slots_[slot].has_quality)
                items_[slot][lvl][proto->Quality].push_back(i);
            else
                items_[slot][lvl][ITEM_QUALITY_NORMAL].push_back(i);
        }
    } while (result->NextRow());

    logging.info("Loot Selection loaded %u slots, %u rules and %u items.\n",
        loaded_slots, loaded_rules, loaded_items);
}

void loot_selection::load_zone_drops()
{
    logging.info("Loading Zone Drops...");
    std::unique_ptr<QueryResult> result(WorldDatabase.PQuery(
        "SELECT zone_id, item_id, chance, min_count, max_count, exclusion_list "
        "FROM loot_zone_drops"));
    if (!result)
    {
        logging.info("Loaded 0 zone drops.\n");
        return;
    }

    BarGoLink bar(result->GetRowCount());
    uint32 count = 0;
    do
    {
        bar.step();

        Field* field = result->Fetch();
        uint32 zone_id = field[0].GetUInt32();
        uint32 item_id = field[1].GetUInt32();
        float chance = field[2].GetFloat();
        uint8 min = field[3].GetUInt8();
        uint8 max = field[4].GetUInt8();
        std::string excludes = field[5].GetCppString();

        const ItemPrototype* prot =
            sObjectMgr::Instance()->GetItemPrototype(item_id);
        if (!prot)
        {
            logging.error(
                "loot_zone_drops: Zone %u, item %u has no corresponding item "
                "prototype. Ignored zone drop as a result.",
                zone_id, item_id);
            continue;
        }

        if (chance <= 0 || chance > 100)
        {
            logging.error(
                "loot_zone_drops: Zone %u, item %u has an invalid drop chance, "
                "valid range: (0,100]. Ignored zone drop as a result.",
                zone_id, item_id);
            continue;
        }

        if (min == 0 || max == 0 || max > min)
        {
            logging.error(
                "loot_zone_drops: Zone %u, item %u has an invalid min/max "
                "count. Ignored zone drop as a result.",
                zone_id, item_id);
            continue;
        }

        std::set<uint32> excluded;
        std::stringstream ss;
        ss << excludes;

        uint32 id;
        while (ss >> id)
            excluded.insert(id);

        zone_drop d = {item_id, chance, min, max, excluded};
        zone_drops_[zone_id].push_back(d);

        ++count;
    } while (result->NextRow());

    // Verify all zones that their total drop chance does not exceed 100%
    for (auto itr = zone_drops_.begin(); itr != zone_drops_.end();)
    {
        float accumulate = 0;
        for (const auto& d : itr->second)
            accumulate += d.chance;
        if (accumulate > 100.0f)
        {
            count -= itr->second.size();
            logging.error(
                "loot_zone_drops: Zone %u was skipped completely because the "
                "accumulated chance of all items exceeded 100%%.",
                itr->first);
            itr = zone_drops_.erase(itr);
        }
        else
            ++itr;
    }

    logging.info("Loaded %u zone drops.\n", count);
}

std::pair<uint32, uint32> loot_selection::calc_lvl_range(
    const ItemPrototype* proto) const
{
    int min, max;

    // For vanilla the level derivation is simple
    if (proto->ItemLevel <= 60)
    {
        min = (int)proto->ItemLevel - 1;
        max = (int)proto->ItemLevel + 3;
    }
    // For TBC it's more erradic
    else
    {
        if (proto->Quality == ITEM_QUALITY_POOR)
        {
            if (proto->ItemLevel == 66)
            {
                min = 58;
                max = 65;
            }
            else if (proto->ItemLevel == 72)
            {
                min = 66;
                max = 73;
            }
            else
            {
                min = 58;
                max = 73;
            }
        }
        else if (proto->Quality == ITEM_QUALITY_EPIC && proto->ItemLevel == 100)
        {
            min = 70;
            max = 73;
        }
        else
        {
            min = ((int)proto->ItemLevel - 80) / 4 + 58 - 1;
            max = ((int)proto->ItemLevel - 80) / 4 + 58 + 3;
        }

        // NOTE: TBC level offset by +10
        min += 10;
        max += 10;
    }

    return std::make_pair(uint32(min), uint32(max));
}

ItemQualities loot_selection::rand_quality(
    const slot& slot_data, const rule& r, bool elite) const
{
    ItemQualities quality = ITEM_QUALITY_NORMAL;

    if (slot_data.has_quality)
    {
        float epic_chance = elite ? r.epic_chance_elite : r.epic_chance;
        float common_chance = elite ? r.common_chance_elite : r.common_chance;
        float rare_chance = elite ? r.rare_chance_elite : r.rare_chance;
        float uncommon_chance =
            elite ? r.uncommon_chance_elite : r.uncommon_chance;

        if (epic_chance > 0 && rand_norm_f() < epic_chance)
            quality = ITEM_QUALITY_EPIC;
        else if (rare_chance > 0 && rand_norm_f() < rare_chance)
            quality = ITEM_QUALITY_RARE;
        else if (uncommon_chance > 0 && rand_norm_f() < uncommon_chance)
            quality = ITEM_QUALITY_UNCOMMON;
        else if (common_chance > 0 && rand_norm_f() < common_chance)
            quality = ITEM_QUALITY_NORMAL;
        else
            quality = ITEM_QUALITY_POOR;

        LOG_DEBUG(log_slot,
            "Selected quality %u. Drop chances: Epic: %f, Rare: %f, Uncommon: "
            "%f, Normal: %f.",
            quality, epic_chance, rare_chance, uncommon_chance, common_chance);
    }

    return quality;
}

// helper classes for zone drops
class range_container_entry
{
public:
    range_container_entry(
        float min, float max, const loot_selection::zone_drop& drop)
      : min_(min), max_(max), drop_(drop)
    {
    }

    bool matches(float f) const { return min_ <= f && f < max_; }
    const loot_selection::zone_drop& drop() const { return drop_; };

private:
    float min_;
    float max_;
    const loot_selection::zone_drop& drop_;
};
class range_container
{
public:
    void push_back(const range_container_entry& e) { vec_.push_back(e); }
    const loot_selection::zone_drop* match(float f) const
    {
        for (auto& e : vec_)
            if (e.matches(f))
                return &e.drop();
        return nullptr;
    }

private:
    std::vector<range_container_entry> vec_;
};

void loot_selection::drop_slots(loot_selection_type type, bool elite,
    uint32 ctype, uint32 cfamily, uint32 level,
    std::vector<std::pair<uint32, uint32>>& drops) const
{
    LOG_DEBUG(log_slot,
        "Dropping slot loot for type %u elite: %s ctype: %u "
        "cfamily: %u level: %u",
        type, (elite ? "yes" : "no"), ctype, cfamily, level);

    auto itr = rules_.find(type);
    if (itr == rules_.end())
        return;

    const auto& rules = itr->second;
    for (const rule& r : rules)
    {
        // Check criteria for rule
        if (!r.creature_types.empty() &&
            std::find(r.creature_types.begin(), r.creature_types.end(),
                ctype) == r.creature_types.end())
        {
            LOG_DEBUG(log_slot, "Rule %u does not match ctype %u", r.id, ctype);
            continue;
        }
        if (!r.creature_families.empty() &&
            std::find(r.creature_families.begin(), r.creature_families.end(),
                cfamily) == r.creature_families.end())
        {
            LOG_DEBUG(
                log_slot, "Rule %u does not match cfamily %u", r.id, cfamily);
            continue;
        }
        // Throw dice
        float roll = rand_norm_f();
        if (roll > r.chance)
        {
            LOG_DEBUG(log_slot, "Rule %u did not pass chance of %f (roll: %f)",
                r.id, r.chance, roll);
            continue;
        }

        // Get slot data
        auto sitr = slots_.find(r.slot);
        if (sitr == slots_.end())
            continue;
        const slot& s = sitr->second;

        LOG_DEBUG(log_slot,
            "Rule %u passed roll, dropping from slot '%s' according to rule...",
            r.id, s.name.c_str());

        // Get quality map
        auto sm = items_.find(sitr->first);
        if (sm == items_.end())
            continue;
        auto lm = sm->second.find(level);
        if (lm == sm->second.end())
            continue;
        const auto& quality_map = lm->second;

        // Find item with matching quality
        auto quality = rand_quality(s, r, elite);
        while (true)
        {
            auto itr = quality_map.find(quality);
            if (itr != quality_map.end() && !itr->second.empty())
            {
                const auto& drop =
                    itr->second[urand(0, itr->second.size() - 1)];
                if (drop.lowered_chance > 0 && roll > drop.lowered_chance)
                {
                    LOG_DEBUG(log_slot,
                        "Rule %u missed out on %u due to lowered chance (%f), "
                        "roll was %f",
                        r.id, drop.item_id, drop.lowered_chance, roll);
                    break;
                }
                drops.push_back(std::make_pair(
                    drop.item_id, urand(drop.min_count, drop.max_count)));
                LOG_DEBUG(log_slot,
                    "Rule %u caused item %u to be dropped %u times", r.id,
                    drops.back().first, drops.back().second);
                break;
            }
            if (!s.has_quality || quality == ITEM_QUALITY_POOR)
            {
                LOG_DEBUG(
                    log_slot, "Rule %u was selected, but no valid items", r.id);
                break;
            }

            LOG_DEBUG(log_slot,
                "Could not find item with quality %u, downgrading", quality);
            quality = (ItemQualities)((int)quality - 1);
        }
    }
}

void loot_selection::drop_zone(
    Creature* creature, std::vector<std::pair<uint32, uint32>>& drops) const
{
    auto itr = zone_drops_.find(creature->GetZoneId());
    if (itr == zone_drops_.end())
    {
        LOG_DEBUG(log_zone, "No available zone drops for zone with id: %u.",
            creature->GetZoneId());
        return;
    }

    // to make sure the drop chance of a zone drop does not affect the other we
    // take the following approach:
    // a) build up a range, starting at 0. let's say we have 3 items with drop
    // chance 5%, 20% and 50%, our range would be:
    //      [0,5):   drops item A,
    //      [5,25):  drops item B,
    //      [25,75): drops item C
    // b) do a roll between 0 and 100, and decide the item that drops

    float at = 0;
    range_container c;

    for (const auto& drop : itr->second)
    {
        auto exc_itr = drop.exclusion_list.find(creature->GetEntry());
        if (exc_itr != drop.exclusion_list.end())
        {
            LOG_DEBUG(log_zone,
                "Skipped item %u because creature %u was in the exclusion "
                "list.",
                drop.item_id, creature->GetEntry());
            continue;
        }

        range_container_entry e(at, at + drop.chance, drop);
        c.push_back(e);
        at += drop.chance; // that this does not overflow 100% is checked in
                           // loading
    }

    float roll = frand(0, 100);
    auto drop = c.match(roll);
    if (drop)
    {
        uint32 count =
            urand(drop->min, drop->max); // validity checked at DB load
        LOG_DEBUG(log_zone, "Dropped zone item: %u count: %u (zone: %u)",
            drop->item_id, count, creature->GetZoneId());

        drops.push_back(std::make_pair(drop->item_id, count));
    }
    else
    {
        LOG_DEBUG(log_zone,
            "Rolled for drop in zone %u. A pool of %u items exists for the "
            "zone.",
            creature->GetZoneId(), static_cast<uint32>(itr->second.size()));
    }
}
