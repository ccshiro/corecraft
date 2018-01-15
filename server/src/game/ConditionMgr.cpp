/*
 * Copyright (C) 2008-2012 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2005-2009 MaNGOS <http://getmangos.com/>
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

#include "ConditionMgr.h"
#include "GameEventMgr.h"
#include "InstanceData.h"
#include "ObjectMgr.h"
#include "ProgressBar.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpecialVisCreature.h"
#include "Totem.h"
#include "TemporarySummon.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "World.h"
#include "maps/visitors.h"

// Checks if object meets the condition
// Can have CONDITION_SOURCE_TYPE_NONE && !mReferenceId if called from a special
// event (ie: eventAI)
bool Condition::Meets(ConditionSourceInfo& sourceInfo) const
{
    assert(ConditionTarget < MAX_CONDITION_TARGETS);
    WorldObject* object = sourceInfo.mConditionTargets[ConditionTarget];
    // object not present, return false
    if (!object)
    {
        LOG_DEBUG(logging,
            "Condition object not found for condition (Entry: %u Type: %u "
            "Group: %u)",
            SourceEntry, SourceType, SourceGroup);
        return false;
    }
    bool condMeets = false;
    switch (ConditionType)
    {
    case TC_CONDITION_NONE:
        condMeets = true; // empty condition, always met
        break;
    case TC_CONDITION_AURA:
    {
        if (object->GetTypeId() == TYPEID_UNIT ||
            object->GetTypeId() == TYPEID_PLAYER)
        {
            if (ConditionValue2)
                condMeets = ((Unit*)object)->GetAuraCount(ConditionValue1) ==
                            ConditionValue2;
            else
                condMeets = ((Unit*)object)->has_aura(ConditionValue1);
        }
        break;
    }
    case TC_CONDITION_ITEM:
    {
        if (object->GetTypeId() == TYPEID_PLAYER)
        {
            // don't allow 0 items (it's checked during table load)
            assert(ConditionValue2);
            bool checkBank = ConditionValue3 ? true : false;
            condMeets =
                ((Player*)object)
                    ->HasItemCount(ConditionValue1, ConditionValue2, checkBank);
        }
        break;
    }
    case TC_CONDITION_ITEM_EQUIPPED:
    {
        if (object->GetTypeId() == TYPEID_PLAYER)
            condMeets = ((Player*)object)->has_item_equipped(ConditionValue1);
        break;
    }
    case TC_CONDITION_ZONEID:
        condMeets = object->GetZoneId() == ConditionValue1;
        break;
    case TC_CONDITION_REPUTATION_RANK:
    {
        if (object->GetTypeId() == TYPEID_PLAYER)
        {
            if (FactionEntry const* faction =
                    sFactionStore.LookupEntry(ConditionValue1))
                condMeets = (ConditionValue2 & (1 << ((Player*)object)
                                                         ->GetReputationMgr()
                                                         .GetRank(faction)));
        }
        break;
    }
    case TC_CONDITION_ACHIEVEMENT:
    {
        condMeets = false;
        /*if (object->GetTypeId() == TYPEID_PLAYER)
            condMeets = ((Player*)object)->HasAchieved(ConditionValue1);*/
        break;
    }
    case TC_CONDITION_TEAM:
    {
        if (object->GetTypeId() == TYPEID_PLAYER)
            condMeets = ((Player*)object)->GetTeam() == ConditionValue1;
        break;
    }
    case TC_CONDITION_CLASS:
    {
        if (object->GetTypeId() == TYPEID_UNIT ||
            object->GetTypeId() == TYPEID_PLAYER)
            condMeets = ((Unit*)object)->getClassMask() & ConditionValue1;
        break;
    }
    case TC_CONDITION_RACE:
    {
        if (object->GetTypeId() == TYPEID_UNIT ||
            object->GetTypeId() == TYPEID_PLAYER)
            condMeets = ((Unit*)object)->getRaceMask() & ConditionValue1;
        break;
    }
    case TC_CONDITION_SKILL:
    {
        if (object->GetTypeId() == TYPEID_PLAYER)
            condMeets = ((Player*)object)->HasSkill(ConditionValue1) &&
                        ((Player*)object)->GetBaseSkillValue(ConditionValue1) >=
                            ConditionValue2;
        break;
    }
    case TC_CONDITION_QUESTREWARDED:
    {
        if (object->GetTypeId() == TYPEID_PLAYER)
            condMeets =
                ((Player*)object)->GetQuestRewardStatus(ConditionValue1);
        break;
    }
    case TC_CONDITION_QUESTTAKEN:
    {
        if (object->GetTypeId() == TYPEID_PLAYER)
        {
            QuestStatus status =
                ((Player*)object)->GetQuestStatus(ConditionValue1);
            condMeets = (status == QUEST_STATUS_INCOMPLETE);
        }
        break;
    }
    case TC_CONDITION_QUEST_COMPLETE:
    {
        if (object->GetTypeId() == TYPEID_PLAYER)
        {
            QuestStatus status =
                ((Player*)object)->GetQuestStatus(ConditionValue1);
            condMeets =
                (status == QUEST_STATUS_COMPLETE &&
                    !((Player*)object)->GetQuestRewardStatus(ConditionValue1));
        }
        break;
    }
    case TC_CONDITION_QUEST_NONE:
    {
        if (object->GetTypeId() == TYPEID_PLAYER)
        {
            QuestStatus status =
                ((Player*)object)->GetQuestStatus(ConditionValue1);
            condMeets = (status == QUEST_STATUS_NONE);
        }
        break;
    }
    case TC_CONDITION_ACTIVE_EVENT:
        condMeets = sGameEventMgr::Instance()->IsActiveEvent(ConditionValue1);
        break;
    case TC_CONDITION_INSTANCE_DATA:
    {
        if (InstanceData* id = object->GetMap()->GetInstanceData())
            condMeets = id->GetData(ConditionValue1) == ConditionValue2;
        break;
    }
    case TC_CONDITION_MAPID:
        condMeets = object->GetMapId() == ConditionValue1;
        break;
    case TC_CONDITION_AREAID:
        condMeets = object->GetAreaId() == ConditionValue1;
        break;
    case TC_CONDITION_SPELL:
    {
        if (object->GetTypeId() == TYPEID_PLAYER)
            condMeets = ((Player*)object)->HasSpell(ConditionValue1);
        break;
    }
    case TC_CONDITION_LEVEL:
    {
        if (object->GetTypeId() == TYPEID_UNIT ||
            object->GetTypeId() == TYPEID_PLAYER)
            condMeets =
                CompareValues(static_cast<ComparisionType>(ConditionValue2),
                    static_cast<uint32>(((Unit*)object)->getLevel()),
                    ConditionValue1);
        break;
    }
    case TC_CONDITION_DRUNKENSTATE:
    {
        if (object->GetTypeId() == TYPEID_PLAYER)
            condMeets =
                (uint32)Player::GetDrunkenstateByValue(
                    ((Player*)object)->GetDrunkValue()) >= ConditionValue1;
        break;
    }
    case TC_CONDITION_NEAR_CREATURE:
    {
        condMeets = object->FindNearestCreature(
                        ConditionValue1, (float)ConditionValue2) ?
                        true :
                        false;
        break;
    }
    case TC_CONDITION_NEAR_GAMEOBJECT:
    {
        condMeets = object->FindNearestGameObject(
                        ConditionValue1, (float)ConditionValue2) ?
                        true :
                        false;
        break;
    }
    case TC_CONDITION_OBJECT_ENTRY:
    {
        if (uint32(object->GetTypeId()) == ConditionValue1)
            condMeets =
                (!ConditionValue2) || (object->GetEntry() == ConditionValue2);
        break;
    }
    case TC_CONDITION_TYPE_MASK:
    {
        condMeets = object->isType((TypeMask)ConditionValue1);
        break;
    }
    case TC_CONDITION_RELATION_TO:
    {
        if (WorldObject* toObject =
                sourceInfo.mConditionTargets[ConditionValue1])
        {
            Unit* toUnit = ((Unit*)toObject);
            Unit* unit = ((Unit*)object);
            if ((toObject->GetTypeId() == TYPEID_UNIT ||
                    toObject->GetTypeId() == TYPEID_PLAYER) &&
                (object->GetTypeId() == TYPEID_UNIT ||
                    object->GetTypeId() == TYPEID_PLAYER))
            {
                switch (ConditionValue2)
                {
                case RELATION_SELF:
                    condMeets = unit == toUnit;
                    break;
                case RELATION_IN_PARTY:
                    condMeets = unit->IsInPartyWith(toUnit);
                    break;
                case RELATION_IN_RAID_OR_PARTY:
                    condMeets = unit->IsInRaidWith(toUnit);
                    break;
                case RELATION_OWNED_BY:
                    condMeets = unit->GetOwnerGuid() == toUnit->GetObjectGuid();
                    break;
                case RELATION_PASSENGER_OF:
                    condMeets = false;
                    // condMeets = unit->IsOnVehicle(toUnit);
                    break;
                case RELATION_CREATED_BY:
                    condMeets =
                        unit->GetCreatorGuid() == toUnit->GetObjectGuid();
                    break;
                }
            }
        }
        break;
    }
    case TC_CONDITION_REACTION_TO:
    {
        if (WorldObject* toObject =
                sourceInfo.mConditionTargets[ConditionValue1])
        {
            Unit* toUnit = ((Unit*)toObject);
            Unit* unit = ((Unit*)object);
            if ((toObject->GetTypeId() == TYPEID_UNIT ||
                    toObject->GetTypeId() == TYPEID_PLAYER) &&
                (object->GetTypeId() == TYPEID_UNIT ||
                    object->GetTypeId() == TYPEID_PLAYER))
                condMeets =
                    (1 << unit->GetReactionTo(toUnit)) & ConditionValue2;
        }
        break;
    }
    case TC_CONDITION_DISTANCE_TO:
    {
        if (WorldObject* toObject =
                sourceInfo.mConditionTargets[ConditionValue1])
            condMeets =
                CompareValues(static_cast<ComparisionType>(ConditionValue3),
                    object->GetDistance(toObject),
                    static_cast<float>(ConditionValue2));
        break;
    }
    case TC_CONDITION_ALIVE:
    {
        // ConditionValue1 is guid and 2 is entry
        if (ConditionValue1 != 0)
        {
            if (ConditionValue2 != 0)
            {
                if (Creature* target = object->GetMap()->GetCreature(ObjectGuid(
                        HIGHGUID_UNIT, ConditionValue2, ConditionValue1)))
                    condMeets = target->isAlive();
            }
            else
            {
                auto creature = maps::visitors::yield_single<Creature>{}(object,
                    object->GetMap()->GetVisibilityDistance(),
                    [this](Creature* c)
                    {
                        return c->GetGUIDLow() == ConditionValue1;
                    });

                if (creature)
                    condMeets = creature->isAlive();
            }
        }
        else
        {
            if (object->GetTypeId() == TYPEID_UNIT ||
                object->GetTypeId() == TYPEID_PLAYER)
                condMeets = ((Unit*)object)->isAlive();
        }
        break;
    }
    case TC_CONDITION_HP_VAL:
    {
        if (object->GetTypeId() == TYPEID_UNIT ||
            object->GetTypeId() == TYPEID_PLAYER)
            condMeets =
                CompareValues(static_cast<ComparisionType>(ConditionValue2),
                    ((Unit*)object)->GetHealth(),
                    static_cast<uint32>(ConditionValue1));
        break;
    }
    case TC_CONDITION_HP_PCT:
    {
        if (object->GetTypeId() == TYPEID_UNIT ||
            object->GetTypeId() == TYPEID_PLAYER)
            condMeets =
                CompareValues(static_cast<ComparisionType>(ConditionValue2),
                    ((Unit*)object)->GetHealthPercent(),
                    static_cast<float>(ConditionValue1));
        break;
    }
    case TC_CONDITION_WORLD_STATE:
    {
        condMeets = false;
        // condMeets = ConditionValue2 ==
        // sWorld::Instance()->getWorldState(ConditionValue1); // FIXME: What is
        // world states and do they exist in 2.4.3?
        break;
    }
    case TC_CONDITION_PHASEMASK:
    {
        condMeets = false; // FIXME? Phases shouldn't exist in 2.4.3, afaik
        // condMeets = object->GetPhaseMask() & ConditionValue1;
        break;
    }
    case TC_CONDITION_TITLE:
    {
        if (object->GetTypeId() == TYPEID_PLAYER)
            condMeets = ((Player*)object)->HasTitle(ConditionValue1);
        break;
    }
    case TC_CONDITION_SPAWNMASK:
    {
        condMeets = ((1 << object->GetMap()->GetSpawnMode()) & ConditionValue1);
        break;
    }
    case TC_CONDITION_POWER:
    {
        condMeets = false;
        if (object->GetTypeId() == TYPEID_UNIT ||
            object->GetTypeId() == TYPEID_PLAYER)
        {
            auto u = static_cast<Unit*>(object);
            condMeets = CompareValues(
                static_cast<ComparisionType>(ConditionValue3),
                static_cast<uint32>(
                    u->GetPower(static_cast<Powers>(ConditionValue1))),
                ConditionValue2);
        }
        break;
    }
    case TC_CONDITION_UNIT_GO_FLAG:
    {
        condMeets = false;
        if (object->GetTypeId() == TYPEID_UNIT)
        {
            auto u = static_cast<Unit*>(object);
            condMeets = u->HasFlag(UNIT_FIELD_FLAGS, ConditionValue1);
        }
        else if (object->GetTypeId() == TYPEID_GAMEOBJECT)
        {
            auto go = static_cast<GameObject*>(object);
            condMeets = go->HasFlag(GAMEOBJECT_FLAGS, ConditionValue1);
        }
        break;
    }
    case TC_CONDITION_COMBAT:
    {
        if (object->GetTypeId() == TYPEID_UNIT ||
            object->GetTypeId() == TYPEID_PLAYER)
            condMeets = ((Unit*)object)->isInCombat();
        break;
    }
    case TC_CONDITION_OBJECTIVE_COMPLETE:
    {
        if (object->GetTypeId() != TYPEID_PLAYER)
            return false; // Forcefully return false if not a player

        condMeets =
            ((Player*)object)
                ->IsQuestObjectiveComplete(ConditionValue1, ConditionValue2);
        break;
    }
    case TC_CONDITION_MOVEGEN:
    {
        if (object->GetTypeId() == TYPEID_UNIT ||
            object->GetTypeId() == TYPEID_PLAYER)
        {
            if (ConditionValue2)
                condMeets =
                    static_cast<Unit*>(object)->movement_gens.top_id() ==
                    (movement::gen)ConditionValue1;
            else
                condMeets = static_cast<Unit*>(object)->movement_gens.has(
                    (movement::gen)ConditionValue1);
        }
        break;
    }
    case TC_CONDITION_AURA_TYPE:
    {
        if (object->GetTypeId() == TYPEID_UNIT ||
            object->GetTypeId() == TYPEID_PLAYER)
        {
            condMeets = static_cast<Unit*>(object)->HasAuraType(
                (AuraType)ConditionValue1);
        }
        break;
    }
    case TC_CONDITION_MELEE_REACHABLE:
    {
        WorldObject* worldobj = sourceInfo.mConditionTargets[ConditionValue1];
        if ((object->GetTypeId() == TYPEID_UNIT ||
                object->GetTypeId() == TYPEID_PLAYER) &&
            (worldobj && (worldobj->GetTypeId() == TYPEID_UNIT ||
                             worldobj->GetTypeId() == TYPEID_PLAYER)))
        {
            condMeets = static_cast<Unit*>(object)->CanReachWithMeleeAttack(
                static_cast<Unit*>(worldobj));
        }
        break;
    }
    default:
        condMeets = false;
        break;
    }

    if (NegativeCondition)
        condMeets = !condMeets;

    if (!condMeets)
        sourceInfo.mLastFailedCondition = this;

    bool script = sScriptMgr::Instance()->OnConditionCheck(
        this, sourceInfo); // Returns true by default.
    return condMeets && script;
}

uint32 Condition::GetMaxAvailableConditionTargets() const
{
    // returns number of targets which are available for given source type
    switch (SourceType)
    {
    case CONDITION_SOURCE_TYPE_SPELL_CAST:
    case CONDITION_SOURCE_TYPE_SPELL_TARGET_SELECTION:
    case CONDITION_SOURCE_TYPE_QUEST_AVAILABLE:
    case CONDITION_SOURCE_TYPE_GOSSIP_MENU:
    case CONDITION_SOURCE_TYPE_GOSSIP_MENU_OPTION:
    case CONDITION_SOURCE_TYPE_SMART_EVENT:
    case CONDITION_SOURCE_TYPE_SMART_TARGET:
    case CONDITION_SOURCE_TYPE_NPC_VENDOR:
        return 2;
    default:
        return 1;
    }
}

ConditionMgr::ConditionMgr()
{
}

ConditionMgr::~ConditionMgr()
{
    Clean();
}

const ConditionList* ConditionMgr::GetConditionReferences(uint32 refId)
{
    auto itr = ConditionReferenceStore.find(refId);
    if (itr != ConditionReferenceStore.end())
        return &itr->second;
    return nullptr;
}

bool ConditionMgr::IsObjectMeetToConditionList(
    ConditionSourceInfo& sourceInfo, const ConditionList& conditions)
{
    // groupId, groupCheckPassed
    std::map<uint32, bool> ElseGroupStore;
    for (const auto& condition : conditions)
    {
        if ((condition).isLoaded())
        {
            // Find ElseGroup in ElseGroupStore
            std::map<uint32, bool>::const_iterator itr =
                ElseGroupStore.find((condition).ElseGroup);
            // If not found, add an entry in the store and set to true
            // (placeholder)
            if (itr == ElseGroupStore.end())
                ElseGroupStore[(condition).ElseGroup] = true;
            else if (!(*itr).second)
                continue;

            if ((condition).ReferenceId) // handle reference
            {
                ConditionReferenceContainer::const_iterator ref =
                    ConditionReferenceStore.find((condition).ReferenceId);
                if (ref != ConditionReferenceStore.end())
                {
                    if (!IsObjectMeetToConditionList(sourceInfo, (*ref).second))
                        ElseGroupStore[(condition).ElseGroup] = false;
                }
                else
                {
                    logging.error(
                        "IsPlayerMeetToConditionList: Reference template -%u "
                        "not found",
                        (condition)
                            .ReferenceId); // checked at loading, should never
                                           // happen
                }
            }
            else // handle normal condition
            {
                if (!(condition).Meets(sourceInfo))
                    ElseGroupStore[(condition).ElseGroup] = false;
            }
        }
    }

    // Logical or happens between groups, and logical and inside of groups,
    // basically:
    // if ((c1 && c2) || (c1 && c2) || (c1 && c2 && c3))
    for (std::map<uint32, bool>::const_iterator i = ElseGroupStore.begin();
         i != ElseGroupStore.end(); ++i)
        if (i->second)
            return true;

    return false;
}

bool ConditionMgr::IsObjectMeetToConditions(
    WorldObject* object, const ConditionList* conditions)
{
    ConditionSourceInfo srcInfo = ConditionSourceInfo(object);
    return IsObjectMeetToConditions(srcInfo, conditions);
}

bool ConditionMgr::IsObjectMeetToConditions(
    WorldObject* object1, WorldObject* object2, const ConditionList* conditions)
{
    ConditionSourceInfo srcInfo = ConditionSourceInfo(object1, object2);
    return IsObjectMeetToConditions(srcInfo, conditions);
}

bool ConditionMgr::IsObjectMeetToConditions(
    ConditionSourceInfo& sourceInfo, const ConditionList* conditions)
{
    if (!conditions)
        return true;

    if (conditions->empty())
        return true;

    return IsObjectMeetToConditionList(sourceInfo, *conditions);
}

bool ConditionMgr::CanHaveSourceGroupSet(ConditionSourceType sourceType) const
{
    return (sourceType == CONDITION_SOURCE_TYPE_CREATURE_LOOT_TEMPLATE ||
            sourceType == CONDITION_SOURCE_TYPE_DISENCHANT_LOOT_TEMPLATE ||
            sourceType == CONDITION_SOURCE_TYPE_FISHING_LOOT_TEMPLATE ||
            sourceType == CONDITION_SOURCE_TYPE_GAMEOBJECT_LOOT_TEMPLATE ||
            sourceType == CONDITION_SOURCE_TYPE_ITEM_LOOT_TEMPLATE ||
            sourceType == CONDITION_SOURCE_TYPE_MAIL_LOOT_TEMPLATE ||
            /*sourceType == CONDITION_SOURCE_TYPE_MILLING_LOOT_TEMPLATE ||*/
            sourceType == CONDITION_SOURCE_TYPE_PICKPOCKETING_LOOT_TEMPLATE ||
            sourceType == CONDITION_SOURCE_TYPE_PROSPECTING_LOOT_TEMPLATE ||
            sourceType == CONDITION_SOURCE_TYPE_REFERENCE_LOOT_TEMPLATE ||
            sourceType == CONDITION_SOURCE_TYPE_GOSSIP_MENU ||
            sourceType == CONDITION_SOURCE_TYPE_GOSSIP_MENU_OPTION ||
            sourceType == CONDITION_SOURCE_TYPE_SMART_EVENT ||
            sourceType == CONDITION_SOURCE_TYPE_NPC_VENDOR ||
            sourceType == CONDITION_SOURCE_TYPE_SMART_TARGET);
}

bool ConditionMgr::CanHaveSourceIdSet(ConditionSourceType sourceType) const
{
    return sourceType == CONDITION_SOURCE_TYPE_SMART_EVENT ||
           sourceType == CONDITION_SOURCE_TYPE_SMART_TARGET;
}

const ConditionList* ConditionMgr::GetLootConditions(
    ConditionSourceType type, uint32 entry, uint32 itemid)
{
    auto& tc = LootConditions[type];
    auto entry_itr = tc.find(entry);
    if (entry_itr != tc.end())
    {
        auto& ic = entry_itr->second;
        auto item_itr = ic.find(itemid);
        if (item_itr != ic.end())
            return &item_itr->second;
    }
    return nullptr;
}

const ConditionList* ConditionMgr::GetSpellCastConditions(uint32 spell_id)
{
    auto container = ConditionStore.find(CONDITION_SOURCE_TYPE_SPELL_CAST);
    if (container != ConditionStore.end())
    {
        auto conds = container->second.find(spell_id);
        if (conds != container->second.end())
            return &conds->second;
    }
    return nullptr;
}

const ConditionList* ConditionMgr::GetSpellTargetSelectionConditions(
    uint32 spell_id)
{
    auto container =
        ConditionStore.find(CONDITION_SOURCE_TYPE_SPELL_TARGET_SELECTION);
    if (container != ConditionStore.end())
    {
        auto conds = container->second.find(spell_id);
        if (conds != container->second.end())
            return &conds->second;
    }
    return nullptr;
}

const ConditionList* ConditionMgr::GetGossipMenuConditions(
    uint32 menu_id, uint32 text_id)
{
    auto menu = GossipMenuConditionStore.find(menu_id);
    if (menu != GossipMenuConditionStore.end())
    {
        auto text = menu->second.find(text_id);
        if (text != menu->second.end())
            return &text->second;
    }
    return nullptr;
}

const ConditionList* ConditionMgr::GetGossipOptionConditions(
    uint32 menu_id, uint32 option_id)
{
    auto menu = GossipOptionConditionStore.find(menu_id);
    if (menu != GossipOptionConditionStore.end())
    {
        auto option = menu->second.find(option_id);
        if (option != menu->second.end())
            return &option->second;
    }
    return nullptr;
}

const ConditionList* ConditionMgr::GetQuestAvailableConditions(uint32 quest_id)
{
    auto container = ConditionStore.find(CONDITION_SOURCE_TYPE_QUEST_AVAILABLE);
    if (container != ConditionStore.end())
    {
        auto conds = container->second.find(quest_id);
        if (conds != container->second.end())
            return &conds->second;
    }
    return nullptr;
}

const ConditionList* ConditionMgr::GetConditionsForSmartEvent(
    int32 entryOrGuid, uint32 eventId, uint32 sourceType)
{
    auto itr =
        SmartEventConditionStore.find(std::make_pair(entryOrGuid, sourceType));
    if (itr != SmartEventConditionStore.end())
    {
        auto i = (*itr).second.find(eventId + 1);
        if (i != (*itr).second.end())
            return &i->second;
    }
    return nullptr;
}

const ConditionList* ConditionMgr::GetConditionsForSmartTarget(
    int32 entryOrGuid, uint32 eventId, uint32 sourceType)
{
    auto itr =
        SmartTargetConditionStore.find(std::make_pair(entryOrGuid, sourceType));
    if (itr != SmartTargetConditionStore.end())
    {
        auto i = (*itr).second.find(eventId + 1);
        if (i != (*itr).second.end())
            return &i->second;
    }
    return nullptr;
}

const ConditionList* ConditionMgr::GetVendorItemConditions(
    uint32 creatureId, uint32 itemId)
{
    auto itr = NpcVendorConditions.find(creatureId);
    if (itr != NpcVendorConditions.end())
    {
        auto i_itr = (*itr).second.find(itemId);
        if (i_itr != (*itr).second.end())
            return &i_itr->second;
    }
    return nullptr;
}

void ConditionMgr::LoadConditions(bool /*isReload*/)
{
    Clean();

    std::unique_ptr<QueryResult> result(WorldDatabase.Query(
        "SELECT SourceTypeOrReferenceId, SourceGroup, SourceEntry, SourceId, "
        "ElseGroup, ConditionTypeOrReference, ConditionTarget, "
        " ConditionValue1, ConditionValue2, ConditionValue3, "
        "NegativeCondition, ErrorTextId, ScriptName FROM trinity_conditions"));
    if (!result)
    {
        logging.info(
            "Loaded 0 conditions. DB table `trinity_conditions` is empty!\n");
        return;
    }

    uint32 count = 0;
    BarGoLink bar(result->GetRowCount());

    do
    {
        bar.step();
        Field* fields = result->Fetch();

        Condition cond;
        int32 iSourceTypeOrReferenceId = fields[0].GetInt32();
        cond.SourceGroup = fields[1].GetUInt32();
        cond.SourceEntry = fields[2].GetInt32();
        cond.SourceId = fields[3].GetInt32();
        cond.ElseGroup = fields[4].GetUInt32();
        int32 iConditionTypeOrReference = fields[5].GetInt32();
        cond.ConditionTarget = fields[6].GetUInt8();
        cond.ConditionValue1 = fields[7].GetUInt32();
        cond.ConditionValue2 = fields[8].GetUInt32();
        cond.ConditionValue3 = fields[9].GetUInt32();
        cond.NegativeCondition = fields[10].GetUInt8();
        cond.ErrorTextId = fields[11].GetUInt32();
        cond.ScriptId =
            sScriptMgr::Instance()->GetScriptId(fields[12].GetString());

        if (iConditionTypeOrReference >= 0)
            cond.ConditionType = ConditionTypes(iConditionTypeOrReference);

        if (iSourceTypeOrReferenceId >= 0)
            cond.SourceType = ConditionSourceType(iSourceTypeOrReferenceId);

        if (iConditionTypeOrReference < 0) // it has a reference
        {
            if (iConditionTypeOrReference ==
                iSourceTypeOrReferenceId) // self referencing, skip
            {
                logging.error(
                    "Condition reference %i is referencing self, skipped",
                    iSourceTypeOrReferenceId);
                continue;
            }
            cond.ReferenceId = uint32(abs(iConditionTypeOrReference));

            const char* rowType = "reference template";
            if (iSourceTypeOrReferenceId >= 0)
                rowType = "reference";
            // check for useless data
            if (cond.ConditionTarget)
                logging.error(
                    "Condition %s %i has useless data in ConditionTarget (%u)!",
                    rowType, iSourceTypeOrReferenceId, cond.ConditionTarget);
            if (cond.ConditionValue1)
                logging.error(
                    "Condition %s %i has useless data in value1 (%u)!", rowType,
                    iSourceTypeOrReferenceId, cond.ConditionValue1);
            if (cond.ConditionValue2)
                logging.error(
                    "Condition %s %i has useless data in value2 (%u)!", rowType,
                    iSourceTypeOrReferenceId, cond.ConditionValue2);
            if (cond.ConditionValue3)
                logging.error(
                    "Condition %s %i has useless data in value3 (%u)!", rowType,
                    iSourceTypeOrReferenceId, cond.ConditionValue3);
            if (cond.NegativeCondition)
                logging.error(
                    "Condition %s %i has useless data in NegativeCondition "
                    "(%u)!",
                    rowType, iSourceTypeOrReferenceId, cond.NegativeCondition);
            if (cond.SourceGroup && iSourceTypeOrReferenceId < 0)
                logging.error(
                    "Condition %s %i has useless data in SourceGroup (%u)!",
                    rowType, iSourceTypeOrReferenceId, cond.SourceGroup);
            if (cond.SourceEntry && iSourceTypeOrReferenceId < 0)
                logging.error(
                    "Condition %s %i has useless data in SourceEntry (%u)!",
                    rowType, iSourceTypeOrReferenceId, cond.SourceEntry);
        }
        else if (!isConditionTypeValid(
                     &cond)) // doesn't have reference, validate ConditionType
        {
            continue;
        }

        if (iSourceTypeOrReferenceId < 0) // it is a reference template
        {
            uint32 uRefId = abs(iSourceTypeOrReferenceId);
            // make sure we have a list for our conditions, based on reference
            // id
            if (ConditionReferenceStore.find(uRefId) ==
                ConditionReferenceStore.end())
            {
                ConditionList mCondList;
                ConditionReferenceStore[uRefId] = mCondList;
            }
            ConditionReferenceStore[uRefId].push_back(
                cond); // add to reference storage
            count++;
            continue;
        } // end of reference templates

        // if not a reference and SourceType is invalid, skip
        if (iConditionTypeOrReference >= 0 && !isSourceTypeValid(&cond))
            continue;

        // Grouping is only allowed for some types (loot templates, gossip
        // menus, gossip items)
        if (cond.SourceGroup && !CanHaveSourceGroupSet(cond.SourceType))
        {
            logging.error(
                "Condition type %u has not allowed value of SourceGroup = %u!",
                uint32(cond.SourceType), cond.SourceGroup);
            continue;
        }
        if (cond.SourceId && !CanHaveSourceIdSet(cond.SourceType))
        {
            logging.error(
                "Condition type %u has not allowed value of SourceId = %u!",
                uint32(cond.SourceType), cond.SourceId);
            continue;
        }

        if (cond.SourceGroup)
        {
            // handle grouped conditions
            switch (cond.SourceType)
            {
            case CONDITION_SOURCE_TYPE_CREATURE_LOOT_TEMPLATE:
            case CONDITION_SOURCE_TYPE_DISENCHANT_LOOT_TEMPLATE:
            case CONDITION_SOURCE_TYPE_FISHING_LOOT_TEMPLATE:
            case CONDITION_SOURCE_TYPE_GAMEOBJECT_LOOT_TEMPLATE:
            case CONDITION_SOURCE_TYPE_ITEM_LOOT_TEMPLATE:
            case CONDITION_SOURCE_TYPE_MAIL_LOOT_TEMPLATE:
            case CONDITION_SOURCE_TYPE_PICKPOCKETING_LOOT_TEMPLATE:
            case CONDITION_SOURCE_TYPE_PROSPECTING_LOOT_TEMPLATE:
            case CONDITION_SOURCE_TYPE_REFERENCE_LOOT_TEMPLATE:
            case CONDITION_SOURCE_TYPE_SKINNING_LOOT_TEMPLATE:
            {
                LootConditions[cond.SourceType][cond.SourceGroup]
                              [cond.SourceEntry].push_back(cond);
                ++count;
                break;
            }
            case CONDITION_SOURCE_TYPE_GOSSIP_MENU:
            {
                GossipMenuConditionStore[cond.SourceGroup][cond.SourceEntry]
                    .push_back(cond);
                ++count;
                break;
            }
            case CONDITION_SOURCE_TYPE_GOSSIP_MENU_OPTION:
            {
                GossipOptionConditionStore[cond.SourceGroup][cond.SourceEntry]
                    .push_back(cond);
                ++count;
                break;
            }
            case CONDITION_SOURCE_TYPE_SMART_EVENT:
            {
                std::pair<int32, uint32> key =
                    std::make_pair(cond.SourceEntry, cond.SourceId);
                SmartEventConditionStore[key][cond.SourceGroup].push_back(cond);
                ++count;
                break;
            }
            case CONDITION_SOURCE_TYPE_SMART_TARGET:
            {
                std::pair<int32, uint32> key =
                    std::make_pair(cond.SourceEntry, cond.SourceId);
                SmartTargetConditionStore[key][cond.SourceGroup].push_back(
                    cond);
                ++count;
                break;
            }
            case CONDITION_SOURCE_TYPE_NPC_VENDOR:
            {
                NpcVendorConditions[cond.SourceGroup][cond.SourceEntry]
                    .push_back(cond);
                ++count;
                break;
            }
            default:
                logging.error("Not handled grouped condition, SourceGroup %u",
                    cond.SourceGroup);
                break;
            }
            continue; // condition is grouped and added to a container, skip
                      // rest
        }

        // handle not grouped conditions
        // make sure we have a storage list for our SourceType
        if (ConditionStore.find(cond.SourceType) == ConditionStore.end())
        {
            ConditionTypeContainer mTypeMap;
            ConditionStore[cond.SourceType] =
                mTypeMap; // add new empty list for SourceType
        }

        // make sure we have a condition list for our SourceType's entry
        if (ConditionStore[cond.SourceType].find(cond.SourceEntry) ==
            ConditionStore[cond.SourceType].end())
        {
            ConditionList mCondList;
            ConditionStore[cond.SourceType][cond.SourceEntry] = mCondList;
        }

        // add new Condition to storage based on Type/Entry
        ConditionStore[cond.SourceType][cond.SourceEntry].push_back(cond);
        ++count;
    } while (result->NextRow());

    logging.info("Loaded %u conditions\n", count);
}

bool ConditionMgr::isSourceTypeValid(Condition* cond)
{
    if (cond->SourceType == CONDITION_SOURCE_TYPE_NONE ||
        cond->SourceType >= CONDITION_SOURCE_TYPE_MAX)
    {
        logging.error(
            "Invalid ConditionSourceType %u in `trinity_conditions` table, "
            "ignoring.",
            uint32(cond->SourceType));
        return false;
    }

    switch (cond->SourceType)
    {
    case CONDITION_SOURCE_TYPE_NPC_VENDOR:
    {
        if (!sObjectMgr::Instance()->GetCreatureTemplate(cond->SourceGroup))
        {
            logging.error(
                "SourceGroup %u in `trinity_conditions` table, does not exist "
                "in `creature_template`, ignoring.",
                cond->SourceGroup);
            return false;
        }
        if (!sObjectMgr::Instance()->GetItemPrototype(cond->SourceEntry))
        {
            logging.error(
                "SourceEntry %u in `trinity_conditions` table, does not exist "
                "in `item_template`, ignoring.",
                cond->SourceEntry);
            return false;
        }
        break;
    }
    case CONDITION_SOURCE_TYPE_QUEST_AVAILABLE:
    {
        if (!sObjectMgr::Instance()->GetQuestTemplate(cond->SourceEntry))
        {
            logging.error(
                "SourceEntry %u in `trinity_conditions` table, does not exist "
                "in `quest_template`, ignoring.",
                cond->SourceEntry);
            return false;
        }
        if (cond->SourceGroup != 0)
        {
            logging.error(
                "SourceEntry %u in `trinity_conditions` table has specified "
                "data in ignored columns, ignoring condition.",
                cond->SourceEntry);
            return false;
        }
        break;
    }
    case CONDITION_SOURCE_TYPE_SPELL_CAST:
    case CONDITION_SOURCE_TYPE_SPELL_TARGET_SELECTION:
    {
        if (!sSpellStore.LookupEntry(cond->SourceEntry))
        {
            logging.error(
                "SourceEntry %u in `trinity_conditions` table, does not exist "
                "in `spell_dbc`, ignoring.",
                cond->SourceEntry);
            return false;
        }
        break;
    }
    case CONDITION_SOURCE_TYPE_CREATURE_LOOT_TEMPLATE:
    case CONDITION_SOURCE_TYPE_DISENCHANT_LOOT_TEMPLATE:
    case CONDITION_SOURCE_TYPE_FISHING_LOOT_TEMPLATE:
    case CONDITION_SOURCE_TYPE_GAMEOBJECT_LOOT_TEMPLATE:
    case CONDITION_SOURCE_TYPE_ITEM_LOOT_TEMPLATE:
    case CONDITION_SOURCE_TYPE_MAIL_LOOT_TEMPLATE:
    case CONDITION_SOURCE_TYPE_PICKPOCKETING_LOOT_TEMPLATE:
    case CONDITION_SOURCE_TYPE_PROSPECTING_LOOT_TEMPLATE:
    case CONDITION_SOURCE_TYPE_REFERENCE_LOOT_TEMPLATE:
    case CONDITION_SOURCE_TYPE_SKINNING_LOOT_TEMPLATE:
    case CONDITION_SOURCE_TYPE_SMART_EVENT:
    case CONDITION_SOURCE_TYPE_GOSSIP_MENU:
    case CONDITION_SOURCE_TYPE_GOSSIP_MENU_OPTION:
    case CONDITION_SOURCE_TYPE_SMART_TARGET:
        break;
    default:
        logging.error(
            "SourceEntry %u in `trinity_conditions` table, uses a currently "
            "unimplemented ConditionSourceType.",
            cond->SourceGroup);
        return false;
    }

    return true;
}
bool ConditionMgr::isConditionTypeValid(Condition* cond)
{
    if (cond->ConditionType == TC_CONDITION_NONE ||
        cond->ConditionType >= TC_CONDITION_MAX)
    {
        logging.error(
            "Invalid ConditionType %u at SourceEntry %u in "
            "`trinity_conditions` table, ignoring.",
            uint32(cond->ConditionType), cond->SourceEntry);
        return false;
    }

    if (cond->ConditionTarget >= cond->GetMaxAvailableConditionTargets())
    {
        logging.error(
            "SourceType %u, SourceEntry %u in `trinity_conditions` table, has "
            "incorrect ConditionTarget set, ignoring.",
            cond->SourceType, cond->SourceEntry);
        return false;
    }

    switch (cond->ConditionType)
    {
    case TC_CONDITION_AURA:
    {
        if (!sSpellStore.LookupEntry(cond->ConditionValue1))
        {
            logging.error(
                "Aura condition has non existing spell (Id: %d), skipped",
                cond->ConditionValue1);
            return false;
        }

        if (cond->ConditionValue3)
            logging.error("Aura condition has useless data in value3 (%u)!",
                cond->ConditionValue3);
        break;
    }
    case TC_CONDITION_ITEM:
    {
        ItemPrototype const* proto =
            sObjectMgr::Instance()->GetItemPrototype(cond->ConditionValue1);
        if (!proto)
        {
            logging.error("Item condition has non existing item (%u), skipped",
                cond->ConditionValue1);
            return false;
        }

        if (!cond->ConditionValue2)
        {
            logging.error(
                "Item condition has 0 set for item count in value2 (%u), "
                "skipped",
                cond->ConditionValue2);
            return false;
        }
        break;
    }
    case TC_CONDITION_ITEM_EQUIPPED:
    {
        ItemPrototype const* proto =
            sObjectMgr::Instance()->GetItemPrototype(cond->ConditionValue1);
        if (!proto)
        {
            logging.error(
                "ItemEquipped condition has non existing item (%u), skipped",
                cond->ConditionValue1);
            return false;
        }

        if (cond->ConditionValue2)
            logging.error(
                "ItemEquipped condition has useless data in value2 (%u)!",
                cond->ConditionValue2);
        if (cond->ConditionValue3)
            logging.error(
                "ItemEquipped condition has useless data in value3 (%u)!",
                cond->ConditionValue3);
        break;
    }
    case TC_CONDITION_ZONEID:
    {
        AreaTableEntry const* areaEntry =
            GetAreaEntryByAreaID(cond->ConditionValue1);
        if (!areaEntry)
        {
            logging.error(
                "ZoneID condition has non existing area (%u), skipped",
                cond->ConditionValue1);
            return false;
        }

        if (areaEntry->zone != 0)
        {
            logging.error(
                "ZoneID condition requires to be in area (%u) which is a "
                "subzone but zone expected, skipped",
                cond->ConditionValue1);
            return false;
        }

        if (cond->ConditionValue2)
            logging.error("ZoneID condition has useless data in value2 (%u)!",
                cond->ConditionValue2);
        if (cond->ConditionValue3)
            logging.error("ZoneID condition has useless data in value3 (%u)!",
                cond->ConditionValue3);
        break;
    }
    case TC_CONDITION_REPUTATION_RANK:
    {
        FactionEntry const* factionEntry =
            sFactionStore.LookupEntry(cond->ConditionValue1);
        if (!factionEntry)
        {
            logging.error(
                "Reputation condition has non existing faction (%u), skipped",
                cond->ConditionValue1);
            return false;
        }
        if (cond->ConditionValue3)
            logging.error(
                "Reputation condition has useless data in value3 (%u)!",
                cond->ConditionValue3);
        break;
    }
    case TC_CONDITION_TEAM:
    {
        if (cond->ConditionValue1 != ALLIANCE && cond->ConditionValue1 != HORDE)
        {
            logging.error("Team condition specifies unknown team (%u), skipped",
                cond->ConditionValue1);
            return false;
        }

        if (cond->ConditionValue2)
            logging.error("Team condition has useless data in value2 (%u)!",
                cond->ConditionValue2);
        if (cond->ConditionValue3)
            logging.error("Team condition has useless data in value3 (%u)!",
                cond->ConditionValue3);
        break;
    }
    case TC_CONDITION_SKILL:
    {
        SkillLineEntry const* pSkill =
            sSkillLineStore.LookupEntry(cond->ConditionValue1);
        if (!pSkill)
        {
            logging.error(
                "Skill condition specifies non-existing skill (%u), skipped",
                cond->ConditionValue1);
            return false;
        }

        if (cond->ConditionValue2 < 1 ||
            cond->ConditionValue2 >
                sWorld::Instance()->GetConfigMaxSkillValue())
        {
            logging.error(
                "Skill condition specifies invalid skill value (%u), skipped",
                cond->ConditionValue2);
            return false;
        }
        if (cond->ConditionValue3)
            logging.error("Skill condition has useless data in value3 (%u)!",
                cond->ConditionValue3);
        break;
    }
    case TC_CONDITION_QUESTREWARDED:
    case TC_CONDITION_QUESTTAKEN:
    case TC_CONDITION_QUEST_NONE:
    case TC_CONDITION_QUEST_COMPLETE:
    {
        if (!sObjectMgr::Instance()->GetQuestTemplate(cond->ConditionValue1))
        {
            logging.error(
                "Quest condition specifies non-existing quest (%u), skipped",
                cond->ConditionValue1);
            return false;
        }

        if (cond->ConditionValue2 > 1)
            logging.error("Quest condition has useless data in value2 (%u)!",
                cond->ConditionValue2);
        if (cond->ConditionValue3)
            logging.error("Quest condition has useless data in value3 (%u)!",
                cond->ConditionValue3);
        break;
    }
    case TC_CONDITION_ACTIVE_EVENT:
    {
        GameEventMgr::GameEventDataMap const& events =
            sGameEventMgr::Instance()->GetEventMap();
        if (cond->ConditionValue1 >= events.size() ||
            !events[cond->ConditionValue1].isValid())
        {
            logging.error(
                "ActiveEvent condition has non existing event id (%u), skipped",
                cond->ConditionValue1);
            return false;
        }

        if (cond->ConditionValue2)
            logging.error(
                "ActiveEvent condition has useless data in value2 (%u)!",
                cond->ConditionValue2);
        if (cond->ConditionValue3)
            logging.error(
                "ActiveEvent condition has useless data in value3 (%u)!",
                cond->ConditionValue3);
        break;
    }
    case TC_CONDITION_ACHIEVEMENT:
    {
        return false;
        /*AchievementEntry const* achievement =
        sAchievementStore.LookupEntry(cond->ConditionValue1);
        if (!achievement)
        {
            logging.error("Achivement condition has non existing
        achivement id (%u), skipped", cond->ConditionValue1);
            return false;
        }

        if (cond->ConditionValue2)
            logging.error("Achivement condition has useless data
        in value2 (%u)!", cond->ConditionValue2);
        if (cond->ConditionValue3)
            logging.error("Achivement condition has useless data
        in value3 (%u)!", cond->ConditionValue3);*/
        break;
    }
    case TC_CONDITION_CLASS:
    {
        if (!(cond->ConditionValue1 & CLASSMASK_ALL_PLAYABLE))
        {
            logging.error(
                "Class condition has non existing classmask (%u), skipped",
                cond->ConditionValue1 & ~CLASSMASK_ALL_PLAYABLE);
            return false;
        }

        if (cond->ConditionValue2)
            logging.error("Class condition has useless data in value2 (%u)!",
                cond->ConditionValue2);
        if (cond->ConditionValue3)
            logging.error("Class condition has useless data in value3 (%u)!",
                cond->ConditionValue3);
        break;
    }
    case TC_CONDITION_RACE:
    {
        if (!(cond->ConditionValue1 & RACEMASK_ALL_PLAYABLE))
        {
            logging.error(
                "Race condition has non existing racemask (%u), skipped",
                cond->ConditionValue1 & ~RACEMASK_ALL_PLAYABLE);
            return false;
        }

        if (cond->ConditionValue2)
            logging.error("Race condition has useless data in value2 (%u)!",
                cond->ConditionValue2);
        if (cond->ConditionValue3)
            logging.error("Race condition has useless data in value3 (%u)!",
                cond->ConditionValue3);
        break;
    }
    case TC_CONDITION_MAPID:
    {
        MapEntry const* me = sMapStore.LookupEntry(cond->ConditionValue1);
        if (!me)
        {
            logging.error("Map condition has non existing map (%u), skipped",
                cond->ConditionValue1);
            return false;
        }

        if (cond->ConditionValue2)
            logging.error("Map condition has useless data in value2 (%u)!",
                cond->ConditionValue2);
        if (cond->ConditionValue3)
            logging.error("Map condition has useless data in value3 (%u)!",
                cond->ConditionValue3);
        break;
    }
    case TC_CONDITION_SPELL:
    {
        if (!sSpellStore.LookupEntry(cond->ConditionValue1))
        {
            logging.error(
                "Spell condition has non existing spell (Id: %d), skipped",
                cond->ConditionValue1);
            return false;
        }

        if (cond->ConditionValue2)
            logging.error("Spell condition has useless data in value2 (%u)!",
                cond->ConditionValue2);
        if (cond->ConditionValue3)
            logging.error("Spell condition has useless data in value3 (%u)!",
                cond->ConditionValue3);
        break;
    }
    case TC_CONDITION_LEVEL:
    {
        if (cond->ConditionValue2 >= COMP_TYPE_MAX)
        {
            logging.error("Level condition has invalid option (%u), skipped",
                cond->ConditionValue2);
            return false;
        }
        if (cond->ConditionValue3)
            logging.error("Level condition has useless data in value3 (%u)!",
                cond->ConditionValue3);
        break;
    }
    case TC_CONDITION_DRUNKENSTATE:
    {
        if (cond->ConditionValue1 > DRUNKEN_SMASHED)
        {
            logging.error(
                "DrunkState condition has invalid state (%u), skipped",
                cond->ConditionValue1);
            return false;
        }
        if (cond->ConditionValue2)
        {
            logging.error(
                "DrunkState condition has useless data in value2 (%u)!",
                cond->ConditionValue2);
            return false;
        }
        if (cond->ConditionValue3)
            logging.error(
                "DrunkState condition has useless data in value3 (%u)!",
                cond->ConditionValue3);
        break;
    }
    case TC_CONDITION_NEAR_CREATURE:
    {
        if (!sObjectMgr::Instance()->GetCreatureTemplate(cond->ConditionValue1))
        {
            logging.error(
                "NearCreature condition has non existing creature template "
                "entry (%u), skipped",
                cond->ConditionValue1);
            return false;
        }
        if (cond->ConditionValue3)
            logging.error(
                "NearCreature condition has useless data in value3 (%u)!",
                cond->ConditionValue3);
        break;
    }
    case TC_CONDITION_NEAR_GAMEOBJECT:
    {
        if (!sObjectMgr::Instance()->GetGameObjectInfo(cond->ConditionValue1))
        {
            logging.error(
                "NearGameObject condition has non existing gameobject template "
                "entry (%u), skipped",
                cond->ConditionValue1);
            return false;
        }
        if (cond->ConditionValue3)
            logging.error(
                "NearGameObject condition has useless data in value3 (%u)!",
                cond->ConditionValue3);
        break;
    }
    case TC_CONDITION_OBJECT_ENTRY:
    {
        switch (cond->ConditionValue1)
        {
        case TYPEID_UNIT:
            if (cond->ConditionValue2 &&
                !sObjectMgr::Instance()->GetCreatureTemplate(
                    cond->ConditionValue2))
            {
                logging.error(
                    "ObjectEntry condition has non existing creature template "
                    "entry  (%u), skipped",
                    cond->ConditionValue2);
                return false;
            }
            break;
        case TYPEID_GAMEOBJECT:
            if (cond->ConditionValue2 &&
                !sObjectMgr::Instance()->GetGameObjectInfo(
                    cond->ConditionValue2))
            {
                logging.error(
                    "ObjectEntry condition has non existing game object "
                    "template entry  (%u), skipped",
                    cond->ConditionValue2);
                return false;
            }
            break;
        case TYPEID_PLAYER:
        case TYPEID_CORPSE:
            if (cond->ConditionValue2)
                logging.error(
                    "ObjectEntry condition has useless data in value2 (%u)!",
                    cond->ConditionValue2);
            break;
        default:
            logging.error(
                "ObjectEntry condition has wrong typeid set (%u), skipped",
                cond->ConditionValue1);
            return false;
        }
        if (cond->ConditionValue3)
            logging.error(
                "ObjectEntry condition has useless data in value3 (%u)!",
                cond->ConditionValue3);
        break;
    }
    case TC_CONDITION_TYPE_MASK:
    {
        if (!cond->ConditionValue1 ||
            (cond->ConditionValue1 &
                ~(TYPEMASK_UNIT | TYPEMASK_PLAYER | TYPEMASK_GAMEOBJECT |
                    TYPEMASK_CORPSE)))
        {
            logging.error(
                "TypeMask condition has invalid typemask set (%u), skipped",
                cond->ConditionValue2);
            return false;
        }
        if (cond->ConditionValue2)
            logging.error("TypeMask condition has useless data in value2 (%u)!",
                cond->ConditionValue2);
        if (cond->ConditionValue3)
            logging.error("TypeMask condition has useless data in value3 (%u)!",
                cond->ConditionValue3);
        break;
    }
    case TC_CONDITION_RELATION_TO:
    {
        if (cond->ConditionValue1 >= cond->GetMaxAvailableConditionTargets())
        {
            logging.error(
                "RelationTo condition has invalid "
                "ConditionValue1(ConditionTarget selection) (%u), skipped",
                cond->ConditionValue1);
            return false;
        }
        if (cond->ConditionValue1 == cond->ConditionTarget)
        {
            logging.error(
                "RelationTo condition has ConditionValue1(ConditionTarget "
                "selection) set to self (%u), skipped",
                cond->ConditionValue1);
            return false;
        }
        if (cond->ConditionValue2 >= RELATION_MAX)
        {
            logging.error(
                "RelationTo condition has invalid "
                "ConditionValue2(RelationType) (%u), skipped",
                cond->ConditionValue2);
            return false;
        }
        if (cond->ConditionValue3)
            logging.error(
                "RelationTo condition has useless data in value3 (%u)!",
                cond->ConditionValue3);
        break;
    }
    case TC_CONDITION_REACTION_TO:
    {
        if (cond->ConditionValue1 >= cond->GetMaxAvailableConditionTargets())
        {
            logging.error(
                "ReactionTo condition has invalid "
                "ConditionValue1(ConditionTarget selection) (%u), skipped",
                cond->ConditionValue1);
            return false;
        }
        if (cond->ConditionValue1 == cond->ConditionTarget)
        {
            logging.error(
                "ReactionTo condition has ConditionValue1(ConditionTarget "
                "selection) set to self (%u), skipped",
                cond->ConditionValue1);
            return false;
        }
        if (!cond->ConditionValue2)
        {
            logging.error(
                "mConditionValue2 condition has invalid "
                "ConditionValue2(rankMask) (%u), skipped",
                cond->ConditionValue2);
            return false;
        }
        break;
    }
    case TC_CONDITION_DISTANCE_TO:
    {
        if (cond->ConditionValue1 >= cond->GetMaxAvailableConditionTargets())
        {
            logging.error(
                "DistanceTo condition has invalid "
                "ConditionValue1(ConditionTarget selection) (%u), skipped",
                cond->ConditionValue1);
            return false;
        }
        if (cond->ConditionValue1 == cond->ConditionTarget)
        {
            logging.error(
                "DistanceTo condition has ConditionValue1(ConditionTarget "
                "selection) set to self (%u), skipped",
                cond->ConditionValue1);
            return false;
        }
        if (cond->ConditionValue3 >= COMP_TYPE_MAX)
        {
            logging.error(
                "DistanceTo condition has invalid ComparisionType (%u), "
                "skipped",
                cond->ConditionValue3);
            return false;
        }
        break;
    }
    case TC_CONDITION_ALIVE:
    {
        if (cond->ConditionValue3)
            logging.error("Alive condition has useless data in value3 (%u)!",
                cond->ConditionValue3);
        break;
    }
    case TC_CONDITION_HP_VAL:
    {
        if (cond->ConditionValue2 >= COMP_TYPE_MAX)
        {
            logging.error(
                "HpVal condition has invalid ComparisionType (%u), skipped",
                cond->ConditionValue2);
            return false;
        }
        if (cond->ConditionValue3)
            logging.error("HpVal condition has useless data in value3 (%u)!",
                cond->ConditionValue3);
        break;
    }
    case TC_CONDITION_HP_PCT:
    {
        if (cond->ConditionValue1 > 100)
        {
            logging.error(
                "HpPct condition has too big percent value (%u), skipped",
                cond->ConditionValue1);
            return false;
        }
        if (cond->ConditionValue2 >= COMP_TYPE_MAX)
        {
            logging.error(
                "HpPct condition has invalid ComparisionType (%u), skipped",
                cond->ConditionValue2);
            return false;
        }
        if (cond->ConditionValue3)
            logging.error("HpPct condition has useless data in value3 (%u)!",
                cond->ConditionValue3);
        break;
    }
    case TC_CONDITION_AREAID:
    case TC_CONDITION_INSTANCE_DATA:
        break;
    case TC_CONDITION_WORLD_STATE:
    {
        return false;
        /*if (!sWorld::Instance()->getWorldState(cond->ConditionValue1))
        {
            logging.error("World state condition has non existing
        world state in value1 (%u), skipped", cond->ConditionValue1);
            return false;
        }
        if (cond->ConditionValue3)
            logging.error("World state condition has useless data
        in value3 (%u)!", cond->ConditionValue3);*/
        break;
    }
    case TC_CONDITION_PHASEMASK:
    {
        if (cond->ConditionValue2)
            logging.error(
                "Phasemask condition has useless data in value2 (%u)!",
                cond->ConditionValue2);
        if (cond->ConditionValue3)
            logging.error(
                "Phasemask condition has useless data in value3 (%u)!",
                cond->ConditionValue3);
        break;
    }
    case TC_CONDITION_TITLE:
    {
        CharTitlesEntry const* titleEntry =
            sCharTitlesStore.LookupEntry(cond->ConditionValue1);
        if (!titleEntry)
        {
            logging.error(
                "Title condition has non existing title in value1 (%u), "
                "skipped",
                cond->ConditionValue1);
            return false;
        }
        break;
    }
    case TC_CONDITION_SPAWNMASK:
    {
        if (cond->ConditionValue1 >
            SPAWNMASK_DUNGEON_ALL) // SPAWNMASK_RAID_ALL for 3.3.3
        {
            logging.error(
                "SpawnMask condition has non existing SpawnMask in value1 "
                "(%u), skipped",
                cond->ConditionValue1);
            return false;
        }
        break;
    }
    case TC_CONDITION_POWER:
    {
        if (cond->ConditionValue1 >= MAX_POWERS)
        {
            logging.error(
                "Power condition has non existing Power in value1 (%u), "
                "skipped",
                cond->ConditionValue1);
            return false;
        }
        if (cond->ConditionValue3 >= COMP_TYPE_MAX)
        {
            logging.error(
                "Power condition has invalid ComparisionType (%u), "
                "skipped",
                cond->ConditionValue3);
            return false;
        }
        break;
    }
    case TC_CONDITION_COMBAT:
    {
        if (cond->ConditionValue1)
            logging.error("Combat condition has useless data in value1 (%u)!",
                cond->ConditionValue1);
        if (cond->ConditionValue2)
            logging.error("Combat condition has useless data in value2 (%u)!",
                cond->ConditionValue2);
        if (cond->ConditionValue3)
            logging.error("Combat condition has useless data in value3 (%u)!",
                cond->ConditionValue3);
        break;
    }
    case TC_CONDITION_OBJECTIVE_COMPLETE:
    {
        if (!sObjectMgr::Instance()->GetQuestTemplate(cond->ConditionValue1))
        {
            logging.error(
                "ObjectiveComplete condition has invalid quest id (%u)! "
                "Skipped!",
                cond->ConditionValue1);
            return false;
        }
        if (cond->ConditionValue2 > 3)
        {
            logging.error(
                "ObjectiveComplete condition has invalid objective index (%u)! "
                "Skipped!",
                cond->ConditionValue2);
            return false;
        }
        if (cond->ConditionValue3)
            logging.error(
                "ObjectiveComplete condition has useless data in value3 (%u)!",
                cond->ConditionValue3);
        break;
    }
    case TC_CONDITION_MOVEGEN:
    {
        if (cond->ConditionValue1 >= (int)movement::gen::max)
        {
            logging.error(
                "Condition movegen condition has invalid gen type (%u)! "
                "Skipped!",
                cond->ConditionValue1);
            return false;
        }
        break;
    }
    case TC_CONDITION_AURA_TYPE:
    {
        if (cond->ConditionValue1 >= TOTAL_AURAS ||
            cond->ConditionValue1 == SPELL_AURA_NONE)
        {
            logging.error(
                "Condition aura type condition has invalid aura type (%u)! "
                "Skipped!",
                cond->ConditionValue1);
            return false;
        }
        break;
    }
    case TC_CONDITION_MELEE_REACHABLE:
    {
        if (cond->ConditionValue1 >= cond->GetMaxAvailableConditionTargets())
        {
            logging.error(
                "MeleeReachable condition has invalid "
                "ConditionValue1(ConditionTarget selection) (%u), skipped",
                cond->ConditionValue1);
            return false;
        }
        break;
    }
    case TC_CONDITION_UNUSED_24:
        logging.error(
            "Found ConditionTypeOrReference = TC_CONDITION_UNUSED_24 in "
            "`conditions` table - ignoring");
        return false;
    default:
        break;
    }
    return true;
}

void ConditionMgr::Clean()
{
    ConditionReferenceStore.clear();
    ConditionStore.clear();
    GossipMenuConditionStore.clear();
    GossipOptionConditionStore.clear();
    SmartEventConditionStore.clear();
    SmartTargetConditionStore.clear();
    NpcVendorConditions.clear();
}
