/*
 * Copyright (C) 2008-2012 TrinityCore <http://www.trinitycore.org/>
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

#ifndef TRINITY_SMARTSCRIPT_H
#define TRINITY_SMARTSCRIPT_H

#include "Common.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "SmartScriptMgr.h"
#include "Spell.h"
#include "Unit.h"
#include <map>
#include <unordered_map>

class Map;

class SmartScript
{
public:
    SmartScript();
    ~SmartScript();

    void OnInitialize(WorldObject* obj, AreaTriggerEntry const* at = nullptr);
    void GetScript();
    void FillScript(
        SmartAIEventList e, WorldObject* obj, AreaTriggerEntry const* at);

    bool ProcessEventsFor(SMART_EVENT e, Unit* unit = nullptr, uint32 var0 = 0,
        uint32 var1 = 0, bool bvar = false, const SpellEntry* spell = nullptr,
        GameObject* gob = nullptr);
    bool ProcessEvent(SmartScriptHolder& e, Unit* unit = nullptr,
        uint32 var0 = 0, uint32 var1 = 0, bool bvar = false,
        const SpellEntry* spell = nullptr, GameObject* gob = nullptr);
    bool CheckTimer(SmartScriptHolder const& e) const;
    void RecalcTimer(SmartScriptHolder& e, uint32 min, uint32 max);
    void UpdateTimer(SmartScriptHolder& e, uint32 const diff);
    void InitTimer(SmartScriptHolder& e);
    void ProcessAction(SmartScriptHolder& e, Unit* unit = nullptr,
        uint32 var0 = 0, uint32 var1 = 0, bool bvar = false,
        const SpellEntry* spell = nullptr, GameObject* gob = nullptr);
    ObjectList GetTargets(SmartScriptHolder const& e, Unit* invoker = nullptr);
    SmartScriptHolder CreateEvent(SMART_EVENT e, uint32 event_flags,
        uint32 event_param1, uint32 event_param2, uint32 event_param3,
        uint32 event_param4, SMART_ACTION action, uint32 action_param1,
        uint32 action_param2, uint32 action_param3, uint32 action_param4,
        uint32 action_param5, uint32 action_param6, SMARTAI_TARGETS t,
        uint32 target_param1, uint32 target_param2, uint32 target_param3,
        uint32 target_param4, uint32 target_param5, uint32 phaseMask = 0);
    void AddEvent(SMART_EVENT e, uint32 event_flags, uint32 event_param1,
        uint32 event_param2, uint32 event_param3, uint32 event_param4,
        SMART_ACTION action, uint32 action_param1, uint32 action_param2,
        uint32 action_param3, uint32 action_param4, uint32 action_param5,
        uint32 action_param6, SMARTAI_TARGETS t, uint32 target_param1,
        uint32 target_param2, uint32 target_param3, uint32 phaseMask = 0);
    void SetPathId(uint32 id) { mPathId = id; }
    uint32 GetPathId() const { return mPathId; }
    WorldObject* GetBaseObject()
    {
        WorldObject* obj = nullptr;
        if (me)
            obj = me;
        else if (go)
            obj = go;
        return obj;
    }

    bool IsUnit(WorldObject* obj)
    {
        return obj && (obj->GetTypeId() == TYPEID_UNIT ||
                          obj->GetTypeId() == TYPEID_PLAYER);
    }

    bool IsPlayer(WorldObject* obj)
    {
        return obj && obj->GetTypeId() == TYPEID_PLAYER;
    }

    bool IsCreature(WorldObject* obj)
    {
        return obj && obj->GetTypeId() == TYPEID_UNIT;
    }

    bool IsGameObject(WorldObject* obj)
    {
        return obj && obj->GetTypeId() == TYPEID_GAMEOBJECT;
    }

    void OnUpdate(const uint32 diff);
    void OnMoveInLineOfSight(Unit* who);

    bool GetTargetPosition(
        SmartScriptHolder& e, float& x, float& y, float& z, float& o);

    std::vector<Creature*> DoFindFriendlyCC(float range);
    std::vector<Creature*> DoFindFriendlyMissingBuff(
        float range, uint32 spellid);

    void StoreTargets(const ObjectList& targets, uint32 id);
    ObjectList GetStoredTargets(uint32 id);

    bool IsSmart(Creature* c = nullptr);
    bool IsSmartGO(GameObject* g = nullptr);

    void OnReset(int type_mask);

    // TIMED_ACTIONLIST (script type 9 aka script9)
    void SetScript9(SmartScriptHolder& e, uint32 entry);
    Unit* GetLastInvoker();
    ObjectGuid mLastInvoker;

    uint32 GetPhase() const { return mEventPhase; }
    const SmartAIEventList& GetEvents() const { return mEvents; }
    const SmartAIEventList& GetTimedActionList() const
    {
        return mTimedActionList;
    }

private:
    bool IsInPhase(uint32 p) const { return (1 << (mEventPhase - 1)) & p; }
    void SaveCurrentPhase(uint32 id)
    {
        mSavedPhases[id] = mNewPhase != -1 ? mNewPhase : mEventPhase;
    }
    void LoadSavedPhase(uint32 id)
    {
        if (mSavedPhases.find(id) != mSavedPhases.end())
            mNewPhase = mSavedPhases[id];
    }

    void _SetEventPhase(uint32 phase);

    SmartAIEventList mEvents;
    SmartAIEventList mInstallEvents;
    SmartAIEventList mTimedActionList;
    Creature* me;
    GameObject* go;
    AreaTriggerEntry const* area_trigger;
    SmartScriptType mScriptType;
    uint32 mEventPhase;
    int mNewPhase;
    std::map<uint32 /*identifier*/, uint32 /*phase*/> mSavedPhases;

    std::unordered_map<int32, int32> mStoredDecimals;
    uint32 mPathId;
    SmartAIEventList mStoredEvents;
    std::list<uint32> mRemIDs;

    uint32 mTextTimer;
    uint32 mLastTextID;
    ObjectGuid mTextGUID;
    uint32 mTalkerEntry;
    uint32 mKillSayCooldown;
    bool mUseTextTimer;

    StoredTargetsMap mStoredTargets;
    std::vector<ObjectGuid> mNoneSelectedTargets;

    void InstallEvents();

    void RemoveStoredEvent(uint32 id)
    {
        if (!mStoredEvents.empty())
        {
            for (auto i = mStoredEvents.begin(); i != mStoredEvents.end(); ++i)
            {
                if (i->event_id == id)
                {
                    mStoredEvents.erase(i);
                    return;
                }
            }
        }
    }
    SmartScriptHolder* FindLinkedEvent(uint32 link)
    {
        for (auto& elem : mEvents)
            if (elem.event_id == link)
                return &elem;
        return nullptr;
    }
};

#endif
