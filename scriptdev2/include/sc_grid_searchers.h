/* Copyright (C) 2006 - 2012 ScriptDev2 <http://www.scriptdev2.com/>
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#ifndef SC_GRIDSEARCH_H
#define SC_GRIDSEARCH_H

#include "Object.h"
#include "Unit.h"

class Creature;
class GameObject;
struct ObjectDistanceOrder
    : public std::binary_function<const WorldObject, const WorldObject, bool>
{
    const Unit* m_pSource;

    ObjectDistanceOrder(const Unit* pSource) : m_pSource(pSource){};

    bool operator()(const WorldObject* pLeft, const WorldObject* pRight) const
    {
        return m_pSource->GetDistanceOrder(pLeft, pRight);
    }
};

struct ObjectDistanceOrderReversed
    : public std::binary_function<const WorldObject, const WorldObject, bool>
{
    const Unit* m_pSource;

    ObjectDistanceOrderReversed(const Unit* pSource) : m_pSource(pSource){};

    bool operator()(const WorldObject* pLeft, const WorldObject* pRight) const
    {
        return !m_pSource->GetDistanceOrder(pLeft, pRight);
    }
};

GameObject* GetClosestGameObjectWithEntry(
    WorldObject* pSource, uint32 uiEntry, float fMaxSearchRange);
Creature* GetClosestCreatureWithEntry(WorldObject* pSource, uint32 uiEntry,
    float fMaxSearchRange, bool bOnlyAlive = true, bool bOnlyDead = false);
Player* GetClosestPlayer(WorldObject* source, float search_dist);

std::vector<GameObject*> GetGameObjectListWithEntryInGrid(
    WorldObject* pSource, uint32 uiEntry, float fMaxSearchRange);
std::vector<Creature*> GetCreatureListWithEntryInGrid(
    WorldObject* pSource, uint32 uiEntry, float fMaxSearchRange);

std::vector<Creature*> GetFriendlyCreatureListInGrid(
    WorldObject* pSource, float fMaxSearchRange);
std::vector<Creature*> GetUnfriendlyCreatureListInGrid(
    WorldObject* pSource, float fMaxSearchRange);

Creature* SelectCreatureWithDispellableAuraInGrid(WorldObject* pSource,
    bool positiveAuras, DispelType type, bool LoSonly, float fMaxSearchRange);
Player* SelectPlayerWithDispellableAuraInMap(WorldObject* pSource,
    bool positiveAuras, DispelType type, bool LoSonly, float fMaxSearchRange);

Player* SelectPlayerThatIsCastingInMap(
    WorldObject* pSource, bool LoSonly, float fMaxSearchRange);

std::vector<Player*> GetAllPlayersInObjectRangeCheckInCell(
    WorldObject* pSource, float fMaxSearchRange);

std::vector<Unit*> AllEnemiesInRange(
    WorldObject* source, Unit* friendly_unit, float range);

#endif
