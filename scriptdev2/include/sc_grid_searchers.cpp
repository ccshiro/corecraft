/* Copyright (C) 2006 - 2012 ScriptDev2 <http://www.scriptdev2.com/>
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#include "precompiled.h"
#include "SpecialVisCreature.h"
#include "Totem.h"
#include "TemporarySummon.h"
#include "maps/checks.h"
#include "maps/visitors.h"

// return closest GO in grid, with range from pSource
GameObject* GetClosestGameObjectWithEntry(
    WorldObject* pSource, uint32 uiEntry, float fMaxSearchRange)
{
    return maps::visitors::yield_best_match<GameObject>{}(
        pSource, fMaxSearchRange, maps::checks::entry_guid{uiEntry, 0});
}

// return closest creature alive in grid, with range from pSource
Creature* GetClosestCreatureWithEntry(WorldObject* pSource, uint32 uiEntry,
    float fMaxSearchRange, bool bOnlyAlive /*=true*/, bool bOnlyDead /*=false*/)
{
    boost::logic::tribool tribool;
    if (bOnlyAlive)
        tribool = true;
    else if (bOnlyDead)
        tribool = false;
    else
        tribool = maps::checks::alive_or_dead;

    return maps::visitors::yield_best_match<Creature, Creature,
        SpecialVisCreature, TemporarySummon, Totem>{}(pSource, fMaxSearchRange,
        maps::checks::entry_guid{uiEntry, 0, pSource, tribool});
}

std::vector<GameObject*> GetGameObjectListWithEntryInGrid(
    WorldObject* pSource, uint32 uiEntry, float fMaxSearchRange)
{
    return maps::visitors::yield_set<GameObject>{}(
        pSource, fMaxSearchRange, maps::checks::entry_guid{uiEntry, 0});
}

Player* GetClosestPlayer(WorldObject* source, float search_dist)
{
    return maps::visitors::yield_best_match<Player>{}(source, search_dist,
        [](Player* p)
        {
            return !p->isGameMaster();
        });
}

std::vector<Creature*> GetCreatureListWithEntryInGrid(
    WorldObject* pSource, uint32 uiEntry, float fMaxSearchRange)
{
    return maps::visitors::yield_set<Creature, Creature, SpecialVisCreature,
        TemporarySummon, Totem>{}(
        pSource, fMaxSearchRange, maps::checks::entry_guid{uiEntry, 0});
}

std::vector<Creature*> GetFriendlyCreatureListInGrid(
    WorldObject* pSource, float fMaxSearchRange)
{
    return maps::visitors::yield_set<Creature, Creature, SpecialVisCreature,
        TemporarySummon, Totem>{}(pSource, fMaxSearchRange, [pSource](Unit* u)
        {
            return u->isAlive() && pSource->IsFriendlyTo(u);
        });
}

std::vector<Creature*> GetUnfriendlyCreatureListInGrid(
    WorldObject* pSource, float fMaxSearchRange)
{
    return maps::visitors::yield_set<Creature, Creature, SpecialVisCreature,
        TemporarySummon, Totem>{}(
        pSource, fMaxSearchRange, maps::checks::friendly_status{pSource,
                                      maps::checks::friendly_status::hostile});
}

Creature* SelectCreatureWithDispellableAuraInGrid(WorldObject* source,
    bool positive_auras, DispelType type, bool los_only, float range)
{
    auto creatures =
        maps::visitors::yield_set<Creature, Creature, SpecialVisCreature,
            TemporarySummon, Totem>{}(source, range, [](Creature* elem)
            {
                return elem->isAlive();
            });

    for (auto creature : creatures)
    {
        if (creature->isDead())
            continue;

        if (los_only && !source->IsWithinWmoLOSInMap(creature))
            continue;

        if (positive_auras)
        {
            if (creature->HasDispellableBuff(type))
                return creature;
        }
        else
        {
            if (creature->HasDispellableDebuff(type))
                return creature;
        }
    }

    return nullptr;
}

Player* SelectPlayerWithDispellableAuraInMap(WorldObject* source,
    bool positive_auras, DispelType type, bool los_only, float range)
{
    const Map::PlayerList& list = source->GetMap()->GetPlayers();

    for (const auto& elem : list)
    {
        Player* player = elem.getSource();
        if (player->isGameMaster() || player->isDead() ||
            source->GetDistance(player) > range)
            continue;

        if (los_only && !source->IsWithinWmoLOSInMap(player))
            continue;

        if (positive_auras)
        {
            if (player->HasDispellableBuff(type))
                return player;
        }
        else
        {
            if (player->HasDispellableDebuff(type))
                return player;
        }
    }

    return nullptr;
}

Player* SelectPlayerThatIsCastingInMap(
    WorldObject* source, bool los_only, float range)
{
    const Map::PlayerList& list = source->GetMap()->GetPlayers();

    for (const auto& elem : list)
    {
        Player* player = elem.getSource();

        if (player->isDead() || player->isGameMaster())
            continue;

        if (los_only && !source->IsWithinWmoLOSInMap(player))
            continue;

        if (range <= 5 && source->GetTypeId() == TYPEID_UNIT)
        {
            // Use melee attack rather than distance if range <= 5. Reasoning:
            // It probably means we want to "pummel" a spell
            if (!static_cast<Creature*>(source)->CanReachWithMeleeAttack(
                    player))
                continue;
        }
        else
        {
            if (source->GetDistance(player) > range)
                continue;
        }

        if (!player->IsNonMeleeSpellCasted(false))
            continue;
        ;

        return player;
    }

    return nullptr;
}

std::vector<Player*> GetAllPlayersInObjectRangeCheckInCell(
    WorldObject* pSource, float fMaxSearchRange)
{
    auto set = maps::visitors::yield_set<Player>{}(pSource, fMaxSearchRange,
        maps::checks::entry_guid{0, 0, nullptr, true});

    std::remove_if(set.begin(), set.end(), [](const Player* const& p)
        {
            return p->isGameMaster();
        });

    return set;
}

std::vector<Unit*> AllEnemiesInRange(
    WorldObject* source, Unit* friendly_unit, float range)
{
    return maps::visitors::yield_set<Unit, Player, Creature, Pet,
        SpecialVisCreature, TemporarySummon>{}(source, range,
        [friendly_unit](Unit* u)
        {
            if (!u->isAlive())
                return false;

            if (u->GetTypeId() == TYPEID_UNIT &&
                u->GetCreatureType() == CREATURE_TYPE_NON_COMBAT_PET)
                return false;

            if (u->GetTypeId() == TYPEID_UNIT &&
                u->GetCreatureType() == CREATURE_TYPE_CRITTER)
                return false;

            if (!u->isTargetableForAttack(false))
                return false;

            return friendly_unit->IsHostileTo(u);
        });
}
