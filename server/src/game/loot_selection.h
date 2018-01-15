#ifndef GAME__LOOT_SELECTION_H
#define GAME__LOOT_SELECTION_H

#include "ItemPrototype.h"
#include "SharedDefines.h"
#include "Policies/Singleton.h"
#include <map>
#include <utility>
#include <vector>

class Creature;
class GameObject;
class Object;

/* How the DB tables work:
 * `loot_zone_drops`:
 * Description:
 *     Defines drops that can happen based on Zone. Mobs in said zone are giving
 *     a chance to drop the specified item.
 * Columns:
 *     `zone_id`:        The Zone the drop can happen in.
 *     `item_id`:        Item id.
 *     `chance`:         Chance the drop has. Range: (0, 100] *
 *     `min_count`:      Min stacks of item.
 *     `max_count`:      Max stacks of item.
 *     `exclusion_list`: Ids of creatures that do not drop said item. Space
 *                       seperated.
 *
 * * The total drop chance of a zone may NOT exceed 100. Reason is given in
 * loot_selection::drop_zone_drops if you're curious as to why.
 *
 * `loot_slots`:
 * Description:
 *     Defines what loot slots there are, as well as their drop chance.
 * Columns:
 *     `id`:              Loot selection slot id.
 *     `name`:            Slot name, mainly used for organizational purposes
 *     `has_quality`:     True if the slot has qualities.
 *
 * `loot_slot_rules`:
 * Description:
 *     Defines the rules for how loot is selected.
 * Columns:
 *     `drop_type`:         Loot type (see enum loot_selection_type).
 *     `slot`:              Slot id. Drop a single item from this slot.
 *     `chance`:            Chance slot has to drop (0, 1].
 *     `creature_types`:    creature_template.type that may drop this. Space
 *                          seperated for multiple.
 *     `creature_families`: creature_template.family that may drop this. Space
 *                          seperated for multiple.
 *     `common_chance`:     Chance for common items.
 *     `uncommon_chance`:   Chance for uncommon items.
 *     `rare_chance`:       Chance for rare items.
 *     `epic_chance`:       Chance for epic items.
 *
 *  Note about quality chances:
 *    - All chances are in range (0, 1].
 *    - They only apply if has_quality is True.
 *    - Each column has a corresponding _elite column defining the drop rate for
 *      elite mobs.
 *    - If the slot has qualities, then the poor chance is derived as:
 *      1.0 - (all non-poor chances combined)
 *    - Note that the overall chance defines the chance of said slot, and only
 *      when the slot passes its roll are the quality chances tried. For
 *      example, if you have a chance of 0.1, and an uncommon chance of 0.1,
 *      you'd get a combined chance of 0.01, resulting in an average of 1
 *      uncommon per 100 drops.
 *
 * `loot_slot_items`:
 * Description:
 *     Defines what loot slots can drop and how the rules for said drop is.
 * Columns:
 *     `item_id`:        Item id.
 *     `slot`:           Loot selection slot id.
 *     `min_count`:      Min stacks of item.
 *     `max_count`:      Max stacks of item.
 *     `min_level`:      Min level this drops from.
 *     `max_level`:      Max level this drops from.
 *     `lowered_chance`: Specific chance for item. Only has effect if lower than
 *                       loot_slot_rules.chance. (*)
 *
 * If level is (0, 0) then ilvl of item is used to derive level range. A mob in
 * TBC content whom is above level 50 has its loot selection level offset by
 * +10. So if an item can drop from 50, 60, a level 58 TBC mob will not drop it,
 * but a Vanilla mob would.
 *
 * Level for non-NPC drops are specified in the template of said object. For
 * items: item_template.loot_selection_level; for GOs:
 * gameobject_template.loot_selection_level.
 *
 * (*) Note that this column messes with the overall chance that somehting will
 * drop. This should only be used in special circumstances. Let's say you're
 * defining cloth drops, and you need wool at level X to Y to have a lower
 * chance, use this column. Otherwise stay clear!
 *
 */

/* Notations:
 * - It is possible for a mob to have an empty drop (except for mobs that always
 *   drop money.)
 * - Only one cloth type can drop at a time.
 * - Water drops from melee mobs as well.
 * - Instance mobs (elites?) seem to drop greens more frequently.
 * - Armor & Weapon drops from beast and other non-humanoids as well.
 */

enum loot_selection_type
{
    LOOT_SELECTION_TYPE_CREATURE = 1,
    LOOT_SELECTION_TYPE_CHEST = 2,
    LOOT_SELECTION_TYPE_PICKPOCKET = 3,
    LOOT_SELECTION_TYPE_LOCKBOX = 4,
    LOOT_SELECTION_TYPE_RARE_CREATURE = 5,
};

#define MAX_LOOT_SELECTION_TYPE 6

class loot_selection
{
public:
    // Non thread-safe functions
    void load_from_db();

    // Thread-safe functions (assuming no non-thread safe is being used)
    std::vector<std::pair<uint32, uint32>> select_loot(
        loot_selection_type type, Object* object) const;

private:
    friend class range_container_entry;
    friend class range_container;
    struct zone_drop
    {
        // order dependent, don't rearrange
        uint32 item_id;
        float chance;
        uint32 min;
        uint32 max;
        std::set<uint32> exclusion_list; // excluded creature entries
    };

    struct slot
    {
        std::string name;
        bool has_quality;
    };

    struct rule
    {
        uint32 id; // rule id, for debugging purposes only
        uint32 slot;
        float chance;
        std::vector<uint32> creature_types;
        std::vector<uint32> creature_families;
        float common_chance;
        float uncommon_chance;
        float rare_chance;
        float epic_chance;
        float common_chance_elite;
        float uncommon_chance_elite;
        float rare_chance_elite;
        float epic_chance_elite;
    };

    struct item
    {
        uint32 item_id;
        uint16 min_count;
        uint16 max_count;
        float lowered_chance;
    };

    // map<slot id, slot>
    std::map<uint32, slot> slots_;
    // map<type, vector<rule>>
    std::map<loot_selection_type, std::vector<rule>> rules_;
    // map<slot id, map<level, map<quality, vector<item>>>>
    std::map<uint32, std::map<uint32, std::map<uint32, std::vector<item>>>>
        items_;
    std::map<uint32 /*zone*/, std::vector<zone_drop>> zone_drops_;

    void load_loot_slots();
    void load_zone_drops();

    std::pair<uint32, uint32> calc_lvl_range(const ItemPrototype* proto) const;
    ItemQualities rand_quality(
        const slot& slot_data, const rule& r, bool elite) const;

    void drop_slots(loot_selection_type type, bool elite, uint32 ctype,
        uint32 cfamily, uint32 level,
        std::vector<std::pair<uint32, uint32>>& drops) const;
    void drop_zone(Creature* creature,
        std::vector<std::pair<uint32, uint32>>& drops) const;
};

#define sLootSelection MaNGOS::UnlockedSingleton<loot_selection>

#endif
