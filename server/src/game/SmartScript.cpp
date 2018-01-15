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

#include "SmartScript.h"
#include "ConditionMgr.h"
#include "CreatureEventAI.h"
#include "CreatureTextMgr.h"
#include "DynamicObject.h"
#include "GameObjectAI.h"
#include "GossipDef.h"
#include "Group.h"
#include "InstanceData.h"
#include "Language.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "movement/IdleMovementGenerator.h"
#include "movement/PointMovementGenerator.h"
#include "movement/RandomMovementGenerator.h"
#include "movement/TargetedMovementGenerator.h"
#include "movement/WaypointManager.h"
#include "movement/WaypointMovementGenerator.h"
#include "SmartAI.h"
#include "SpellMgr.h"
#include "SpecialVisCreature.h"
#include "TemporarySummon.h"
#include "Totem.h"
#include "loot_distributor.h"
#include "pet_behavior.h"
#include "Database/DatabaseEnv.h"
#include "maps/checks.h"
#include "maps/map_grid.h"

class StringTextBuilder
{
public:
    StringTextBuilder(WorldObject* obj, ChatMsg msgtype, int32 id,
        uint32 language, uint64 targetGUID)
      : _source(obj), _msgType(msgtype), _textId(id), _language(language),
        _targetGUID(targetGUID)
    {
    }

    size_t operator()(WorldPacket* data, LocaleConstant locale) const
    {
        std::string text =
            sObjectMgr::Instance()->GetMangosString(_textId, locale);
        char const* localizedName = _source->GetNameForLocaleIdx(locale);

        *data << uint8(_msgType);
        *data << uint32(_language);
        *data << uint64(_source->GetObjectGuid().GetRawValue());
        *data << uint32(1); // 2.1.0
        *data << uint32(strlen(localizedName) + 1);
        *data << localizedName;
        size_t whisperGUIDpos = data->wpos();
        *data << uint64(_targetGUID); // Unit Target
        if (_targetGUID && !ObjectGuid(_targetGUID).IsPlayer())
        {
            *data << uint32(1); // target name length
            *data << uint8(0);  // target name
        }
        *data << uint32(text.length() + 1);
        *data << text;
        *data << uint8(0); // ChatTag

        return whisperGUIDpos;
    }

    WorldObject* _source;
    ChatMsg _msgType;
    int32 _textId;
    uint32 _language;
    uint64 _targetGUID;
};

SmartScript::SmartScript()
  : me(nullptr), go(nullptr), area_trigger(nullptr),
    mScriptType(SMART_SCRIPT_TYPE_CREATURE), mEventPhase(0), mNewPhase(-1),
    mPathId(0), mTextTimer(0), mLastTextID(0), mTalkerEntry(0),
    mKillSayCooldown(0), mUseTextTimer(false)
{
}

SmartScript::~SmartScript()
{
}

void SmartScript::OnReset(int type_mask)
{
    _SetEventPhase(0);
    mNewPhase = -1;
    for (auto& elem : mEvents)
    {
        if (!((elem).event.event_flags & SMART_EVENT_FLAG_DONT_RESET))
        {
            InitTimer((elem));
            (elem).runOnce = false;
        }
    }
    ProcessEventsFor(SMART_EVENT_RESET, nullptr, type_mask);
    mLastInvoker.Clear();
}

bool SmartScript::ProcessEventsFor(SMART_EVENT e, Unit* unit, uint32 var0,
    uint32 var1, bool bvar, const SpellEntry* spell, GameObject* gob)
{
    bool didProcess = false;

    // ProcessEventsFor can trigger while we have a pending phase change
    if (mNewPhase != -1)
        _SetEventPhase(mNewPhase);
    mNewPhase = -1; // -1 means unchanged

    for (auto& elem : mEvents)
    {
        SMART_EVENT eventType = SMART_EVENT((elem).GetEventType());
        if (eventType == SMART_EVENT_LINK) // special handling
            continue;

        if (eventType ==
            e /* && (!(*i).event.event_phase_mask || IsInPhase((*i).event.event_phase_mask)) && !((*i).event.event_flags & SMART_EVENT_FLAG_NOT_REPEATABLE && (*i).runOnce)*/)
        {
            bool meets = true;
            const ConditionList* conds =
                sConditionMgr::Instance()->GetConditionsForSmartEvent(
                    (elem).entryOrGuid, (elem).event_id, (elem).source_type);
            ConditionSourceInfo info =
                ConditionSourceInfo(unit, GetBaseObject());
            meets = sConditionMgr::Instance()->IsObjectMeetToConditions(
                info, conds);

            if (meets)
            {
                if (ProcessEvent(elem, unit, var0, var1, bvar, spell, gob))
                    didProcess = true;
            }
        }
    }
    // Only update phase until after event has been processed
    if (mNewPhase != -1)
        _SetEventPhase(mNewPhase);
    return didProcess;
}

void SmartScript::ProcessAction(SmartScriptHolder& e, Unit* unit, uint32 var0,
    uint32 var1, bool bvar, const SpellEntry* spell, GameObject* gob)
{
    if (!area_trigger &&
        (!GetBaseObject() || (me && !IsSmart()) || (go && !IsSmartGO())))
        return;

    // calc random
    if (e.GetEventType() != SMART_EVENT_LINK && e.event.event_chance < 100 &&
        e.event.event_chance)
    {
        uint32 rnd = urand(0, 100);
        if (e.event.event_chance <= rnd)
            return;
    }
    e.runOnce = true; // used for repeat check

    if (unit)
        mLastInvoker = unit->GetObjectGuid();

    switch (e.GetActionType())
    {
    case SMART_ACTION_NONE:
    {
        if (e.GetTargetType() == SMART_TARGET_NONE)
            break;
        auto targets = GetTargets(e, unit);
        if ((e.action.raw.param1 == 0 && targets.empty()) ||
            (e.action.raw.param1 == 1 && !targets.empty()))
            return;
        mNoneSelectedTargets.clear();
        mNoneSelectedTargets.reserve(targets.size());
        for (auto& target : targets)
            mNoneSelectedTargets.push_back(target->GetObjectGuid());
        break;
    }
    case SMART_ACTION_TALK:
    {
        ObjectList targets = GetTargets(e, unit);
        Creature* talker = nullptr;
        Player* targetPlayer = nullptr;

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (IsCreature((*itr)))
            {
                talker = ((Creature*)*itr);
                break;
            }
            else if (IsPlayer((*itr)))
            {
                targetPlayer = ((Player*)*itr);
                break;
            }
        }

        if (targetPlayer)
            talker = me;

        if (!talker)
            break;

        mTalkerEntry = talker->GetEntry();
        mLastTextID = e.action.talk.textGroupID;
        mTextTimer = e.action.talk.duration;
        if (IsPlayer(
                GetLastInvoker())) // used for $vars in texts and whisper target
            mTextGUID = GetLastInvoker()->GetObjectGuid();
        else if (targetPlayer)
            mTextGUID = targetPlayer->GetObjectGuid();
        else
            mTextGUID.Clear();

        mUseTextTimer = true;
        sCreatureTextMgr::Instance()->SendChat(
            talker, uint8(e.action.talk.textGroupID), mTextGUID);
        LOG_DEBUG(logging,
            "SmartScript::ProcessAction: SMART_ACTION_TALK: talker: %s "
            "(GuidLow: %u), textGuid: %u",
            talker->GetName(), talker->GetGUIDLow(), mTextGUID.GetEntry());
        break;
    }
    case SMART_ACTION_TALK2:
    {
        if (!me)
            return;

        ObjectList targets = GetTargets(e, unit);
        Unit* target = nullptr;

        for (auto& tar : targets)
        {
            if (tar != me && (IsPlayer(tar) || IsCreature(tar)))
            {
                target = static_cast<Unit*>(tar);
                break;
            }
        }

        mTalkerEntry = me->GetEntry();
        mLastTextID = e.action.talk.textGroupID;
        mTextTimer = e.action.talk.duration;

        if (target)
            mTextGUID = target->GetObjectGuid();
        else
            mTextGUID.Clear();

        mUseTextTimer = true;
        sCreatureTextMgr::Instance()->SendChat(
            me, uint8(e.action.talk.textGroupID), mTextGUID, target->GetName());
        LOG_DEBUG(logging,
            "SmartScript::ProcessAction: SMART_ACTION_TALK2: talker: %s "
            "(GuidLow: %u), textGuid: %u",
            me->GetName(), me->GetGUIDLow(), mTextGUID.GetEntry());
        break;
    }
    case SMART_ACTION_SIMPLE_TALK:
    {
        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (IsCreature(*itr))
                sCreatureTextMgr::Instance()->SendChat(((Creature*)*itr),
                    uint8(e.action.talk.textGroupID),
                    IsPlayer(GetLastInvoker()) ?
                        GetLastInvoker()->GetObjectGuid() :
                        0);
            else if (IsPlayer(*itr) && me)
            {
                Unit* templastInvoker = GetLastInvoker();
                sCreatureTextMgr::Instance()->SendChat(me,
                    uint8(e.action.talk.textGroupID),
                    IsPlayer(templastInvoker) ?
                        templastInvoker->GetObjectGuid() :
                        0,
                    nullptr, CHAT_MSG_ADDON, LANG_ADDON, 0, TEAM_NONE, false,
                    ((Player*)*itr));
            }
            LOG_DEBUG(logging,
                "SmartScript::ProcessAction:: SMART_ACTION_SIMPLE_TALK: "
                "talker: %s (GuidLow: %u), textGroupId: %u",
                (*itr)->GetName(), (*itr)->GetGUIDLow(),
                uint8(e.action.talk.textGroupID));
        }

        break;
    }
    case SMART_ACTION_KILL_SAY:
    {
        if (!me)
            return;

        if (mKillSayCooldown)
            return;

        ObjectList targets = GetTargets(e, unit);

        for (auto tar : targets)
        {
            if (IsPlayer(tar))
            {
                sCreatureTextMgr::Instance()->SendChat(
                    me, uint8(e.action.talk.textGroupID), tar->GetObjectGuid());
                mKillSayCooldown = KILL_SAY_COOLDOWN;
                break;
            }
        }

        break;
    }
    case SMART_ACTION_PLAY_EMOTE:
    {
        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (IsUnit(*itr))
            {
                ((Unit*)*itr)->HandleEmoteCommand(e.action.emote.emote);
                LOG_DEBUG(logging,
                    "SmartScript::ProcessAction:: SMART_ACTION_PLAY_EMOTE: "
                    "target: %s (GuidLow: %u), emote: %u",
                    (*itr)->GetName(), (*itr)->GetGUIDLow(),
                    e.action.emote.emote);
            }
        }

        break;
    }
    case SMART_ACTION_SOUND:
    {
        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (IsUnit(*itr))
            {
                if (e.action.sound.local)
                {
                    (*itr)->PlayDistanceSound(e.action.sound.sound);
                }
                else
                {
                    (*itr)->PlayDirectSound(
                        e.action.sound.sound,
                        (e.action.sound.range == 0 &&
                            (*itr)->GetTypeId() == TYPEID_PLAYER) ?
                            ((Player*)*itr) :
                            nullptr);
                }
            }
        }

        break;
    }
    case SMART_ACTION_SET_FACTION:
    {
        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (IsCreature(*itr))
            {
                if (e.action.faction.factionID)
                {
                    ((Creature*)*itr)->setFaction(e.action.faction.factionID);
                    LOG_DEBUG(logging,
                        "SmartScript::ProcessAction:: "
                        "SMART_ACTION_SET_FACTION: Creature entry %u, GuidLow "
                        "%u set faction to %u",
                        (*itr)->GetEntry(), (*itr)->GetGUIDLow(),
                        e.action.faction.factionID);
                }
                else
                {
                    if (CreatureInfo const* ci =
                            sObjectMgr::Instance()->GetCreatureTemplate(
                                ((Creature*)*itr)->GetEntry()))
                    {
                        if (((Creature*)*itr)->getFaction() != ci->faction_A)
                        {
                            ((Creature*)*itr)->setFaction(ci->faction_A);
                            LOG_DEBUG(logging,
                                "SmartScript::ProcessAction:: "
                                "SMART_ACTION_SET_FACTION: Creature entry %u, "
                                "GuidLow %u set faction to %u",
                                (*itr)->GetEntry(), (*itr)->GetGUIDLow(),
                                ci->faction_A);
                        }
                    }
                }

                auto c = static_cast<Creature*>(*itr);
                if (IsSmart(c))
                    static_cast<SmartAI*>((c)->AI())->UpdatePassive();
            }
        }
        break;
    }
    case SMART_ACTION_MORPH_TO_ENTRY_OR_MODEL:
    {
        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (IsPlayer(*itr))
            {
                if (!e.action.morphOrMount.model)
                    ((Player*)*itr)->DeMorph();
                else
                    ((Player*)*itr)->SetDisplayId(e.action.morphOrMount.model);
                continue;
            }

            // The rest of the logic is creatures only
            if (!IsCreature(*itr))
                continue;

            if (e.action.morphOrMount.creature || e.action.morphOrMount.model)
            {
                // set model based on entry from creature_template
                if (e.action.morphOrMount.creature)
                {
                    if (e.action.morphOrMount.updateEntry)
                    {
                        // Use UpdateEntry instead of model morph
                        ((Creature*)*itr)
                            ->UpdateEntry(e.action.morphOrMount.creature);
                    }
                    else if (CreatureInfo const* ci =
                                 sObjectMgr::Instance()->GetCreatureTemplate(
                                     e.action.morphOrMount.creature))
                    {
                        uint32 display_id =
                            ((Creature*)*itr)
                                ->ChooseDisplayId(
                                    ci); // sObjectMgr::Instance()->ChooseDisplayId(0,
                                         // ci);
                        ((Creature*)*itr)->SetDisplayId(display_id);
                        LOG_DEBUG(logging,
                            "SmartScript::ProcessAction:: "
                            "SMART_ACTION_MORPH_TO_ENTRY_OR_MODEL: Creature "
                            "entry %u, GuidLow %u set displayid to %u",
                            (*itr)->GetEntry(), (*itr)->GetGUIDLow(),
                            display_id);
                    }
                }
                // if no param1, then use value from param2 (modelId)
                else
                {
                    ((Creature*)*itr)
                        ->SetDisplayId(e.action.morphOrMount.model);
                    LOG_DEBUG(logging,
                        "SmartScript::ProcessAction:: "
                        "SMART_ACTION_MORPH_TO_ENTRY_OR_MODEL: Creature entry "
                        "%u, GuidLow %u set displayid to %u",
                        (*itr)->GetEntry(), (*itr)->GetGUIDLow(),
                        e.action.morphOrMount.model);
                }
            }
            else
            {
                ((Creature*)*itr)->DeMorph();
                LOG_DEBUG(logging,
                    "SmartScript::ProcessAction:: "
                    "SMART_ACTION_MORPH_TO_ENTRY_OR_MODEL: Creature entry %u, "
                    "GuidLow %u demorphs.",
                    (*itr)->GetEntry(), (*itr)->GetGUIDLow());
            }
        }

        break;
    }
    case SMART_ACTION_FAIL_QUEST:
    {
        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (IsPlayer(*itr))
            {
                if (e.action.questGroup.entireGroup)
                    static_cast<Player*>(*itr)->FailGroupQuest(
                        e.action.questGroup.quest);
                else
                    static_cast<Player*>(*itr)->FailQuest(
                        e.action.questGroup.quest);
            }
        }

        break;
    }
    case SMART_ACTION_ADD_QUEST:
    {
        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (IsPlayer(*itr))
            {
                if (Quest const* q = sObjectMgr::Instance()->GetQuestTemplate(
                        e.action.quest.quest))
                {
                    ((Player*)*itr)->AddQuest(q, nullptr);
                    LOG_DEBUG(logging,
                        "SmartScript::ProcessAction:: SMART_ACTION_ADD_QUEST: "
                        "Player guidLow %u add quest %u",
                        (*itr)->GetGUIDLow(), e.action.quest.quest);
                }
            }
        }

        break;
    }
    case SMART_ACTION_SET_REACT_STATE:
    {
        logging.error(
            "SmartScript::ProcessAction:: SMART_ACTION_SET_REACT_STATE was "
            "invoked. It's not implemented yet, however!");
        break;

        if (!me)
            break;

        me->GetCharmInfo()->SetReactState(ReactStates(
            e.action.react
                .state)); // me->SetReactState(ReactStates(e.action.react.state));
        LOG_DEBUG(logging,
            "SmartScript::ProcessAction:: SMART_ACTION_SET_REACT_STATE: "
            "Creature guidLow %u set reactstate %u",
            me->GetGUIDLow(), e.action.react.state);
        break;
    }
    case SMART_ACTION_RANDOM_EMOTE:
    {
        ObjectList targets = GetTargets(e, unit);
        if (targets.empty())
            break;

        uint32 emotes[SMART_ACTION_PARAM_COUNT];
        emotes[0] = e.action.randomEmote.emote1;
        emotes[1] = e.action.randomEmote.emote2;
        emotes[2] = e.action.randomEmote.emote3;
        emotes[3] = e.action.randomEmote.emote4;
        emotes[4] = e.action.randomEmote.emote5;
        emotes[5] = e.action.randomEmote.emote6;
        uint32 temp[SMART_ACTION_PARAM_COUNT];
        uint32 count = 0;
        for (auto& emote : emotes)
        {
            if (emote)
            {
                temp[count] = emote;
                ++count;
            }
        }

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (IsUnit(*itr))
            {
                uint32 emote = temp[urand(0, count)];
                ((Unit*)*itr)->HandleEmoteCommand(emote);
                LOG_DEBUG(logging,
                    "SmartScript::ProcessAction:: SMART_ACTION_RANDOM_EMOTE: "
                    "Creature guidLow %u handle random emote %u",
                    (*itr)->GetGUIDLow(), emote);
            }
        }

        break;
    }
    case SMART_ACTION_THREAT_ALL_PCT:
    {
        if (!me)
            break;

        std::list<HostileReference*> const& threatList =
            me->getThreatManager().getThreatList();
        for (const auto& elem : threatList)
        {
            if (Unit* target = sObjectAccessor::Instance()->GetUnit(
                    *me, (elem)->getUnitGuid()))
            {
                me->getThreatManager().modifyThreatPercent(
                    target, e.action.threatPCT.threatINC ?
                                (int32)e.action.threatPCT.threatINC :
                                -(int32)e.action.threatPCT.threatDEC);
                LOG_DEBUG(logging,
                    "SmartScript::ProcessAction:: SMART_ACTION_THREAT_ALL_PCT: "
                    "Creature guidLow %u modify threat for unit %u, value %i",
                    me->GetGUIDLow(), target->GetGUIDLow(),
                    e.action.threatPCT.threatINC ?
                        (int32)e.action.threatPCT.threatINC :
                        -(int32)e.action.threatPCT.threatDEC);
            }
        }
        break;
    }
    case SMART_ACTION_THREAT_SINGLE_PCT:
    {
        if (!me)
            break;

        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (IsUnit(*itr))
            {
                me->getThreatManager().modifyThreatPercent(
                    ((Unit*)*itr), e.action.threatPCT.threatINC ?
                                       (int32)e.action.threatPCT.threatINC :
                                       -(int32)e.action.threatPCT.threatDEC);
                LOG_DEBUG(logging,
                    "SmartScript::ProcessAction:: "
                    "SMART_ACTION_THREAT_SINGLE_PCT: Creature guidLow %u "
                    "modify threat for unit %u, value %i",
                    me->GetGUIDLow(), (*itr)->GetGUIDLow(),
                    e.action.threatPCT.threatINC ?
                        (int32)e.action.threatPCT.threatINC :
                        -(int32)e.action.threatPCT.threatDEC);
            }
        }

        break;
    }
    case SMART_ACTION_CALL_AREAEXPLOREDOREVENTHAPPENS:
    {
        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (IsPlayer(*itr))
            {
                ((Player*)*itr)
                    ->AreaExploredOrEventHappens(e.action.quest.quest);
                LOG_DEBUG(logging,
                    "SmartScript::ProcessAction:: "
                    "SMART_ACTION_CALL_AREAEXPLOREDOREVENTHAPPENS: Player "
                    "guidLow %u credited quest %u",
                    (*itr)->GetGUIDLow(), e.action.quest.quest);
            }
        }

        break;
    }
    case SMART_ACTION_SEND_CASTCREATUREORGO:
    {
        if (!GetBaseObject())
            break;

        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (IsPlayer(*itr))
            {
                ((Player*)*itr)
                    ->CastedCreatureOrGO(e.action.castedCreatureOrGO.creature,
                        GetBaseObject()->GetObjectGuid(),
                        e.action.castedCreatureOrGO.spell);
                LOG_DEBUG(logging,
                    "SmartScript::ProcessAction:: "
                    "SMART_ACTION_SEND_CASTCREATUREORGO: Player guidLow %u.org "
                    "Creature: %u, BaseObject GUID: " UI64FMTD ", Spell: %u",
                    (*itr)->GetGUIDLow(), e.action.castedCreatureOrGO.creature,
                    GetBaseObject()->GetObjectGuid().GetRawValue(),
                    e.action.castedCreatureOrGO.spell);
            }
        }

        break;
    }
    case SMART_ACTION_CAST:
    {
        if (!me)
            break;

        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (IsUnit(*itr))
            {
                if (e.action.cast.flags & SMARTCAST_INTERRUPT_PREVIOUS)
                    me->InterruptNonMeleeSpells(false);

                if (!(e.action.cast.flags & SMARTCAST_AURA_NOT_PRESENT) ||
                    !((Unit*)*itr)->has_aura(e.action.cast.spell))
                {
                    if (e.action.cast.flags & SMARTCAST_USE_DESTINATION)
                    {
                        float x, y, z;
                        static_cast<Unit*>(*itr)->GetPosition(x, y, z);
                        me->CastSpell(x, y, z, e.action.cast.spell,
                            (e.action.cast.flags & SMARTCAST_TRIGGERED) ?
                                true :
                                false);
                    }
                    else
                    {
                        me->CastSpell(((Unit*)*itr), e.action.cast.spell,
                            (e.action.cast.flags & SMARTCAST_TRIGGERED) ?
                                true :
                                false);
                    }
                }
                else
                {
                    LOG_DEBUG(logging,
                        "Spell %u not casted because it has flag "
                        "SMARTCAST_AURA_NOT_PRESENT and the target "
                        "(Guid: " UI64FMTD
                        " Entry: %u Type: %u) already has the aura",
                        e.action.cast.spell,
                        (*itr)->GetObjectGuid().GetRawValue(),
                        (*itr)->GetEntry(), uint32((*itr)->GetTypeId()));
                }
                LOG_DEBUG(logging,
                    "SmartScript::ProcessAction:: SMART_ACTION_CAST:: Creature "
                    "%u casts spell %u on target %u with castflags %u",
                    me->GetGUIDLow(), e.action.cast.spell, (*itr)->GetGUIDLow(),
                    e.action.cast.flags);
            }
        }

        break;
    }
    case SMART_ACTION_INVOKER_CAST:
    {
        Unit* tempLastInvoker = GetLastInvoker();
        if (!tempLastInvoker)
            break;

        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (IsUnit(*itr))
            {
                if (e.action.cast.flags & SMARTCAST_INTERRUPT_PREVIOUS)
                    tempLastInvoker->InterruptNonMeleeSpells(false);

                if (!(e.action.cast.flags & SMARTCAST_AURA_NOT_PRESENT) ||
                    !((Unit*)*itr)->has_aura(e.action.cast.spell))
                {
                    if (e.action.cast.flags & SMARTCAST_USE_DESTINATION)
                    {
                        float x, y, z;
                        static_cast<Unit*>(*itr)->GetPosition(x, y, z);
                        tempLastInvoker->CastSpell(x, y, z, e.action.cast.spell,
                            (e.action.cast.flags & SMARTCAST_TRIGGERED) ?
                                true :
                                false);
                    }
                    else
                    {
                        tempLastInvoker->CastSpell(((Unit*)*itr),
                            e.action.cast.spell,
                            (e.action.cast.flags & SMARTCAST_TRIGGERED) ?
                                true :
                                false);
                    }
                }
                else
                {
                    LOG_DEBUG(logging,
                        "Spell %u not casted because it has flag "
                        "SMARTCAST_AURA_NOT_PRESENT and the target "
                        "(Guid: " UI64FMTD
                        " Entry: %u Type: %u) already has the aura",
                        e.action.cast.spell,
                        (*itr)->GetObjectGuid().GetRawValue(),
                        (*itr)->GetEntry(), uint32((*itr)->GetTypeId()));
                }

                LOG_DEBUG(logging,
                    "SmartScript::ProcessAction:: SMART_ACTION_INVOKER_CAST: "
                    "Invoker %u casts spell %u on target %u with castflags %u",
                    tempLastInvoker->GetGUIDLow(), e.action.cast.spell,
                    (*itr)->GetGUIDLow(), e.action.cast.flags);
            }
        }

        break;
    }
    case SMART_ACTION_ADD_AURA:
    {
        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (IsUnit(*itr))
            {
                ((Unit*)*itr)
                    ->AddAuraThroughNewHolder(
                        e.action.cast.spell, ((Unit*)*itr));
                LOG_DEBUG(logging,
                    "SmartScript::ProcessAction:: SMART_ACTION_ADD_AURA: "
                    "Adding aura %u to unit %u",
                    e.action.cast.spell, (*itr)->GetGUIDLow());
            }
        }

        break;
    }
    case SMART_ACTION_PACIFY:
    {
        if (me && me->AI())
            me->AI()->Pacify(e.action.raw.param1);
        break;
    }
    case SMART_ACTION_ACTIVATE_GOBJECT:
    {
        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (IsGameObject(*itr))
            {
                // Activate
                ((GameObject*)*itr)->SetLootState(GO_READY);
                ((GameObject*)*itr)->UseDoorOrButton(0, false, unit);
            }
        }

        break;
    }
    case SMART_ACTION_RESET_GOBJECT:
    {
        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (IsGameObject(*itr))
                ((GameObject*)*itr)->ResetDoorOrButton();
        }

        break;
    }
    case SMART_ACTION_SET_EMOTE_STATE:
    {
        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (IsUnit(*itr))
            {
                ((Unit*)*itr)
                    ->SetUInt32Value(UNIT_NPC_EMOTESTATE, e.action.emote.emote);
                LOG_DEBUG(logging,
                    "SmartScript::ProcessAction:: "
                    "SMART_ACTION_SET_EMOTE_STATE. Unit %u set emotestate to "
                    "%u",
                    (*itr)->GetGUIDLow(), e.action.emote.emote);
            }
        }

        break;
    }
    case SMART_ACTION_SET_UNIT_FLAG:
    {
        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (IsUnit(*itr) && e.action.unitFlag.type != 1)
            {
                if (e.action.unitFlag.type == 0)
                {
                    ((Unit*)*itr)
                        ->SetFlag(UNIT_FIELD_FLAGS, e.action.unitFlag.flag);
                    if ((*itr)->GetTypeId() == TYPEID_UNIT)
                    {
                        auto c = static_cast<Creature*>(*itr);
                        if (IsSmart(c))
                            static_cast<SmartAI*>((c)->AI())->UpdatePassive();
                    }
                }
                else
                {
                    ((Unit*)*itr)
                        ->SetFlag(UNIT_FIELD_FLAGS_2, e.action.unitFlag.flag);
                }
            }
            else if (IsGameObject(*itr) && e.action.unitFlag.type == 1)
            {
                static_cast<GameObject*>(*itr)->SetFlag(
                    GAMEOBJECT_FLAGS, e.action.unitFlag.flag);
            }
        }

        break;
    }
    case SMART_ACTION_REMOVE_UNIT_FLAG:
    {
        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (IsUnit(*itr) && e.action.unitFlag.type != 1)
            {
                if (e.action.unitFlag.type == 0)
                {
                    ((Unit*)*itr)
                        ->RemoveFlag(UNIT_FIELD_FLAGS, e.action.unitFlag.flag);

                    if ((*itr)->GetTypeId() == TYPEID_UNIT)
                    {
                        auto c = static_cast<Creature*>(*itr);
                        if (IsSmart(c))
                            static_cast<SmartAI*>((c)->AI())->UpdatePassive();
                    }
                }
                else
                {
                    ((Unit*)*itr)
                        ->RemoveFlag(
                            UNIT_FIELD_FLAGS_2, e.action.unitFlag.flag);
                }
            }
            else if (IsGameObject(*itr) && e.action.unitFlag.type == 1)
            {
                static_cast<GameObject*>(*itr)->RemoveFlag(
                    GAMEOBJECT_FLAGS, e.action.unitFlag.flag);
            }
        }

        break;
    }
    case SMART_ACTION_AUTO_ATTACK:
    {
        if (!me)
            break;

        ((SmartAI*)me->AI())
            ->SetAutoAttack(e.action.autoAttack.attack ? true : false);
        LOG_DEBUG(logging,
            "SmartScript::ProcessAction:: SMART_ACTION_AUTO_ATTACK: Creature: "
            "%u bool on = %u",
            me->GetGUIDLow(), e.action.autoAttack.attack);
        break;
    }
    case SMART_ACTION_ALLOW_COMBAT_MOVEMENT:
    {
        if (!IsSmart())
            break;

        bool move = e.action.combatMove.move ? true : false;
        ((SmartAI*)me->AI())->SetCombatMove(move);
        LOG_DEBUG(logging,
            "SmartScript::ProcessAction:: SMART_ACTION_ALLOW_COMBAT_MOVEMENT: "
            "Creature %u bool on = %u",
            me->GetGUIDLow(), e.action.combatMove.move);
        break;
    }
    case SMART_ACTION_SET_EVENT_PHASE:
    {
        if (!GetBaseObject())
            break;

        mNewPhase = e.action.setEventPhase.phase;
        LOG_DEBUG(logging,
            "SmartScript::ProcessAction:: SMART_ACTION_SET_EVENT_PHASE: "
            "Creature %u set event phase %u",
            GetBaseObject()->GetGUIDLow(), e.action.setEventPhase.phase);
        break;
    }
    case SMART_ACTION_INC_EVENT_PHASE:
    {
        if (!GetBaseObject())
            break;

        if (e.action.incEventPhase.inc > 0)
        {
            if (mNewPhase == -1)
                mNewPhase = mEventPhase;
            mNewPhase += e.action.incEventPhase.inc;
        }
        else if (e.action.incEventPhase.dec > 0)
        {
            if (mNewPhase == -1)
                mNewPhase = mEventPhase;
            if (e.action.incEventPhase.dec >= (uint32)mNewPhase)
                mNewPhase = 0;
            else
                mNewPhase -= e.action.incEventPhase.dec;
        }
        LOG_DEBUG(logging,
            "SmartScript::ProcessAction:: SMART_ACTION_INC_EVENT_PHASE: "
            "Creature %u inc event phase by %u, "
            "decrease by %u",
            GetBaseObject()->GetGUIDLow(), e.action.incEventPhase.inc,
            e.action.incEventPhase.dec);
        break;
    }
    case SMART_ACTION_EVADE:
    {
        if (!me)
            break;

        me->AI()->EnterEvadeMode();
        LOG_DEBUG(logging,
            "SmartScript::ProcessAction:: SMART_ACTION_EVADE: Creature %u "
            "EnterEvadeMode",
            me->GetGUIDLow());
        break;
    }
    case SMART_ACTION_FLEE_FOR_ASSIST:
    {
        if (!me)
            break;

        me->RunAwayInFear(e.action.flee.withEmote);
        LOG_DEBUG(logging,
            "SmartScript::ProcessAction:: SMART_ACTION_FLEE_FOR_ASSIST: "
            "Creature %u DoFleeToGetAssistance",
            me->GetGUIDLow());
        break;
    }
    case SMART_ACTION_CALL_GROUPEVENTHAPPENS:
    {
        if (!GetBaseObject())
            break;

        auto targets = GetTargets(e, unit);
        for (auto& target : targets)
        {
            if (IsPlayer(target))
            {
                static_cast<Player*>(target)->GroupEventHappens(
                    e.action.quest.quest, GetBaseObject());
            }
        }
        break;
    }
    case SMART_ACTION_CALL_CASTEDCREATUREORGO:
    {
        if (!GetBaseObject())
            break;

        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (IsPlayer((*itr)))
            {
                ((Player*)*itr)
                    ->CastedCreatureOrGO(e.action.castedCreatureOrGO.creature,
                        GetBaseObject()->GetObjectGuid(),
                        e.action.castedCreatureOrGO.spell);
                LOG_DEBUG(logging,
                    "SmartScript::ProcessAction: "
                    "SMART_ACTION_CALL_CASTEDCREATUREORGO: Player %u, target "
                    "%u, spell %u",
                    (*itr)->GetGUIDLow(), e.action.castedCreatureOrGO.creature,
                    e.action.castedCreatureOrGO.spell);
            }
        }

        break;
    }
    case SMART_ACTION_REMOVEAURASFROMSPELL:
    {
        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (!IsUnit((*itr)))
                continue;

            if (e.action.removeAura.spell == 0)
                ((Unit*)*itr)->remove_auras();
            else
                ((Unit*)*itr)->remove_auras(e.action.removeAura.spell);

            LOG_DEBUG(logging,
                "SmartScript::ProcessAction: "
                "SMART_ACTION_REMOVEAURASFROMSPELL: Unit %u, spell %u",
                (*itr)->GetGUIDLow(), e.action.removeAura.spell);
        }

        break;
    }
    case SMART_ACTION_FOLLOW:
    {
        if (!IsSmart() || !me)
            break;

        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (IsUnit((*itr)))
            {
                float followdist, followangle;
                if (e.action.follow.followType ==
                    SMART_FOLLOW_TYPE_HALF_CIRCLE_BEHIND)
                {
                    followdist = 0;
                    followangle = M_PI_F + frand(-M_PI_F / 2, M_PI_F / 2);
                }
                else
                {
                    followdist = e.action.follow.dist > 0 ?
                                     (float)e.action.follow.dist :
                                     PET_FOLLOW_DIST;
                    followangle = e.action.follow.angle > 0 ?
                                      (float)e.action.follow.angle :
                                      PET_FOLLOW_ANGLE;
                }
                me->movement_gens.push(new movement::FollowMovementGenerator(
                    static_cast<Unit*>(*itr), followdist, followangle));
                break;
            }
        }

        break;
    }
    case SMART_ACTION_RANDOM_PHASE:
    {
        if (!GetBaseObject())
            break;

        uint32 phases[SMART_ACTION_PARAM_COUNT];
        phases[0] = e.action.randomPhase.phase1;
        phases[1] = e.action.randomPhase.phase2;
        phases[2] = e.action.randomPhase.phase3;
        phases[3] = e.action.randomPhase.phase4;
        phases[4] = e.action.randomPhase.phase5;
        phases[5] = e.action.randomPhase.phase6;
        uint32 temp[SMART_ACTION_PARAM_COUNT];
        uint32 count = 0;
        for (auto& phase : phases)
        {
            if (phase > 0)
            {
                temp[count] = phase;
                ++count;
            }
        }

        uint32 phase = temp[urand(0, count)];
        mNewPhase = phase;
        LOG_DEBUG(logging,
            "SmartScript::ProcessAction: SMART_ACTION_RANDOM_PHASE: Creature "
            "%u sets event phase to %u",
            GetBaseObject()->GetGUIDLow(), phase);
        break;
    }
    case SMART_ACTION_RANDOM_PHASE_RANGE:
    {
        if (!GetBaseObject())
            break;

        uint32 phase = urand(e.action.randomPhaseRange.phaseMin,
            e.action.randomPhaseRange.phaseMax);
        mNewPhase = phase;
        LOG_DEBUG(logging,
            "SmartScript::ProcessAction: SMART_ACTION_RANDOM_PHASE_RANGE: "
            "Creature %u sets event phase to %u",
            GetBaseObject()->GetGUIDLow(), phase);
        break;
    }
    case SMART_ACTION_CALL_KILLEDMONSTER:
    {
        Player* player = nullptr;
        if (me)
            player = me->GetLootDistributor() ?
                         me->GetLootDistributor()
                             ->recipient_mgr()
                             ->first_valid_player() :
                         nullptr;

        if (me && player)
            player->RewardPlayerAndGroupAtEvent(
                e.action.killedMonster.creature, player);
        else if (GetBaseObject())
        {
            ObjectList targets = GetTargets(e, unit);

            for (ObjectList::const_iterator itr = targets.begin();
                 itr != targets.end(); ++itr)
            {
                if (!IsPlayer(*itr))
                    continue;

                ((Player*)*itr)
                    ->RewardPlayerAndGroupAtEvent(
                        e.action.killedMonster.creature, ((Player*)*itr));
                LOG_DEBUG(logging,
                    "SmartScript::ProcessAction: "
                    "SMART_ACTION_CALL_KILLEDMONSTER: Player %u, Killcredit: "
                    "%u",
                    (*itr)->GetGUIDLow(), e.action.killedMonster.creature);
            }
        }
        else if (area_trigger && IsPlayer(unit))
        {
            ((Player*)unit)
                ->RewardPlayerAndGroupAtEvent(
                    e.action.killedMonster.creature, unit);
            LOG_DEBUG(logging,
                "SmartScript::ProcessAction: SMART_ACTION_CALL_KILLEDMONSTER: "
                "(trigger == true) Player %u, Killcredit: %u",
                unit->GetGUIDLow(), e.action.killedMonster.creature);
        }
        break;
    }
    case SMART_ACTION_SET_INST_DATA:
    {
        WorldObject* obj = GetBaseObject();
        if (!obj)
            obj = unit;

        if (!obj)
            break;

        InstanceData* instance = obj->GetInstanceData();
        if (!instance)
        {
            logging.error(
                "SmartScript: Event %u attempt to set instance data without "
                "instance script. EntryOrGuid %d",
                e.GetEventType(), e.entryOrGuid);
            break;
        }

        instance->SetData(
            e.action.setInstanceData.field, e.action.setInstanceData.data);
        LOG_DEBUG(logging,
            "SmartScript::ProcessAction: SMART_ACTION_SET_INST_DATA: Field: "
            "%u, data: %u",
            e.action.setInstanceData.field, e.action.setInstanceData.data);
        break;
    }
    case SMART_ACTION_SET_INST_DATA64:
    {
        WorldObject* obj = GetBaseObject();
        if (!obj)
            obj = unit;

        if (!obj)
            break;

        InstanceData* instance = obj->GetInstanceData();
        if (!instance)
        {
            logging.error(
                "SmartScript: Event %u attempt to set instance data without "
                "instance script. EntryOrGuid %d",
                e.GetEventType(), e.entryOrGuid);
            break;
        }

        ObjectList targets = GetTargets(e, unit);
        if (targets.empty())
            break;

        instance->SetData64(
            e.action.setInstanceData64.field, targets.front()->GetObjectGuid());
        LOG_DEBUG(logging,
            "SmartScript::ProcessAction: SMART_ACTION_SET_INST_DATA64: Field: "
            "%u, data: " UI64FMTD,
            e.action.setInstanceData64.field,
            targets.front()->GetObjectGuid().GetRawValue());
        break;
    }
    case SMART_ACTION_UPDATE_TEMPLATE:
    {
        if (!me || me->GetEntry() == e.action.updateTemplate.creature)
            break;

        me->UpdateEntry(e.action.updateTemplate.creature,
            e.action.updateTemplate.team ? HORDE : ALLIANCE);
        LOG_DEBUG(logging,
            "SmartScript::ProcessAction: SMART_ACTION_UPDATE_TEMPLATE: "
            "Creature %u, Template: %u, Team: %u",
            me->GetGUIDLow(), me->GetEntry(),
            e.action.updateTemplate.team ? HORDE : ALLIANCE);
        break;
    }
    case SMART_ACTION_DIE:
    {
        if (me && !me->isDead())
        {
            me->Kill(me);
            LOG_DEBUG(logging,
                "SmartScript::ProcessAction: SMART_ACTION_DIE: Creature %u",
                me->GetGUIDLow());
        }
        break;
    }
    case SMART_ACTION_SET_IN_COMBAT_WITH_ZONE:
    {
        if (me)
        {
            me->SetInCombatWithZone();
            LOG_DEBUG(logging,
                "SmartScript::ProcessAction: "
                "SMART_ACTION_SET_IN_COMBAT_WITH_ZONE: Creature %u",
                me->GetGUIDLow());
        }
        break;
    }
    case SMART_ACTION_CALL_FOR_HELP:
    {
        if (me)
        {
            me->CallForHelp(e.action.callHelp.range);
            LOG_DEBUG(logging,
                "SmartScript::ProcessAction: SMART_ACTION_CALL_FOR_HELP: "
                "Creature %u",
                me->GetGUIDLow());
        }
        break;
    }
    case SMART_ACTION_SET_SHEATH:
    {
        if (me)
        {
            me->SetSheath(SheathState(e.action.setSheath.sheath));
            LOG_DEBUG(logging,
                "SmartScript::ProcessAction: SMART_ACTION_SET_SHEATH: Creature "
                "%u, State: %u",
                me->GetGUIDLow(), e.action.setSheath.sheath);
        }
        break;
    }
    case SMART_ACTION_FORCE_DESPAWN:
    {
        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (!IsCreature(*itr))
                continue;
            static_cast<Creature*>(*itr)->ForcedDespawn(
                e.action.forceDespawn.delay);
        }
        break;
    }
    case SMART_ACTION_SET_INGAME_PHASE_MASK:
    {
        /*if (GetBaseObject())
                GetBaseObject()->SetPhaseMask(e.action.ingamePhaseMask.mask, true);*/ // 3.3.3
        break;
    }
    case SMART_ACTION_MOUNT_TO_ENTRY_OR_MODEL:
    {
        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (!IsUnit(*itr))
                continue;

            if (e.action.morphOrMount.creature || e.action.morphOrMount.model)
            {
                if (e.action.morphOrMount.creature > 0)
                {
                    if (CreatureInfo const* cInfo =
                            sObjectMgr::Instance()->GetCreatureTemplate(
                                e.action.morphOrMount.creature))
                    {
                        uint32 display_id =
                            cInfo
                                ->GetRandomValidModelId(); // sObjectMgr::Instance()->ChooseDisplayId(0,
                                                           // cInfo);
                        ((Unit*)*itr)->Mount(display_id);
                    }
                }
                else
                    ((Unit*)*itr)->Mount(e.action.morphOrMount.model);
            }
            else
                ((Unit*)*itr)->Unmount();
        }

        break;
    }
    case SMART_ACTION_SET_INVINCIBILITY_HP_LEVEL:
    {
        if (!me || !IsSmart())
            break;

        if (e.action.invincHP.percent)
            ((SmartAI*)me->AI())
                ->SetInvincibilityHpLevel(
                    uint32((float)me->GetMaxHealth() *
                           float(100 / e.action.invincHP.percent)));
        else
            ((SmartAI*)me->AI())
                ->SetInvincibilityHpLevel(e.action.invincHP.minHP);
        break;
    }
    case SMART_ACTION_SET_DATA:
    {
        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (IsCreature(*itr))
                ((Creature*)*itr)
                    ->AI()
                    ->SetData(e.action.setData.field, e.action.setData.data);
            else if (IsGameObject(*itr))
                ((GameObject*)*itr)
                    ->AI()
                    ->SetData(e.action.setData.field, e.action.setData.data);
        }

        break;
    }
    case SMART_ACTION_MOVE_FORWARD:
    {
        if (!me || !IsSmart())
            break;

        auto pos = me->GetPoint(0.0f, (float)e.action.raw.param1);
        me->movement_gens.push(new movement::PointMovementGenerator(
            SMART_RANDOM_POINT, pos.x, pos.y, pos.z, false,
            static_cast<SmartAI*>(me->AI())->GetRun()));
        break;
    }
    case SMART_ACTION_SET_VISIBILITY:
    {
        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (IsCreature(*itr))
            {
                Creature* c = (Creature*)*itr;

                UnitVisibility vis;
                if (e.action.visibility.state)
                {
                    vis = c->HasAuraType(SPELL_AURA_MOD_STEALTH) ?
                              VISIBILITY_GROUP_STEALTH :
                              c->HasAuraType(SPELL_AURA_MOD_INVISIBILITY) ?
                              VISIBILITY_GROUP_INVISIBILITY :
                              VISIBILITY_ON;
                }
                else
                {
                    vis = VISIBILITY_OFF;
                }

                c->SetVisibility(vis);
                if (e.action.visibility.togglePassivity)
                {
                    if (!e.action.visibility.state)
                        c->SetFlag(UNIT_FIELD_FLAGS,
                            UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE);
                    else
                        c->RemoveFlag(UNIT_FIELD_FLAGS,
                            UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE);
                }
            }
        }

        break;
    }
    case SMART_ACTION_SET_ACTIVE:
    {
        if (WorldObject* obj = GetBaseObject())
            obj->SetActiveObjectState(e.action.onOff.boolean);
        break;
    }
    case SMART_ACTION_ATTACK_START:
    {
        if (!me)
            break;

        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (IsUnit(*itr))
            {
                me->AI()->AttackStart(((Unit*)*itr));
                break;
            }
        }

        break;
    }
    case SMART_ACTION_SUMMON_CREATURE:
    {
        ObjectList targets = GetTargets(e, unit);
        if (!targets.empty())
        {
            float x, y, z, o;
            for (ObjectList::const_iterator itr = targets.begin();
                 itr != targets.end(); ++itr)
            {
                (*itr)->GetPosition(x, y, z);
                o = (*itr)->GetO();
                x += e.target.x;
                y += e.target.y;
                z += e.target.z;
                o += e.target.o;
                if (Creature* summon = GetBaseObject()->SummonCreature(
                        e.action.summonCreature.creature, x, y, z, o,
                        (TempSummonType)e.action.summonCreature.type,
                        e.action.summonCreature.duration,
                        e.action.summonCreature.summonOptions))
                {
                    if (e.action.summonCreature.attackInvoker)
                        summon->AI()->AttackStart(((Unit*)*itr));
                    else if (e.action.summonCreature.assistOwner &&
                             GetBaseObject()->GetTypeId() == TYPEID_UNIT &&
                             ((Unit*)GetBaseObject())->getVictim())
                        summon->AI()->AttackStart(
                            ((Unit*)GetBaseObject())->getVictim());
                }
            }

            break;
        }

        float x, y, z, o;
        if (!GetTargetPosition(e, x, y, z, o))
            break;

        if (Creature* summon = GetBaseObject()->SummonCreature(
                e.action.summonCreature.creature, x, y, z, o,
                (TempSummonType)e.action.summonCreature.type,
                e.action.summonCreature.duration,
                e.action.summonCreature.summonOptions))
        {
            if (unit && e.action.summonCreature.attackInvoker)
                summon->AI()->AttackStart(unit);
            else if (e.action.summonCreature.assistOwner &&
                     GetBaseObject()->GetTypeId() == TYPEID_UNIT &&
                     ((Unit*)GetBaseObject())->getVictim())
                summon->AI()->AttackStart(
                    ((Unit*)GetBaseObject())->getVictim());
        }
        break;
    }
    case SMART_ACTION_SUMMON_GO:
    {
        if (!GetBaseObject())
            break;

        float x, y, z, o;
        if (GetTargetPosition(e, x, y, z, o))
        {
            GetBaseObject()->SummonGameObject(e.action.summonGO.entry, x, y, z,
                o, 0, 0, 0, 0, e.action.summonGO.despawnTime, true);
            break;
        }

        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (!IsUnit(*itr))
                continue;

            float x, y, z, o;
            (*itr)->GetPosition(x, y, z);
            o = (*itr)->GetO();
            x += e.target.x;
            y += e.target.y;
            z += e.target.z;
            o += e.target.o;
            GetBaseObject()->SummonGameObject(e.action.summonGO.entry, x, y, z,
                o, 0, 0, 0, 0, e.action.summonGO.despawnTime, true);
        }
        break;
    }
    case SMART_ACTION_KILL_UNIT:
    {
        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (!IsUnit(*itr))
                continue;

            ((Unit*)*itr)->Kill(((Unit*)*itr));
        }

        break;
    }
    case SMART_ACTION_ADD_ITEM:
    {
        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (!IsPlayer(*itr))
                continue;

            inventory::transaction trans;
            trans.add(e.action.item.entry, e.action.item.count);
            static_cast<Player*>(*itr)->storage().finalize(trans);
        }

        break;
    }
    case SMART_ACTION_REMOVE_ITEM:
    {
        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (!IsPlayer(*itr))
                continue;

            // XXX
            inventory::transaction trans(false);
            trans.destroy(e.action.item.entry, e.action.item.count);
            ((Player*)*itr)->storage().finalize(trans);
        }

        break;
    }
    case SMART_ACTION_STORE_VARIABLE_DECIMAL:
    {
        if (mStoredDecimals.find(e.action.storeVar.id) != mStoredDecimals.end())
            mStoredDecimals.erase(e.action.storeVar.id);
        mStoredDecimals[e.action.storeVar.id] = e.action.storeVar.number;
        break;
    }
    case SMART_ACTION_STORE_TARGET_LIST:
    {
        ObjectList targets = GetTargets(e, unit);
        StoreTargets(targets, e.action.storeTargets.id);
        break;
    }
    case SMART_ACTION_TELEPORT:
    {
        float x, y, z, o;
        if (!GetTargetPosition(e, x, y, z, o))
            break;

        ObjectList targets = GetTargets(
            CreateEvent(SMART_EVENT_UPDATE_IC, 0, 0, 0, 0, 0, SMART_ACTION_NONE,
                0, 0, 0, 0, 0, 0, (SMARTAI_TARGETS)e.action.teleport.targetType,
                e.action.teleport.targetParam1, e.action.teleport.targetParam2,
                e.action.teleport.targetParam3, 0, 0, 0),
            unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (IsPlayer(*itr))
            {
                if (e.action.teleport.mapID)
                    ((Player*)*itr)
                        ->TeleportTo(e.action.teleport.mapID, x, y, z, o);
                else
                    logging.error(
                        "SmartAI: SMART_ACTION_TELEPORT (Entryorguid: %u "
                        "sourceType: %u id: %u) teleports player yet uses an "
                        "empty mapId. Teleport skipped.",
                        e.entryOrGuid, e.source_type, e.event_id);
            }
            else if (IsCreature(*itr))
            {
                ((Creature*)*itr)->NearTeleportTo(x, y, z, o);
            }
        }

        break;
    }
    case SMART_ACTION_SET_FLY:
    {
        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (IsCreature(*itr))
            {
                Creature* c = (Creature*)*itr;
                if (e.action.setRun.run)
                    c->SetLevitate(true);
                else
                    c->SetLevitate(false);
            }
        }

        break;
    }
    case SMART_ACTION_SET_RUN:
    {
        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (IsCreature(*itr))
            {
                Creature* c = (Creature*)*itr;
                if (c->AI() && dynamic_cast<SmartAI*>(c->AI()) != nullptr)
                    static_cast<SmartAI*>(c->AI())->SetRun(e.action.setRun.run);
                else
                    c->SetWalk(!e.action.setRun.run);
            }
        }

        break;
    }
    case SMART_ACTION_SET_SWIM:
    {
        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (IsCreature(*itr))
            {
                Creature* c = (Creature*)*itr;
                if (e.action.setRun.run)
                    c->SetSwim(true);
                else
                    c->SetSwim(false);
            }
        }

        break;
    }
    case SMART_ACTION_WP_START:
    {
        if (!IsSmart())
            break;

        ObjectList targets = GetTargets(e, unit);

        WorldObject* target = nullptr;
        if (!targets.empty())
            target = *targets.begin();

        // If target list doesn't contain a player, switch to invoker
        if (e.action.wpStart.quest &&
            (!target || target->GetTypeId() != TYPEID_PLAYER))
            target = unit;

        if (e.action.wpStart.quest &&
            (!target || target->GetTypeId() != TYPEID_PLAYER))
        {
            logging.error(
                "SmartAI: SMART_ACTION_WP_START (Entryorguid: %u sourceType: "
                "%u id: %u) has a quest id but target is not a player.",
                e.entryOrGuid, e.source_type, e.event_id);
            break;
        }

        bool run = e.action.wpStart.run ? true : false;
        uint32 entry = e.action.wpStart.pathID;
        bool repeat = e.action.wpStart.repeat ? true : false;

        // FIXME: React state for escort
        // me->GetCharmInfo()->SetReactState((ReactStates)e.action.wpStart.reactState);

        ((SmartAI*)me->AI())->StartPath(run, entry, repeat, (Unit*)target);

        ((SmartAI*)me->AI())->mEscortQuestID = e.action.wpStart.quest;
        if (e.action.wpStart.despawnTime)
            ((SmartAI*)me->AI())
                ->SetEscortDespawnTime(e.action.wpStart.despawnTime);
        break;
    }
    case SMART_ACTION_WP_PAUSE:
    {
        if (!IsSmart())
            break;

        ((SmartAI*)me->AI())
            ->PausePath(e.action.wpPause.delay,
                e.GetEventType() == SMART_EVENT_WAYPOINT_REACHED ? false :
                                                                   true);
        break;
    }
    case SMART_ACTION_WP_STOP:
    {
        if (!IsSmart())
            break;

        ((SmartAI*)me->AI())
            ->StopPath(e.action.wpStop.despawnTime, e.action.wpStop.quest,
                e.action.wpStop.fail ? true : false);
        break;
    }
    case SMART_ACTION_WP_RESUME:
    {
        if (!IsSmart())
            break;

        ((SmartAI*)me->AI())
            ->RemoveEscortState(SMART_ESCORT_PAUSED | SMART_ESCORT_RETURNING);
        ((SmartAI*)me->AI())->ResumePath();
        break;
    }
    case SMART_ACTION_WP_SET_RUN:
    {
        if (!IsSmart())
            break;

        ((SmartAI*)me->AI())->UpdatePathRunMode(e.action.setRun.run);
        break;
    }
    case SMART_ACTION_SET_ORIENTATION:
    {
        if (!me)
            break;

        ObjectList targets = GetTargets(e, unit);
        float new_o;

        if (e.GetTargetType() == SMART_TARGET_POSITION)
            new_o = e.target.o;
        else if (!targets.empty())
            new_o = me->GetAngle(*targets.begin());
        else
            break;

        me->SetFacingTo(new_o);

        if (e.action.raw.param1)
        {
            uint32 event_mask = !me->isInCombat() ?
                                    movement::EVENT_ENTER_COMBAT :
                                    movement::EVENT_LEAVE_COMBAT;

            me->movement_gens.push(
                new movement::StoppedMovementGenerator(
                    e.action.raw.param1, -(int)e.action.raw.param1),
                event_mask);
        }
        else if (me->movement_gens.has(movement::gen::idle))
        {
            me->movement_gens.remove_all(movement::gen::idle);
            me->movement_gens.push(new movement::IdleMovementGenerator(
                me->GetX(), me->GetY(), me->GetZ(), new_o));
        }

        break;
    }
    case SMART_ACTION_PLAYMOVIE:
    {
        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (!IsPlayer(*itr))
                continue;

            ((Player*)*itr)->SendCinematicStart(e.action.movie.entry);
        }

        break;
    }
    case SMART_ACTION_PUSH_POINT_GEN:
    {
        if (!IsSmart())
            break;

        WorldObject* target = nullptr;
        ObjectList targets = GetTargets(e, unit);
        if (!targets.empty())
            target = targets.front();

        bool run = static_cast<SmartAI*>(me->AI())->GetRun() ||
                   e.action.PointGen.settings == 1 ||
                   e.action.PointGen.settings == 3;

        float x, y, z;

        if (!target)
        {
            float o;
            if (!GetTargetPosition(e, x, y, z, o))
                break;
        }
        else
        {
            if (e.action.PointGen.interactDistance)
            {
                G3D::Vector3 target_pos;
                target->GetPosition(target_pos.x, target_pos.y, target_pos.z);
                auto pos = target->GetPointXYZ(target_pos, target->GetAngle(me),
                    e.action.PointGen.interactDistance, true);
                x = pos.x;
                y = pos.y;
                z = pos.z;
            }
            else
            {
                x = target->GetX() + e.target.x;
                y = target->GetY() + e.target.y;
                z = target->GetZ() + e.target.z;
            }
        }

        uint32 event_mask = 0;
        if (e.action.PointGen.settings <= 1)
            event_mask = !me->isInCombat() ? movement::EVENT_ENTER_COMBAT :
                                             movement::EVENT_LEAVE_COMBAT;

        if (e.action.PointGen.stoppedMs > 0)
            me->movement_gens.push(new movement::StoppedMovementGenerator(
                                       e.action.PointGen.stoppedMs,
                                       -(int)e.action.PointGen.stoppedMs),
                event_mask);

        me->movement_gens.push(
            new movement::PointMovementGenerator(e.action.PointGen.pointId, x,
                y, z, e.action.PointGen.useMmaps, run),
            event_mask, e.action.PointGen.prio);

        break;
    }
    case SMART_ACTION_RESPAWN_TARGET:
    {
        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (IsCreature(*itr))
            {
                Creature* c = (Creature*)*itr;
                c->Respawn();
            }
            else if (IsGameObject(*itr))
            {
                auto go = (GameObject*)*itr;
                if (e.action.RespawnTarget.goRespawnTime)
                {
                    go->SetRespawnTime(e.action.RespawnTarget.goRespawnTime);
                    go->Refresh();
                }
                else
                    go->Respawn();
            }
        }

        break;
    }
    case SMART_ACTION_CLOSE_GOSSIP:
    {
        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
            if (IsPlayer(*itr))
                ((Player*)*itr)->PlayerTalkClass->CloseGossip();

        break;
    }
    case SMART_ACTION_EQUIP:
    {
        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if ((*itr)->GetTypeId() == TYPEID_UNIT)
            {
                Creature* npc = (Creature*)*itr;
                if (e.action.equip.entry)
                {
                    npc->SetCurrentEquipmentId(e.action.equip.entry);
                    npc->LoadEquipment(e.action.equip.entry);
                }
                else if (!e.action.equip.slot1 && !e.action.equip.slot2 &&
                         !e.action.equip.slot3 && !e.action.equip.mask)
                {
                    // Reload old equipment
                    npc->SetCurrentEquipmentId(
                        npc->GetCreatureInfo()->equipmentId);
                    npc->LoadEquipment(
                        npc->GetCreatureInfo()->equipmentId, true);
                }
                else
                {
                    npc->SetCurrentEquipmentId(0);
                    if (!e.action.equip.mask || e.action.equip.mask & 1)
                        npc->SetVirtualItem(
                            VIRTUAL_ITEM_SLOT_0, e.action.equip.slot1);
                    if (!e.action.equip.mask || e.action.equip.mask & 2)
                        npc->SetVirtualItem(
                            VIRTUAL_ITEM_SLOT_1, e.action.equip.slot2);
                    if (!e.action.equip.mask || e.action.equip.mask & 4)
                        npc->SetVirtualItem(
                            VIRTUAL_ITEM_SLOT_2, e.action.equip.slot3);
                }
            }
        }

        break;
    }
    case SMART_ACTION_CREATE_TIMED_EVENT:
    {
        SmartEvent ne;
        ne.type = (SMART_EVENT)SMART_EVENT_UPDATE;
        ne.event_chance = e.action.timeEvent.chance;
        if (!ne.event_chance)
            ne.event_chance = 100;

        ne.minMaxRepeat.min = e.action.timeEvent.min;
        ne.minMaxRepeat.max = e.action.timeEvent.max;
        ne.minMaxRepeat.repeatMin = e.action.timeEvent.repeatMin;
        ne.minMaxRepeat.repeatMax = e.action.timeEvent.repeatMax;

        ne.event_flags = 0;
        if (!ne.minMaxRepeat.repeatMin && !ne.minMaxRepeat.repeatMax)
            ne.event_flags |= SMART_EVENT_FLAG_NOT_REPEATABLE;

        SmartAction ac;
        ac.type = (SMART_ACTION)SMART_ACTION_TRIGGER_TIMED_EVENT;
        ac.timeEvent.id = e.action.timeEvent.id;

        SmartScriptHolder ev;
        ev.event = ne;
        ev.event_id = e.action.timeEvent.id;
        ev.target = e.target;
        ev.action = ac;
        InitTimer(ev);
        mStoredEvents.push_back(ev);
        break;
    }
    case SMART_ACTION_TRIGGER_TIMED_EVENT:
        ProcessEventsFor((SMART_EVENT)SMART_EVENT_TIMED_EVENT_TRIGGERED,
            nullptr, e.action.timeEvent.id);
        break;
    case SMART_ACTION_REMOVE_TIMED_EVENT:
        mRemIDs.push_back(e.action.timeEvent.id);
        break;
    case SMART_ACTION_CALL_SCRIPT_RESET:
        OnReset(SMART_RESET_TYPE_SCRIPT);
        break;
    case SMART_ACTION_CALL_TIMED_ACTIONLIST:
    {
        if (e.GetTargetType() == SMART_TARGET_NONE)
        {
            logging.error(
                "SmartScript: Entry %d SourceType %u Event %u Action %u is "
                "using TARGET_NONE(0) for Script9 target. Please correct "
                "target_type in database.",
                e.entryOrGuid, e.GetScriptType(), e.GetEventType(),
                e.GetActionType());
            break;
        }

        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (Creature* target = dynamic_cast<Creature*>(*itr))
            {
                if (IsSmart(target))
                    ((SmartAI*)target->AI())
                        ->SetScript9(
                            e, e.action.timedActionList.id, GetLastInvoker());
            }
            /*else if (GameObject* goTarget = (*itr)->ToGameObject())
            {
                if (IsSmartGO(goTarget))
                    CAST_AI(SmartGameObjectAI, goTarget->AI())->SetScript9(e,
            e.action.timedActionList.id, GetLastInvoker());
            }*/
        }

        break;
    }
    case SMART_ACTION_SET_NPC_FLAG:
    {
        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
            if (IsUnit(*itr))
                ((Unit*)*itr)
                    ->SetUInt32Value(UNIT_NPC_FLAGS, e.action.unitFlag.flag);

        break;
    }
    case SMART_ACTION_ADD_NPC_FLAG:
    {
        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
            if (IsUnit(*itr))
                ((Unit*)*itr)->SetFlag(UNIT_NPC_FLAGS, e.action.unitFlag.flag);

        break;
    }
    case SMART_ACTION_REMOVE_NPC_FLAG:
    {
        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
            if (IsUnit(*itr))
                ((Unit*)*itr)
                    ->RemoveFlag(UNIT_NPC_FLAGS, e.action.unitFlag.flag);

        break;
    }
    case SMART_ACTION_CROSS_CAST:
    {
        ObjectList casters = GetTargets(
            CreateEvent(SMART_EVENT_UPDATE_IC, 0, 0, 0, 0, 0, SMART_ACTION_NONE,
                0, 0, 0, 0, 0, 0, (SMARTAI_TARGETS)e.action.cast.targetType,
                e.action.cast.targetParam1, e.action.cast.targetParam2,
                e.action.cast.targetParam3, e.target.x, e.target.y, 0),
            unit);
        if (casters.empty())
            break;

        ObjectList targets = GetTargets(e, unit);
        if (targets.empty())
            break;

        for (ObjectList::const_iterator itr = casters.begin();
             itr != casters.end(); ++itr)
        {
            if (IsUnit(*itr))
            {
                if (e.action.cast.flags & SMARTCAST_INTERRUPT_PREVIOUS)
                    ((Unit*)*itr)->InterruptNonMeleeSpells(false);

                for (ObjectList::const_iterator it = targets.begin();
                     it != targets.end(); ++it)
                {
                    if (IsUnit(*it))
                    {
                        if (!(e.action.cast.flags &
                                SMARTCAST_AURA_NOT_PRESENT) ||
                            !((Unit*)*it)->has_aura(e.action.cast.spell))
                        {
                            if (e.action.cast.flags & SMARTCAST_USE_DESTINATION)
                            {
                                float x, y, z;
                                static_cast<Unit*>(*it)->GetPosition(x, y, z);
                                static_cast<Unit*>(*itr)->CastSpell(
                                    x, y, z, e.action.cast.spell,
                                    (e.action.cast.flags &
                                        SMARTCAST_TRIGGERED) ?
                                        true :
                                        false);
                            }
                            else
                            {
                                ((Unit*)*itr)
                                    ->CastSpell(((Unit*)*it),
                                        e.action.cast.spell,
                                        (e.action.cast.flags &
                                                    SMARTCAST_TRIGGERED) ?
                                            true :
                                            false);
                            }
                        }
                        else
                        {
                            LOG_DEBUG(logging,
                                "Spell %u not casted because it has flag "
                                "SMARTCAST_AURA_NOT_PRESENT and the target "
                                "(Guid: " UI64FMTD
                                " Entry: %u Type: %u) already has the aura",
                                e.action.cast.spell,
                                (*it)->GetObjectGuid().GetRawValue(),
                                (*it)->GetEntry(), uint32((*it)->GetTypeId()));
                        }
                    }
                }
            }
        }

        break;
    }
    case SMART_ACTION_CALL_RANDOM_TIMED_ACTIONLIST:
    {
        uint32 actions[SMART_ACTION_PARAM_COUNT];
        actions[0] = e.action.randTimedActionList.entry1;
        actions[1] = e.action.randTimedActionList.entry2;
        actions[2] = e.action.randTimedActionList.entry3;
        actions[3] = e.action.randTimedActionList.entry4;
        actions[4] = e.action.randTimedActionList.entry5;
        actions[5] = e.action.randTimedActionList.entry6;
        uint32 temp[SMART_ACTION_PARAM_COUNT];
        uint32 count = 0;
        for (auto& action : actions)
        {
            if (action > 0)
            {
                temp[count] = action;
                ++count;
            }
        }

        uint32 id = temp[urand(0, count)];
        if (e.GetTargetType() == SMART_TARGET_NONE)
        {
            logging.error(
                "SmartScript: Entry %d SourceType %u Event %u Action %u is "
                "using TARGET_NONE(0) for Script9 target. Please correct "
                "target_type in database.",
                e.entryOrGuid, e.GetScriptType(), e.GetEventType(),
                e.GetActionType());
            break;
        }

        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (Creature* target = dynamic_cast<Creature*>(*itr))
            {
                if (IsSmart(target))
                    ((SmartAI*)target->AI())
                        ->SetScript9(e, id, GetLastInvoker());
            }
            /*else if (GameObject* goTarget = (*itr)->ToGameObject())
            {
                if (IsSmartGO(goTarget))
                    CAST_AI(SmartGameObjectAI, goTarget->AI())->SetScript9(e,
            id, GetLastInvoker());
            }*/
        }

        break;
    }
    case SMART_ACTION_CALL_RANDOM_RANGE_TIMED_ACTIONLIST:
    {
        uint32 id = urand(e.action.randTimedActionList.entry1,
            e.action.randTimedActionList.entry2);
        if (e.GetTargetType() == SMART_TARGET_NONE)
        {
            logging.error(
                "SmartScript: Entry %d SourceType %u Event %u Action %u is "
                "using TARGET_NONE(0) for Script9 target. Please correct "
                "target_type in database.",
                e.entryOrGuid, e.GetScriptType(), e.GetEventType(),
                e.GetActionType());
            break;
        }

        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (Creature* target = dynamic_cast<Creature*>(*itr))
            {
                if (IsSmart(target))
                    ((SmartAI*)target->AI())
                        ->SetScript9(e, id, GetLastInvoker());
            }
            /*else if (GameObject* goTarget = (*itr)->ToGameObject())
            {
                if (IsSmartGO(goTarget))
                    CAST_AI(SmartGameObjectAI, goTarget->AI())->SetScript9(e,
            id, GetLastInvoker());
            }*/
        }

        break;
    }
    case SMART_ACTION_ACTIVATE_TAXI:
    {
        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
            if (IsPlayer(*itr))
                ((Player*)*itr)->ActivateTaxiPathTo(e.action.taxi.id);

        break;
    }
    case SMART_ACTION_PUSH_RANDOM_GEN:
    {
        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (IsCreature((*itr)))
            {
                auto c = static_cast<Creature*>(*itr);
                c->movement_gens.push(
                    new movement::RandomMovementGenerator(
                        (float)e.action.raw.param1,
                        G3D::Vector3(e.target.x, e.target.y, e.target.z)),
                    e.action.raw.param3, e.action.raw.param2);
            }
        }

        break;
    }
    case SMART_ACTION_SET_UNIT_FIELD_BYTES_1:
    {
        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
            if (IsUnit(*itr))
                ((Unit*)*itr)
                    ->SetByteFlag(UNIT_FIELD_BYTES_1, e.action.setunitByte.type,
                        e.action.setunitByte.byte1);

        break;
    }
    case SMART_ACTION_REMOVE_UNIT_FIELD_BYTES_1:
    {
        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
            if (IsUnit(*itr))
                ((Unit*)*itr)
                    ->RemoveByteFlag(UNIT_FIELD_BYTES_1,
                        e.action.delunitByte.type, e.action.delunitByte.byte1);

        break;
    }
    case SMART_ACTION_INTERRUPT_SPELL:
    {
        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
            if (IsUnit(*itr))
                ((Unit*)*itr)
                    ->InterruptNonMeleeSpells(
                        e.action.interruptSpellCasting.withDelayed,
                        e.action.interruptSpellCasting
                            .spell_id /*, e.action.interruptSpellCasting.withInstant*/);

        break;
    }
    case SMART_ACTION_SEND_GO_CUSTOM_ANIM:
    {
        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
            if (IsGameObject(*itr))
                (*itr)->SendGameObjectCustomAnim(
                    (*itr)->GetObjectGuid(), e.action.sendGoCustomAnim.anim);

        break;
    }
    case SMART_ACTION_SET_DYNAMIC_FLAG:
    {
        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
            if (IsUnit(*itr))
                ((Unit*)*itr)
                    ->SetUInt32Value(
                        UNIT_DYNAMIC_FLAGS, e.action.unitFlag.flag);

        break;
    }
    case SMART_ACTION_ADD_DYNAMIC_FLAG:
    {
        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
            if (IsUnit(*itr))
                ((Unit*)*itr)
                    ->SetFlag(UNIT_DYNAMIC_FLAGS, e.action.unitFlag.flag);

        break;
    }
    case SMART_ACTION_REMOVE_DYNAMIC_FLAG:
    {
        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
            if (IsUnit(*itr))
                ((Unit*)*itr)
                    ->RemoveFlag(UNIT_DYNAMIC_FLAGS, e.action.unitFlag.flag);

        break;
    }
    /*case SMART_ACTION_JUMP_TO_POS:
    {
        if (!me)
            break;

        me->GetMotionMaster()->Clear();
        float x, y, z, o;
        if (GetTargetPosition(e, x, y, z, o))
            me->GetMotionMaster()->MoveJump(x, y, z,
    (float)e.action.jump.speedxy, (float)e.action.jump.speedz);
        // TODO: Resume path when reached jump location
        break;
    }*/
    case SMART_ACTION_GO_SET_LOOT_STATE:
    {
        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
            if (IsGameObject(*itr))
                ((GameObject*)*itr)
                    ->SetLootState((LootState)e.action.setGoLootState.state);

        break;
    }
    case SMART_ACTION_SEND_TARGET_TO_TARGET:
    {
        ObjectList targets = GetTargets(e, unit);

        ObjectList storedTargets =
            GetStoredTargets(e.action.sendTargetToTarget.id);
        if (storedTargets.empty())
            return;

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (IsCreature(*itr))
            {
                Creature* c = (Creature*)*itr;
                if (!IsSmart(c))
                    continue;

                ((SmartAI*)c->AI())
                    ->GetScript()
                    ->StoreTargets(
                        storedTargets, e.action.sendTargetToTarget.id);
            }
            else if (IsGameObject(*itr))
            {
                GameObject* g = (GameObject*)*itr;
                if (!IsSmartGO(g))
                    continue;

                ((SmartGameObjectAI*)g->AI())
                    ->GetScript()
                    ->StoreTargets(
                        storedTargets, e.action.sendTargetToTarget.id);
            }
        }

        break;
    }
    case SMART_ACTION_SEND_GOSSIP_MENU:
    {
        if (!GetBaseObject())
            break;

        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (IsPlayer(*itr))
            {
                Player* player = (Player*)*itr;
                if (e.action.sendGossipMenu.gossipMenuId)
                {
                    player->PrepareGossipMenu(
                        GetBaseObject(), e.action.sendGossipMenu.gossipMenuId);
                    player->SendPreparedGossip(GetBaseObject());
                }
                else
                {
                    player->PlayerTalkClass->ClearMenus();
                    player->PlayerTalkClass->SendGossipMenu(
                        e.action.sendGossipMenu.gossipNpcTextId,
                        GetBaseObject()->GetObjectGuid());
                }
            }
        }

        break;
    }
    case SMART_ACTION_SET_HEALTH_REGEN:
    {
        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (IsCreature(*itr))
                ((Creature*)*itr)
                    ->SetRegeneratingHealth(
                        e.action.setHealthRegen.regenHealth ? true : false);
        }

        break;
    }
    case SMART_ACTION_NOTIFY_AI:
    {
        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (IsCreature(*itr))
            {
                Creature* c = (Creature*)*itr;
                if (!c->AI())
                    continue;

                if (SmartAI* cAI = dynamic_cast<SmartAI*>(c->AI()))
                {
                    cAI->GetScript()->ProcessEventsFor(
                        SMART_EVENT_AI_NOTIFICATION, me ? me : nullptr,
                        e.action.notifyAI.id);
                }
                else if (c->AI())
                {
                    c->AI()->Notify(e.action.notifyAI.id, me ? me : nullptr);
                }
            }
            else if (IsGameObject(*itr))
            {
                GameObject* g = (GameObject*)*itr;
                if (!g->AI())
                    continue;

                if (SmartGameObjectAI* gAI =
                        dynamic_cast<SmartGameObjectAI*>(g->AI()))
                    gAI->GetScript()->ProcessEventsFor(
                        SMART_EVENT_AI_NOTIFICATION, me ? me : nullptr,
                        e.action.notifyAI.id);
            }
        }

        break;
    }
    case SMART_ACTION_SET_AGGRO_DISTANCE:
    {
        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (IsCreature(*itr))
                ((Creature*)*itr)
                    ->SetAggroDistance((float)e.action.aggroDistance.dist);
        }

        break;
    }
    case SMART_ACTION_SWITCH_TARGET:
    {
        if (!me || me->GetTypeId() != TYPEID_UNIT || !me->AI())
            break;

        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            // Take the first best target
            if (IsUnit(*itr))
            {
                if (e.action.switchTarget.dropHostility)
                {
                    me->DeleteThreatList();
                    me->CombatStop(true);
                    me->AI()->AttackStart((Unit*)*itr);
                }
                else
                    me->getThreatManager().tauntTransferAggro((Unit*)*itr);
                me->getThreatManager().addThreatDirectly(
                    (Unit*)*itr, e.action.switchTarget.aggroIncrease);
                break; // Only take one unit
            }
        }

        break;
    }
    case SMART_ACTION_SPAR:
    {
        if (!me || !IsSmart() || me->isInCombat())
            break;

        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (IsCreature(*itr))
            {
                auto unit = static_cast<Creature*>(*itr);
                if (unit == me || unit->isInCombat())
                    continue;
                if (auto ai = dynamic_cast<SmartAI*>(unit->AI()))
                {
                    me->AI()->AttackStart(unit);
                    static_cast<SmartAI*>(me->AI())->SetSparring();
                    ai->SetSparring();
                    break;
                }
            }
        }

        break;
    }
    case SMART_ACTION_RANDOM_ACTION_IN_RANGE:
    {
        uint32 id = urand(
            e.action.randomAction.minAction, e.action.randomAction.maxAction);
        for (auto& elem : mEvents)
        {
            SMART_EVENT eventType = SMART_EVENT((elem).GetEventType());
            if (eventType != SMART_EVENT_LINK)
                continue;

            if ((elem).event_id == id)
            {
                // Note: We skip condition checking, and hold the invoking event
                // responsible for that.

                ProcessEvent(elem);
                break;
            }
        }

        break;
    }
    case SMART_ACTION_SET_HEALTH:
    {
        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (IsUnit(*itr))
            {
                auto u = static_cast<Unit*>(*itr);
                int max = (int)u->GetMaxHealth();
                int hp = (int)u->GetHealth();

                if (e.action.setHealth.flatHp)
                    hp = e.action.setHealth.flatHp;
                else if (e.action.setHealth.pctHp)
                    hp = (e.action.setHealth.pctHp / 100.0f) * max;
                else if (e.action.setHealth.flatIncr)
                    hp += e.action.setHealth.flatIncr;
                else if (e.action.setHealth.flatDecr)
                    hp -= e.action.setHealth.flatDecr;
                else if (e.action.setHealth.pctIncr)
                    hp += (e.action.setHealth.pctIncr / 100.0f) * max;
                else if (e.action.setHealth.pctDecr)
                    hp -= (e.action.setHealth.pctDecr / 100.0f) * max;

                if (hp > max)
                    hp = max;

                // Respect invincibility level
                if (u->GetTypeId() == TYPEID_UNIT)
                {
                    if (auto smartai = dynamic_cast<SmartAI*>(
                            static_cast<Creature*>(u)->AI()))
                    {
                        if (smartai->GetInvincibilityHpLevel() &&
                            (int)smartai->GetInvincibilityHpLevel() > hp)
                        {
                            hp = (int)smartai->GetInvincibilityHpLevel();
                        }
                    }
                }

                if (hp <= 0)
                    u->Kill(u);
                else
                    u->SetHealth(hp);
            }
        }

        break;
    }
    case SMART_ACTION_SET_STAND_STATE:
    {
        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (IsUnit(*itr))
            {
                ((Unit*)*itr)->SetStandState(e.action.standState.state);
            }
        }

        break;
    }
    case SMART_ACTION_BEHAVIORAL_TOGGLE:
    {
        if (!IsSmart())
            break;

        static_cast<SmartAI*>(me->AI())->ToggleBehavioralAI(
            e.action.behaviorToggle.state);
        break;
    }
    case SMART_ACTION_BEHAVIORAL_CHANGE_BEHAVIOR:
    {
        if (!IsSmart())
            break;

        static_cast<SmartAI*>(me->AI())->ToggleBehavioralAI(
            e.action.behaviorChange.behavior);
        break;
    }
    case SMART_ACTION_DISABLE_COMBAT_REACTIONS:
    {
        if (!me || !IsSmart())
            break;
        ((SmartAI*)me->AI())
            ->DisableCombatReactions(e.action.combatReactions.disable);
        break;
    }
    case SMART_ACTION_DISENGAGE:
    {
        if (!me || me->GetTypeId() != TYPEID_UNIT || !me->AI() || !IsSmart())
            break;

        ((SmartAI*)me->AI())
            ->Disengage(e.action.disengage.distance,
                e.action.disengage.toggleAutoAttack);

        break;
    }
    case SMART_ACTION_SET_FOCUS_TARGET:
    {
        if (!me)
            break;

        if (e.GetTargetType() == SMART_TARGET_NONE)
        {
            me->SetFocusTarget(nullptr);
            break;
        }

        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (IsUnit(*itr))
            {
                me->SetFocusTarget((Unit*)*itr);
                break; // Can only set one focus target
            }
        }

        break;
    }
    case SMART_ACTION_SAVE_CURRENT_PHASE:
    {
        SaveCurrentPhase(e.action.phaseSaveLoad.phaseId);
        break;
    }
    case SMART_ACTION_LOAD_SAVED_PHASE:
    {
        LoadSavedPhase(e.action.phaseSaveLoad.phaseId);
        break;
    }
    case SMART_ACTION_PAUSE_GROUP_MOVEMENT:
    {
        ObjectList targets = GetTargets(e, unit);

        std::set<int32>
            alreadyPausedGrps; // Don't pause the same group multiple times

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (IsCreature(*itr))
            {
                Creature* c = (Creature*)*itr;
                if (c->GetGroup() != nullptr &&
                    alreadyPausedGrps.find(c->GetGroup()->GetId()) ==
                        alreadyPausedGrps.end())
                {
                    c->GetMap()->GetCreatureGroupMgr().ClearCurrentPauses(
                        c->GetGroup()->GetId());
                    c->GetMap()->GetCreatureGroupMgr().PauseMovementOfGroup(
                        c->GetGroup()->GetId(), e.action.timer.milliseconds);
                    alreadyPausedGrps.insert(c->GetGroup()->GetId());
                }
            }
        }

        break;
    }
    case SMART_ACTION_TOGGLE_PET_BEHAVIOR:
    {
        if (!me || !IsSmart() || !static_cast<Creature*>(me)->IsPet() ||
            !static_cast<Pet*>(me)->behavior())
            return;

        if (e.action.onOff.boolean)
            static_cast<Pet*>(me)->behavior()->resume();
        else
            static_cast<Pet*>(me)->behavior()->pause();

        break;
    }
    case SMART_ACTION_TEMPSUMMON_LEASH:
    {
        if (!me || !IsSmart())
            return;

        if (!dynamic_cast<TemporarySummon*>(me))
        {
            logging.error(
                "SmartScript::ProcessAction: Entry %d SourceType %u, Event %u, "
                "Action 123 called for non-temporary summon.",
                e.entryOrGuid, e.GetScriptType(), e.event_id);
            return;
        }

        float x, y, z, o;
        if (GetTargetPosition(e, x, y, z, o))
        {
            me->set_leash(x, y, z, (float)e.action.radius.radius);
            return;
        }

        ObjectList targets = GetTargets(e, unit);
        if (targets.empty())
            return;

        WorldObject* obj = targets[0];

        me->set_leash(obj->GetX(), obj->GetY(), obj->GetZ(),
            (float)e.action.radius.radius);
        break;
    }
    case SMART_ACTION_SAVE_POS:
    {
        if (!me || !IsSmart())
            return;
        ((SmartAI*)me->AI())
            ->save_pos(me->GetX(), me->GetY(), me->GetZ(), me->GetO());
        break;
    }
    case SMART_ACTION_POP_MOVE_GENS:
    {
        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (IsCreature((*itr)))
            {
                auto c = static_cast<Creature*>(*itr);
                c->movement_gens.remove_if([&e](const movement::Generator* gen)
                    {
                        return gen->id() ==
                               static_cast<movement::gen>(e.action.raw.param1);
                    });
            }
        }
        break;
    }
    case SMART_ACTION_PUSH_STOPPED_GEN:
    {
        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (IsCreature((*itr)))
            {
                int prio = e.action.raw.param1;
                auto c = static_cast<Creature*>(*itr);

                uint32 auto_remove = 0;
                if (e.action.raw.param3 == 1 && c->isInCombat())
                    auto_remove = movement::EVENT_LEAVE_COMBAT;
                else if (e.action.raw.param3 == 2)
                    auto_remove = !c->isInCombat() ?
                                      movement::EVENT_ENTER_COMBAT :
                                      movement::EVENT_LEAVE_COMBAT;

                c->movement_gens.push(
                    new movement::StoppedMovementGenerator(
                        e.action.raw.param2, e.action.raw.param4),
                    auto_remove, prio);
            }
        }
        break;
    }
    case SMART_ACTION_PUSH_IDLE_GEN:
    {
        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (IsCreature((*itr)))
            {
                auto c = static_cast<Creature*>(*itr);
                uint32 auto_remove = 0;
                if (e.action.raw.param2 == 1)
                    auto_remove = !c->isInCombat() ?
                                      movement::EVENT_ENTER_COMBAT :
                                      movement::EVENT_LEAVE_COMBAT;
                c->movement_gens.push(
                    new movement::IdleMovementGenerator(
                        e.target.x, e.target.y, e.target.z, e.target.o),
                    auto_remove, e.action.raw.param1);
            }
        }
        break;
    }
    case SMART_ACTION_PUSH_FALL_GEN:
    {
        ObjectList targets = GetTargets(e, unit);

        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (IsCreature((*itr)))
            {
                auto c = static_cast<Creature*>(*itr);
                c->movement_gens.push(new movement::FallMovementGenerator(), 0,
                    e.action.raw.param1);
            }
        }
        break;
    }
    case SMART_ACTION_GROUP_CREATE:
    {
        if (!me || me->GetGroup() || !IsSmart(me))
            return;
        std::string name = std::string("SmartAI group, created by: ") +
                           me->GetName() + " (entry: " +
                           std::to_string(me->GetEntry()) + ", guid: " +
                           std::to_string(me->GetGUIDLow()) + ")";
        int group =
            me->GetMap()->GetCreatureGroupMgr().CreateNewGroup(name, true);
        if (auto grp = me->GetMap()->GetCreatureGroupMgr().GetGroup(group))
        {
            static_cast<SmartAI*>(me->AI())->SetGroup(group);
            grp->AddMember(me, false);
            grp->SetLeader(me, false);
        }
        break;
    }
    case SMART_ACTION_GROUP_INVITE:
    {
        if (!me || !me->GetGroup() || !IsSmart(me))
            return;

        auto group = me->GetGroup();
        if (group->GetId() != static_cast<SmartAI*>(me->AI())->GetGroup())
            return;

        if (group->HasFlag(CREATURE_GROUP_FLAG_GROUP_MOVEMENT))
            return;

        ObjectList targets = GetTargets(e, unit);
        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (IsCreature((*itr)))
            {
                auto c = static_cast<Creature*>(*itr);
                auto ai = dynamic_cast<SmartAI*>(c->AI());
                if (c->isAlive() && !c->GetGroup() && ai)
                {
                    group->AddMember(c, false);
                    ai->SetGroup(group->GetId());
                }
            }
        }
        break;
    }
    case SMART_ACTION_GROUP_LEAVE:
    {
        if (!me || !me->GetGroup() || !IsSmart(me))
            return;

        auto group = me->GetGroup();
        if (group->GetId() != static_cast<SmartAI*>(me->AI())->GetGroup())
            return;

        if (group->HasFlag(CREATURE_GROUP_FLAG_GROUP_MOVEMENT))
            return;

        ObjectList targets = GetTargets(e, unit);
        for (ObjectList::const_iterator itr = targets.begin();
             itr != targets.end(); ++itr)
        {
            if (IsCreature((*itr)))
            {
                auto c = static_cast<Creature*>(*itr);
                auto ai = dynamic_cast<SmartAI*>(c->AI());
                if (c->GetGroup() && c->GetGroup()->GetId() == group->GetId() &&
                    ai)
                {
                    group->RemoveMember(c, false);
                    if (auto ai = dynamic_cast<SmartAI*>(c->AI()))
                        ai->SetGroup(0);
                }
            }
        }

        if (group->GetSize() == 0)
            me->GetMap()->GetCreatureGroupMgr().DeleteGroup(group->GetId());
        break;
    }
    case SMART_ACTION_GROUP_DISBAND:
    {
        if (!me || !me->GetGroup() || !IsSmart(me))
            return;

        auto group = me->GetGroup();
        if (group->GetId() != static_cast<SmartAI*>(me->AI())->GetGroup())
            return;

        if (group->HasFlag(CREATURE_GROUP_FLAG_GROUP_MOVEMENT))
            return;

        for (auto c : group->GetMembers())
            if (auto ai = dynamic_cast<SmartAI*>(c->AI()))
                ai->SetGroup(0);

        me->GetMap()->GetCreatureGroupMgr().DeleteGroup(group->GetId());
        break;
    }
    case SMART_ACTION_GROUP_MOVE_IN_FORMATION:
    {
        if (!me || !me->GetGroup() || !IsSmart(me))
            return;

        auto group = me->GetGroup();
        if (group->GetId() != static_cast<SmartAI*>(me->AI())->GetGroup())
            return;

        auto wps =
            sSmartGroupWaypointMgr::Instance()->GetPath(e.action.raw.param1);
        if (!wps)
            return;

        if (group->HasFlag(CREATURE_GROUP_FLAG_GROUP_MOVEMENT))
            return;
        group->AddFlag(CREATURE_GROUP_FLAG_GROUP_MOVEMENT);

        for (auto& wp : *wps)
            me->GetMap()->GetCreatureGroupMgr().GetMovementMgr().AddWaypoint(
                group->GetId(), wp, wp.mmap);

        for (auto c : group->GetMembers())
        {
            c->movement_gens.remove_if([](auto*)
                {
                    return true;
                });
            c->GetMap()->GetCreatureGroupMgr().GetMovementMgr().AddCreature(
                group->GetId(), c);
        }

        me->GetMap()->GetCreatureGroupMgr().GetMovementMgr().StartMovement(
            group->GetId(), group->GetMembers());
        break;
    }
    case SMART_ACTION_GROUP_ABANDON_FORMATION:
    {
        if (!me || !me->GetGroup() || !IsSmart(me))
            return;

        auto group = me->GetGroup();
        if (group->GetId() != static_cast<SmartAI*>(me->AI())->GetGroup())
            return;

        if (!group->HasFlag(CREATURE_GROUP_FLAG_GROUP_MOVEMENT))
            return;
        group->RemoveFlag(CREATURE_GROUP_FLAG_GROUP_MOVEMENT);

        me->GetMap()->GetCreatureGroupMgr().GetMovementMgr().RemoveGroupData(
            group->GetId());
        break;
    }
    case SMART_ACTION_GROUP_ADD_FLAG:
    {
        if (!me || !me->GetGroup() || !IsSmart(me))
            return;

        auto group = me->GetGroup();
        if (group->GetId() != static_cast<SmartAI*>(me->AI())->GetGroup())
            return;

        group->AddFlag(e.action.raw.param1);
        break;
    }
    case SMART_ACTION_GROUP_REMOVE_FLAG:
    {
        if (!me || !me->GetGroup() || !IsSmart(me))
            return;

        auto group = me->GetGroup();
        if (group->GetId() != static_cast<SmartAI*>(me->AI())->GetGroup())
            return;

        group->RemoveFlag(e.action.raw.param1);
        break;
    }
    case SMART_ACTION_PLAY_SPLINE:
    {
        auto targets = GetTargets(e, unit);
        if (targets.empty())
            return;
        for (auto& target : targets)
        {
            if (IsCreature(target))
            {
                static_cast<Creature*>(target)->movement_gens.push(
                    new movement::SplineMovementGenerator(
                        e.action.raw.param1, e.action.raw.param2),
                    e.action.raw.param3);
            }
        }
        break;
    }
    case SMART_ACTION_MOD_EVENT_TIMER:
    {
        for (auto& event : mEvents)
        {
            if (event.event_id == e.action.raw.param1)
            {
                event.timer = urand(
                    uint32(e.action.raw.param2), uint32(e.action.raw.param3));
                event.active = true;
                break;
            }
        }
        break;
    }
    case SMART_ACTION_SET_BEHAVIORAL_AI_CD:
    {
        if (!me || !IsSmart(me))
            return;

        auto cd =
            urand(uint32(e.action.raw.param3), uint32(e.action.raw.param4));

        static_cast<SmartAI*>(me->AI())->GetBehavioralAI().SetCooldown(
            e.action.raw.param1, e.action.raw.param2, cd);

        break;
    }
    case SMART_ACTION_RESET_LOOT_RECIPIENTS:
    {
        auto targets = GetTargets(e, unit);
        for (auto& target : targets)
        {
            if (IsCreature(target))
                static_cast<Creature*>(target)->ResetLootRecipients();
        }
        break;
    }
    case SMART_ACTION_FWD_INVOKER:
    {
        if (!unit)
            return;

        auto targets = GetTargets(e, unit);
        ObjectList send_targets;
        send_targets.push_back(unit);

        for (auto& target : targets)
        {
            if (IsCreature(target))
            {
                auto ai = dynamic_cast<SmartAI*>(
                    static_cast<Creature*>(target)->AI());
                if (ai && ai->GetScript())
                {
                    if (!e.action.raw.param2 ||
                        e.action.raw.param2 &
                            (1 << (ai->GetScript()->GetPhase() - 1)))
                    {
                        ai->GetScript()->StoreTargets(
                            send_targets, e.action.raw.param1);
                        if (e.action.raw.param3)
                            ai->Notify(e.action.raw.param3, unit);
                    }
                }
            }
        }
        break;
    }
    default:
        logging.error(
            "SmartScript::ProcessAction: Entry %d SourceType %u, Event %u, "
            "Unhandled Action type %u",
            e.entryOrGuid, e.GetScriptType(), e.event_id, e.GetActionType());
        break;
    }

    if (e.link && e.link != e.event_id)
    {
        if (SmartScriptHolder* linked = FindLinkedEvent(e.link))
        {
            if (linked->GetEventType() == SMART_EVENT_LINK)
            {
                if (linked->event.minMax.repeatMin != 0)
                    linked->timer = urand(linked->event.minMax.repeatMin,
                        linked->event.minMax
                            .repeatMax); // Manual timer processing
                else
                    ProcessEvent(*linked, unit, var0, var1, bvar, spell, gob);
            }
            else
                logging.error(
                    "SmartScript::ProcessAction: Entry %d SourceType %u, Event "
                    "%u, Link Event %u not found or invalid, skipped.",
                    e.entryOrGuid, e.GetScriptType(), e.event_id, e.link);
        }
    }
}

void SmartScript::AddEvent(SMART_EVENT e, uint32 event_flags,
    uint32 event_param1, uint32 event_param2, uint32 event_param3,
    uint32 event_param4, SMART_ACTION action, uint32 action_param1,
    uint32 action_param2, uint32 action_param3, uint32 action_param4,
    uint32 action_param5, uint32 action_param6, SMARTAI_TARGETS t,
    uint32 target_param1, uint32 target_param2, uint32 target_param3,
    uint32 phaseMask)
{
    mInstallEvents.push_back(
        CreateEvent(e, event_flags, event_param1, event_param2, event_param3,
            event_param4, action, action_param1, action_param2, action_param3,
            action_param4, action_param5, action_param6, t, target_param1,
            target_param2, target_param3, 0, 0, phaseMask));
}

SmartScriptHolder SmartScript::CreateEvent(SMART_EVENT e, uint32 event_flags,
    uint32 event_param1, uint32 event_param2, uint32 event_param3,
    uint32 event_param4, SMART_ACTION action, uint32 action_param1,
    uint32 action_param2, uint32 action_param3, uint32 action_param4,
    uint32 action_param5, uint32 action_param6, SMARTAI_TARGETS t,
    uint32 target_param1, uint32 target_param2, uint32 target_param3,
    uint32 target_param4, uint32 target_param5, uint32 phaseMask)
{
    SmartScriptHolder script;
    script.event.type = e;
    script.event.raw.param1 = event_param1;
    script.event.raw.param2 = event_param2;
    script.event.raw.param3 = event_param3;
    script.event.raw.param4 = event_param4;
    script.event.event_phase_mask = phaseMask;
    script.event.event_flags = event_flags;

    script.action.type = action;
    script.action.raw.param1 = action_param1;
    script.action.raw.param2 = action_param2;
    script.action.raw.param3 = action_param3;
    script.action.raw.param4 = action_param4;
    script.action.raw.param5 = action_param5;
    script.action.raw.param6 = action_param6;

    script.target.type = t;
    script.target.raw.param1 = target_param1;
    script.target.raw.param2 = target_param2;
    script.target.raw.param3 = target_param3;
    script.target.raw.param4 = target_param4;
    script.target.raw.param5 = target_param5;

    script.source_type = SMART_SCRIPT_TYPE_CREATURE;
    InitTimer(script);
    return script;
}

// Helper for SMART_TARGET_GRID_*
namespace
{
template <typename Check>
std::vector<WorldObject*> find_targets_matching_mask(
    uint32 mask, WorldObject* base, float dist, Check check)
{
    std::vector<WorldObject*> set;

    if (mask & 0x1)
    {
        auto tmp =
            maps::visitors::yield_set<WorldObject, Player>{}(base, dist, check);
        set.insert(set.end(), tmp.begin(), tmp.end());
    }

    if (mask & 0x2)
    {
        auto tmp = maps::visitors::yield_set<WorldObject, Creature>{}(
            base, dist, check);
        set.insert(set.end(), tmp.begin(), tmp.end());
    }

    if (mask & 0x4)
    {
        auto tmp =
            maps::visitors::yield_set<WorldObject, Pet>{}(base, dist, check);
        set.insert(set.end(), tmp.begin(), tmp.end());
    }

    if (mask & 0x8)
    {
        auto tmp = maps::visitors::yield_set<WorldObject, GameObject>{}(
            base, dist, check);
        set.insert(set.end(), tmp.begin(), tmp.end());
    }

    if (mask & 0x10)
    {
        auto tmp = maps::visitors::yield_set<WorldObject, DynamicObject>{}(
            base, dist, check);
        set.insert(set.end(), tmp.begin(), tmp.end());
    }

    if (mask & 0x20)
    {
        auto tmp =
            maps::visitors::yield_set<WorldObject, Corpse>{}(base, dist, check);
        set.insert(set.end(), tmp.begin(), tmp.end());
    }

    if (mask & 0x40)
    {
        auto tmp = maps::visitors::yield_set<WorldObject, TemporarySummon>{}(
            base, dist, check);
        set.insert(set.end(), tmp.begin(), tmp.end());
    }

    if (mask & 0x80)
    {
        auto tmp =
            maps::visitors::yield_set<WorldObject, Totem>{}(base, dist, check);
        set.insert(set.end(), tmp.begin(), tmp.end());
    }

    if (mask & 0x100)
    {
        auto tmp = maps::visitors::yield_set<WorldObject, SpecialVisCreature>{}(
            base, dist, check);
        set.insert(set.end(), tmp.begin(), tmp.end());
    }

    return set;
}
}

ObjectList SmartScript::GetTargets(
    SmartScriptHolder const& e, Unit* invoker /*= NULL*/)
{
    Unit* trigger = nullptr;
    if (invoker)
        trigger = invoker;
    else if (Unit* tempLastInvoker = GetLastInvoker())
        trigger = tempLastInvoker;

    // List of conditions target must fulfill
    const ConditionList* conds =
        sConditionMgr::Instance()->GetConditionsForSmartTarget(
            e.entryOrGuid, e.event_id, e.source_type);

    // Lamda to check if WorldObject matches conditions
    auto baseobj = GetBaseObject();
    if (area_trigger)
        baseobj = invoker;
    auto meets_conds = [&conds, baseobj](WorldObject* target)
    {
        if (!conds || !baseobj)
            return true;
        return sConditionMgr::Instance()->IsObjectMeetToConditions(
            target, baseobj, conds);
    };

    ObjectList v;
    switch (e.GetTargetType())
    {
    case SMART_TARGET_NONE:
        break;
    case SMART_TARGET_SELF:
        if (GetBaseObject() && meets_conds(GetBaseObject()))
            v.push_back(GetBaseObject());
        break;
    case SMART_TARGET_VICTIM:
        if (me && me->getVictim() && meets_conds(me->getVictim()))
            v.push_back(me->getVictim());
        break;
    case SMART_TARGET_HOSTILE_SECOND_AGGRO:
        if (me)
            if (Unit* u =
                    me->SelectAttackingTarget(ATTACKING_TARGET_TOPAGGRO, 1))
                if (meets_conds(u))
                    v.push_back(u);
        break;
    case SMART_TARGET_HOSTILE_LAST_AGGRO:
        if (me)
            if (Unit* u =
                    me->SelectAttackingTarget(ATTACKING_TARGET_BOTTOMAGGRO, 0))
                if (meets_conds(u))
                    v.push_back(u);
        break;
    case SMART_TARGET_HOSTILE_RANDOM:
        if (me)
            if (Unit* u = me->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0))
                if (meets_conds(u))
                    v.push_back(u);
        break;
    case SMART_TARGET_HOSTILE_RANDOM_NOT_TOP:
        if (me)
            if (Unit* u = me->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 1))
                if (meets_conds(u))
                    v.push_back(u);
        break;
    case SMART_TARGET_ACTION_INVOKER:
        if (trigger && meets_conds(trigger))
            v.push_back(trigger);
        break;
    case SMART_TARGET_POSITION:
        break;
    case SMART_TARGET_CREATURE_GUID:
    {
        auto source = GetBaseObject();
        if (area_trigger)
            source = invoker;
        if (!source)
            break;
        Creature* target = nullptr;
        ObjectGuid guid =
            ObjectGuid(HIGHGUID_UNIT, e.target.guid.entry, e.target.guid.guid);
        target = source->GetMap()->GetCreature(guid);
        if (target && meets_conds(target))
            v.push_back(target);
        break;
    }
    case SMART_TARGET_GAMEOBJECT_GUID:
    {
        auto source = GetBaseObject();
        if (area_trigger)
            source = invoker;
        if (!source)
            break;
        GameObject* target = nullptr;
        ObjectGuid guid = ObjectGuid(
            HIGHGUID_GAMEOBJECT, e.target.guid.entry, e.target.guid.guid);
        target = source->GetMap()->GetGameObject(guid);
        if (target && meets_conds(target))
            v.push_back(target);
        break;
    }
    case SMART_TARGET_STORED:
    {
        auto targets = GetStoredTargets(e.target.stored.id);
        v.reserve(targets.size());
        for (auto& target : targets)
            if (target && meets_conds(target))
                v.push_back(target);
        break;
    }
    case SMART_TARGET_INVOKER_PARTY:
        if (trigger)
        {
            if (trigger->GetTypeId() == TYPEID_PLAYER)
            {
                Player* player = (Player*)trigger;
                if (Group* group = player->GetGroup())
                {
                    for (auto member : group->members(true))
                        if (meets_conds(member))
                            v.push_back(member);
                }
                // We still add the player to the list if there is no group. If
                // we do this even if there is a group (thus the else-check), it
                // will add the same player to the list twice. We don't want
                // that to happen.
                else if (meets_conds(trigger))
                    v.push_back(trigger);
            }
        }
        break;
    case SMART_TARGET_OWNER:
        if (me)
            if (auto owner = me->GetOwner())
                if (meets_conds(owner))
                    v.push_back(owner);
        break;
    case SMART_TARGET_THREAT_LIST:
    {
        if (me)
        {
            std::list<HostileReference*> const& threatList =
                me->getThreatManager().getThreatList();
            for (const auto& elem : threatList)
                if (Unit* temp =
                        ObjectAccessor::GetUnit(*me, (elem)->getUnitGuid()))
                    if (meets_conds(temp))
                        v.push_back(temp);
        }
        break;
    }
    case SMART_TARGET_RELATIVE_POSITION:
        break;
    case SMART_TARGET_CHARMER:
        if (me)
            if (auto charmer = me->GetCharmer())
                if (meets_conds(charmer))
                    v.push_back(charmer);
        break;
    case SMART_TARGET_NONE_SELECTED_TARGET:
    {
        if (me)
        {
            v.reserve(mNoneSelectedTargets.size());
            for (auto& guid : mNoneSelectedTargets)
            {
                auto target = me->GetMap()->GetWorldObject(guid);
                if (target && meets_conds(target))
                    v.push_back(target);
            }
        }
        break;
    }
    case SMART_TARGET_CREATURE_GROUP:
    {
        if (me && me->GetGroup())
        {
            auto members = me->GetGroup()->GetMembers();
            v.reserve(members.size());
            for (auto member : members)
            {
                if (member->IsInWorld() &&
                    (!e.target.creatureGroup.only_alive || member->isAlive()))
                    if (meets_conds(member))
                        v.push_back(member);
            }
        }
        break;
    }
    case SMART_TARGET_GRID_STANDARD:
    case SMART_TARGET_GRID_ENTRY:
    {
        auto base = GetBaseObject();
        if (area_trigger)
            base = invoker;
        if (!base)
            break;

        uint32 tm = e.target.grid.type_mask;
        float min = e.target.grid.min_dist;
        float max = e.target.grid.max_dist;
        uint32 sm = e.target.grid.selection_mask;

        auto check = [&e, sm, min, base, &meets_conds](WorldObject* t)
        {
            if (t == base)
                return false;

            if (min > 0 && base->GetDistance(t) < min)
                return false;

            // Requires LoS
            if (sm & 0x80 && !base->IsWithinLOSInMap(t))
                return false;

            // Required entry
            if (e.GetTargetType() == SMART_TARGET_GRID_ENTRY &&
                e.target.raw.param5 != t->GetEntry())
                return false;

            if (t->GetTypeId() == TYPEID_GAMEOBJECT && (sm & 0x100) == 0 &&
                !static_cast<GameObject*>(t)->isSpawned())
                return false;

            // The rest is Unit only checks
            if (t->GetTypeId() != TYPEID_UNIT &&
                t->GetTypeId() != TYPEID_PLAYER)
                return true;

            if (t->GetTypeId() == TYPEID_PLAYER &&
                static_cast<Player*>(t)->isGameMaster())
                return false;

            auto u = static_cast<Unit*>(t);

            // Exclude alive
            if (sm & 0x1 && u->isAlive())
                return false;
            // Include dead
            if (!(sm & 0x2 || u->isAlive()))
                return false;
            // Exclude friendly
            if (sm & 0x20 && base->IsFriendlyTo(u))
                return false;
            // Exclude hostile
            if (sm & 0x40 && base->IsHostileTo(u))
                return false;

            if (!meets_conds(t))
                return false;

            return true;
        };

        v = find_targets_matching_mask(tm, base, max, check);

        // Use only one target, pick closest
        if (sm & 0x4 && !v.empty())
        {
            auto closest = v[0];
            float dist = base->GetDistance(closest);
            for (auto& obj : v)
            {
                float ndist = base->GetDistance(obj);
                if (ndist < dist)
                {
                    dist = ndist;
                    closest = obj;
                }
            }
            v.clear();
            v.push_back(closest);
        }
        // Use only one target, pick random
        else if (sm & 0x8 && !v.empty())
        {
            WorldObject* target = nullptr;
            if (!v.empty())
                target = v[urand(0, v.size() - 1)];
            v.clear();
            v.push_back(target);
        }
        // Use only one target, pick furthest away
        else if (sm & 0x10 && !v.empty())
        {
            auto furthest = v[0];
            float dist = base->GetDistance(furthest);
            for (auto& obj : v)
            {
                float ndist = base->GetDistance(obj);
                if (ndist > dist)
                {
                    dist = ndist;
                    furthest = obj;
                }
            }
            v.clear();
            v.push_back(furthest);
        }

        break;
    }
    default:
        break;
    }

    return v;
}

bool SmartScript::ProcessEvent(SmartScriptHolder& e, Unit* unit, uint32 var0,
    uint32 var1, bool bvar, const SpellEntry* spell, GameObject* gob)
{
    if (!e.active && e.GetEventType() != SMART_EVENT_LINK)
        return false;

    if ((e.event.event_phase_mask && !IsInPhase(e.event.event_phase_mask)) ||
        ((e.event.event_flags & SMART_EVENT_FLAG_NOT_REPEATABLE) && e.runOnce))
        return false;

    if (me && me->hasUnitState(UNIT_STAT_CONTROLLED) &&
        !(e.event.event_flags & SMART_EVENT_FLAG_CONTROLLED))
        return false;

    switch (e.GetEventType())
    {
    case SMART_EVENT_LINK: // special handling
        ProcessAction(e, unit, var0, var1, bvar, spell, gob);
        break;
    // called from Update tick
    case SMART_EVENT_UPDATE:
        RecalcTimer(
            e, e.event.minMaxRepeat.repeatMin, e.event.minMaxRepeat.repeatMax);
        ProcessAction(e);
        break;
    case SMART_EVENT_UPDATE_OOC:
        if (me && me->isInCombat())
            return false;
        RecalcTimer(
            e, e.event.minMaxRepeat.repeatMin, e.event.minMaxRepeat.repeatMax);
        ProcessAction(e);
        break;
    case SMART_EVENT_UPDATE_IC:
        if (!me || !me->isInCombat())
            return false;
        RecalcTimer(
            e, e.event.minMaxRepeat.repeatMin, e.event.minMaxRepeat.repeatMax);
        ProcessAction(e);
        break;
    case SMART_EVENT_HEALTH_PCT:
    {
        if (!me || !me->isInCombat() || !me->GetMaxHealth())
            return false;
        uint32 perc = (uint32)me->GetHealthPercent();
        if (perc > e.event.minMaxRepeat.max || perc < e.event.minMaxRepeat.min)
            return false;
        // Flee for assist waits until the current spell cast finishes
        if (e.GetActionType() == SMART_ACTION_FLEE_FOR_ASSIST &&
            me->IsNonMeleeSpellCasted(false))
            return false;
        RecalcTimer(
            e, e.event.minMaxRepeat.repeatMin, e.event.minMaxRepeat.repeatMax);
        ProcessAction(e);
        break;
    }
    case SMART_EVENT_HEALTH_FLAT:
    {
        if (!me || !me->isInCombat() || !me->GetHealth())
            return false;
        uint32 flat = me->GetHealth();
        if (flat > e.event.minMaxRepeat.max || flat < e.event.minMaxRepeat.min)
            return false;
        RecalcTimer(
            e, e.event.minMaxRepeat.repeatMin, e.event.minMaxRepeat.repeatMax);
        ProcessAction(e);
        break;
    }
    case SMART_EVENT_TARGET_HEALTH_PCT:
    {
        if (!me || !me->isInCombat() || !me->getVictim() ||
            !me->getVictim()->GetMaxHealth())
            return false;
        uint32 perc = (uint32)me->getVictim()->GetHealthPercent();
        if (perc > e.event.minMaxRepeat.max || perc < e.event.minMaxRepeat.min)
            return false;
        RecalcTimer(
            e, e.event.minMaxRepeat.repeatMin, e.event.minMaxRepeat.repeatMax);
        ProcessAction(e, me->getVictim());
        break;
    }
    case SMART_EVENT_MANA_PCT:
    {
        if (!me || !me->isInCombat() || !me->GetMaxPower(POWER_MANA))
            return false;
        uint32 perc = uint32(
            100.0f * me->GetPower(POWER_MANA) / me->GetMaxPower(POWER_MANA));
        if (perc > e.event.minMaxRepeat.max || perc < e.event.minMaxRepeat.min)
            return false;
        RecalcTimer(
            e, e.event.minMaxRepeat.repeatMin, e.event.minMaxRepeat.repeatMax);
        ProcessAction(e);
        break;
    }
    case SMART_EVENT_TARGET_MANA_PCT:
    {
        if (!me || !me->isInCombat() || !me->getVictim() ||
            !me->getVictim()->GetMaxPower(POWER_MANA))
            return false;
        uint32 perc = uint32(100.0f * me->getVictim()->GetPower(POWER_MANA) /
                             me->getVictim()->GetMaxPower(POWER_MANA));
        if (perc > e.event.minMaxRepeat.max || perc < e.event.minMaxRepeat.min)
            return false;
        RecalcTimer(
            e, e.event.minMaxRepeat.repeatMin, e.event.minMaxRepeat.repeatMax);
        ProcessAction(e, me->getVictim());
        break;
    }
    case SMART_EVENT_RANGE:
    {
        if (!me || !me->isInCombat() || !me->getVictim())
            return false;

        if (me->IsInRange(me->getVictim(), (float)e.event.minMaxRepeat.min,
                (float)e.event.minMaxRepeat.max))
        {
            ProcessAction(e, me->getVictim());
            RecalcTimer(e, e.event.minMaxRepeat.repeatMin,
                e.event.minMaxRepeat.repeatMax);
        }
        break;
    }
    case SMART_EVENT_TARGET_CASTING:
    {
        if (!me || !me->isInCombat() || !me->getVictim() ||
            !me->getVictim()->IsNonMeleeSpellCasted(false, false, true))
            return false;
        ProcessAction(e, me->getVictim());
        RecalcTimer(e, e.event.minMax.repeatMin, e.event.minMax.repeatMax);
    }
    case SMART_EVENT_FRIENDLY_HEALTH_IC:
    case SMART_EVENT_FRIENDLY_HEALTH:
    {
        if (!me || (e.GetEventType() == SMART_EVENT_FRIENDLY_HEALTH_IC &&
                       !me->isInCombat()))
            return false;

        maps::checks::hurt_friend check{
            me, false, (float)e.event.friendlyHealth.hpMaxPct / 100.0f};
        auto target = maps::visitors::yield_best_match<Unit, Creature,
            SpecialVisCreature, TemporarySummon, Player>{}(
            me, (float)e.event.friendlyHealth.radius, check);
        if (!target)
            return false;
        ProcessAction(e, target);
        RecalcTimer(e, e.event.friendlyHealth.repeatMin,
            e.event.friendlyHealth.repeatMax);
        break;
    }
    case SMART_EVENT_FRIENDLY_IS_CC:
    {
        if (!me || !me->isInCombat())
            return false;

        auto pList = DoFindFriendlyCC((float)e.event.friendlyCC.radius);
        if (pList.empty())
            return false;
        ProcessAction(e, *(pList.begin()));
        RecalcTimer(
            e, e.event.friendlyCC.repeatMin, e.event.friendlyCC.repeatMax);
        break;
    }
    case SMART_EVENT_FRIENDLY_MISSING_BUFF:
    {
        auto pList = DoFindFriendlyMissingBuff(
            (float)e.event.missingBuff.radius, e.event.missingBuff.spell);

        if (pList.empty())
            return false;
        ProcessAction(e, *(pList.begin()));
        RecalcTimer(
            e, e.event.missingBuff.repeatMin, e.event.missingBuff.repeatMax);
        break;
    }
    case SMART_EVENT_HAS_AURA:
    {
        if (!me)
            return false;
        uint32 count = me->GetAuraCount(e.event.aura.spell);
        if ((!e.event.aura.count && !count) ||
            (e.event.aura.count && count >= e.event.aura.count))
        {
            ProcessAction(e);
            RecalcTimer(e, e.event.aura.repeatMin, e.event.aura.repeatMax);
        }
        break;
    }
    case SMART_EVENT_TARGET_BUFFED:
    {
        if (!me || !me->getVictim())
            return false;
        uint32 count = me->getVictim()->GetAuraCount(e.event.aura.spell);
        if (count < e.event.aura.count)
            return false;
        ProcessAction(e);
        RecalcTimer(e, e.event.aura.repeatMin, e.event.aura.repeatMax);
        break;
    }
    case SMART_EVENT_RESET:
    {
        if (e.event.raw.param1 == 0 || e.event.raw.param1 & var0)
            ProcessAction(e);
        break;
    }
    // no params
    case SMART_EVENT_AGGRO:
    case SMART_EVENT_DEATH:
    case SMART_EVENT_EVADE:
    case SMART_EVENT_REACHED_HOME:
    case SMART_EVENT_CHARMED:
    case SMART_EVENT_CHARM_EXPIRED:
    case SMART_EVENT_CORPSE_REMOVED:
    case SMART_EVENT_AI_INIT:
    case SMART_EVENT_TRANSPORT_ADDPLAYER:
    case SMART_EVENT_TRANSPORT_REMOVE_PLAYER:
    case SMART_EVENT_QUEST_ACCEPTED:
    case SMART_EVENT_QUEST_OBJ_COPLETETION:
    case SMART_EVENT_QUEST_COMPLETION:
    case SMART_EVENT_QUEST_REWARDED:
    case SMART_EVENT_QUEST_FAIL:
    case SMART_EVENT_JUST_SUMMONED:
    case SMART_EVENT_JUST_CREATED:
    case SMART_EVENT_GOSSIP_HELLO:
    case SMART_EVENT_ON_SPELLCLICK:
    case SMART_EVENT_SPAWN:
    case SMART_EVENT_BEFORE_DEATH:
    case SMART_EVENT_DISENGAGE_CALLBACK:
    case SMART_EVENT_LEASH:
        ProcessAction(e, unit, var0, var1, bvar, spell, gob);
        break;
    case SMART_EVENT_IS_BEHIND_TARGET:
    {
        if (!me)
            return false;

        if (Unit* victim = me->getVictim())
        {
            if (!victim->HasInArc(static_cast<float>(M_PI), me))
            {
                ProcessAction(e, victim);
                RecalcTimer(e, e.event.behindTarget.cooldownMin,
                    e.event.behindTarget.cooldownMax);
            }
        }
        break;
    }
    case SMART_EVENT_RECEIVE_EMOTE:
        if (e.event.emote.emote == var0)
        {
            ProcessAction(e, unit);
            RecalcTimer(
                e, e.event.emote.cooldownMin, e.event.emote.cooldownMax);
        }
        break;
    case SMART_EVENT_KILL:
    {
        if (!me || !unit)
            return false;
        if (e.event.kill.playerOnly && unit->GetTypeId() != TYPEID_PLAYER)
            return false;
        if (e.event.kill.creature && unit->GetEntry() != e.event.kill.creature)
            return false;
        ProcessAction(e, unit);
        RecalcTimer(e, e.event.kill.cooldownMin, e.event.kill.cooldownMax);
        break;
    }
    case SMART_EVENT_SPELLHIT_TARGET:
    case SMART_EVENT_SPELLHIT:
    {
        if (!spell)
            return false;
        if ((!e.event.spellHit.spell || spell->Id == e.event.spellHit.spell) &&
            (!e.event.spellHit.school ||
                (spell->SchoolMask & e.event.spellHit.school)))
        {
            ProcessAction(e, unit, 0, 0, bvar, spell);
            RecalcTimer(
                e, e.event.spellHit.cooldownMin, e.event.spellHit.cooldownMax);
        }
        break;
    }
    case SMART_EVENT_OOC_LOS:
    {
        if (!me || me->isInCombat())
            return false;
        // can trigger if closer than fMaxAllowedRange
        float range = (float)e.event.los.maxDist;

        // if range is ok and we are actually in LOS
        if (me->IsWithinDistInMap(unit, range) && me->IsWithinWmoLOSInMap(unit))
        {
            // if friendly event&&who is not hostile OR hostile event&&who
            // is
            // hostile
            if ((e.event.los.noHostile && !me->IsHostileTo(unit)) ||
                (!e.event.los.noHostile && me->IsHostileTo(unit)))
            {
                ProcessAction(e, unit);
                RecalcTimer(
                    e, e.event.los.cooldownMin, e.event.los.cooldownMax);
            }
        }
        break;
    }
    case SMART_EVENT_IC_LOS:
    {
        if (!me || !me->isInCombat())
            return false;
        // can trigger if closer than fMaxAllowedRange
        float range = (float)e.event.los.maxDist;

        // if range is ok and we are actually in LOS
        if (me->IsWithinDistInMap(unit, range) && me->IsWithinWmoLOSInMap(unit))
        {
            // if friendly event&&who is not hostile OR hostile event&&who
            // is
            // hostile
            if ((e.event.los.noHostile && !me->IsHostileTo(unit)) ||
                (!e.event.los.noHostile && me->IsHostileTo(unit)))
            {
                ProcessAction(e, unit);
                RecalcTimer(
                    e, e.event.los.cooldownMin, e.event.los.cooldownMax);
            }
        }
        break;
    }
    case SMART_EVENT_RESPAWN:
    {
        if (!GetBaseObject())
            return false;
        if (e.event.respawn.type == SMART_SCRIPT_RESPAWN_CONDITION_MAP &&
            GetBaseObject()->GetMapId() != e.event.respawn.map)
            return false;
        if (e.event.respawn.type == SMART_SCRIPT_RESPAWN_CONDITION_AREA &&
            GetBaseObject()->GetZoneId() != e.event.respawn.area)
            return false;
        ProcessAction(e);
        break;
    }
    case SMART_EVENT_SUMMONED_UNIT:
    {
        if (!IsCreature(unit))
            return false;
        if (e.event.summoned.creature &&
            unit->GetEntry() != e.event.summoned.creature)
            return false;
        ProcessAction(e, unit);
        RecalcTimer(
            e, e.event.summoned.cooldownMin, e.event.summoned.cooldownMax);
        break;
    }
    case SMART_EVENT_RECEIVE_HEAL:
    case SMART_EVENT_DAMAGED:
    case SMART_EVENT_DAMAGED_TARGET:
    {
        if ((e.event.minMaxRepeat.max != 0 || e.event.minMaxRepeat.min != 0) &&
            (var0 > e.event.minMaxRepeat.max ||
                var0 < e.event.minMaxRepeat.min))
            return false;
        ProcessAction(e, unit);
        RecalcTimer(
            e, e.event.minMaxRepeat.repeatMin, e.event.minMaxRepeat.repeatMax);
        break;
    }
    case SMART_EVENT_MOVEMENTINFORM:
    {
        if ((e.event.movementInform.type &&
                var0 != e.event.movementInform.type) ||
            (e.event.movementInform.id && var1 != e.event.movementInform.id))
            return false;
        ProcessAction(e, unit, var0, var1);
        break;
    }
    case SMART_EVENT_TRANSPORT_RELOCATE:
    case SMART_EVENT_WAYPOINT_START:
    {
        if (e.event.waypoint.pathID && var0 != e.event.waypoint.pathID)
            return false;
        ProcessAction(e, unit, var0);
        break;
    }
    case SMART_EVENT_WAYPOINT_REACHED:
    case SMART_EVENT_WAYPOINT_RESUMED:
    case SMART_EVENT_WAYPOINT_PAUSED:
    case SMART_EVENT_WAYPOINT_STOPPED:
    {
        if (!me ||
            (e.event.waypoint.pointID && var0 != e.event.waypoint.pointID) ||
            (e.event.waypoint.pathID && GetPathId() != e.event.waypoint.pathID))
            return false;
        ProcessAction(e, unit);
        break;
    }
    case SMART_EVENT_WAYPOINT_ENDED:
    {
        if (!me || (e.event.waypoint_ended.pointID &&
                       var0 != e.event.waypoint_ended.pointID) ||
            (e.event.waypoint_ended.pathID &&
                var1 != e.event.waypoint_ended.pathID))
            return false;
        uint32 r = e.event.waypoint_ended.result;
        if (r && !((r == 1 && !bvar) || (r == 2 && bvar)))
            return false;
        ProcessAction(e, unit);
        break;
    }
    case SMART_EVENT_SUMMON_DESPAWNED:
    case SMART_EVENT_INSTANCE_PLAYER_ENTER:
    {
        if (e.event.instancePlayerEnter.team &&
            var0 != e.event.instancePlayerEnter.team)
            return false;
        ProcessAction(e, unit, var0);
        RecalcTimer(e, e.event.instancePlayerEnter.cooldownMin,
            e.event.instancePlayerEnter.cooldownMax);
        break;
    }
    case SMART_EVENT_ACCEPTED_QUEST:
    case SMART_EVENT_REWARD_QUEST:
    {
        if (e.event.quest.quest && var0 != e.event.quest.quest)
            return false;
        ProcessAction(e, unit, var0);
        break;
    }
    case SMART_EVENT_TRANSPORT_ADDCREATURE:
    {
        if (e.event.transportAddCreature.creature &&
            var0 != e.event.transportAddCreature.creature)
            return false;
        ProcessAction(e, unit, var0);
        break;
    }
    case SMART_EVENT_AREATRIGGER_ONTRIGGER:
    {
        if (e.event.areatrigger.id && var0 != e.event.areatrigger.id)
            return false;
        ProcessAction(e, unit, var0);
        break;
    }
    case SMART_EVENT_TEXT_OVER:
    {
        if (var0 != e.event.textOver.textGroupID ||
            (e.event.textOver.creatureEntry &&
                e.event.textOver.creatureEntry != var1))
            return false;
        ProcessAction(e, unit, var0);
        break;
    }
    case SMART_EVENT_DATA_SET:
    {
        if (e.event.dataSet.id != var0 || e.event.dataSet.value != var1)
            return false;
        ProcessAction(e, unit, var0, var1);
        RecalcTimer(
            e, e.event.dataSet.cooldownMin, e.event.dataSet.cooldownMax);
        break;
    }
    case SMART_EVENT_PASSENGER_REMOVED:
    case SMART_EVENT_PASSENGER_BOARDED:
    {
        if (!unit)
            return false;
        ProcessAction(e, unit);
        RecalcTimer(e, e.event.minMax.repeatMin, e.event.minMax.repeatMax);
        break;
    }
    case SMART_EVENT_TIMED_EVENT_TRIGGERED:
    {
        if (e.event.timedEvent.id == var0)
            ProcessAction(e, unit);
        break;
    }
    case SMART_EVENT_GOSSIP_SELECT:
    {
        if (e.event.gossip.senderOrMenuId != var0 ||
            e.event.gossip.action != var1)
            return false;
        ProcessAction(e, unit, var0, var1);
        break;
    }
    case SMART_EVENT_DUMMY_EFFECT:
    {
        if (e.event.dummy.spell != var0 || e.event.dummy.effIndex != var1)
            return false;
        ProcessAction(e, unit, var0, var1);
        break;
    }
    case SMART_EVENT_GAME_EVENT_START:
    case SMART_EVENT_GAME_EVENT_END:
    {
        if (e.event.gameEvent.gameEventId != var0)
            return false;
        ProcessAction(e, nullptr, var0);
        break;
    }
    case SMART_EVENT_GO_STATE_CHANGED:
    {
        if (e.event.goStateChanged.state != var0)
            return false;
        ProcessAction(e, unit, var0, var1);
        break;
    }
    case SMART_EVENT_GO_EVENT_INFORM:
    {
        if (e.event.eventInform.eventId != var0)
            return false;
        ProcessAction(e, nullptr, var0);
        break;
    }
    case SMART_EVENT_ACTION_DONE:
    {
        if (e.event.doAction.eventId != var0)
            return false;
        ProcessAction(e, unit, var0);
        break;
    }
    case SMART_EVENT_AI_NOTIFICATION:
    {
        if (me && me->isDead())
            return false;
        if (e.event.aiNotification.id && var0 != e.event.aiNotification.id)
            return false;
        ProcessAction(e, unit, var0);
        break;
    }
    case SMART_EVENT_HAS_AURA_WITH_MECHANIC:
    {
        if (!me)
            return false;
        uint32 mask = e.event.auraMechanic.mechanicMask;
        bool found = false;
        me->loop_auras([&found, mask](AuraHolder* holder)
            {
                if (holder->HasMechanicMask(mask))
                    found = true;
                return !found; // break when found is true
            });
        if (!found)
            return false;
        ProcessAction(e, unit);
        RecalcTimer(e, e.event.auraMechanic.cooldownMin,
            e.event.auraMechanic.cooldownMax);
        break;
    }
    default:
        logging.error("SmartScript::ProcessEvent: Unhandled Event type %u",
            e.GetEventType());
        break;
    }

    return true;
}

void SmartScript::InitTimer(SmartScriptHolder& e)
{
    switch (e.GetEventType())
    {
    // set only events which have initial timers
    case SMART_EVENT_UPDATE:
    case SMART_EVENT_UPDATE_IC:
    case SMART_EVENT_UPDATE_OOC:
    case SMART_EVENT_OOC_LOS:
    case SMART_EVENT_IC_LOS:
        RecalcTimer(e, e.event.minMaxRepeat.min, e.event.minMaxRepeat.max);
        break;
    default:
        e.active = true;
        break;
    }
}
void SmartScript::RecalcTimer(SmartScriptHolder& e, uint32 min, uint32 max)
{
    // min/max was checked at loading!
    e.timer = urand(uint32(min), uint32(max));
    e.active = e.timer ? false : true;
}

void SmartScript::UpdateTimer(SmartScriptHolder& e, uint32 const diff)
{
    if (e.GetEventType() == SMART_EVENT_LINK)
    {
        // Process delayed linking manually
        if (e.timer)
        {
            if (e.timer <= diff)
            {
                ProcessEvent(e);
                e.timer = 0;
            }
            else
                e.timer -= diff;
        }

        return;
    }

    if (e.event.event_phase_mask && !IsInPhase(e.event.event_phase_mask))
        return;

    if (e.GetEventType() == SMART_EVENT_UPDATE_IC && (!me || !me->isInCombat()))
        return;

    if (e.GetEventType() == SMART_EVENT_UPDATE_OOC &&
        (me && me->isInCombat())) // can be used with me=NULL (go script)
        return;

    if (e.timer < diff)
    {
        // delay spell cast event if another spell is being casted, or if
        // we're
        // unable to cast the spell
        if (e.GetActionType() == SMART_ACTION_CAST && me)
        {
            bool retry = false;

            // check if a spell is currently casted
            if (!(e.action.cast.flags &
                    (SMARTCAST_INTERRUPT_PREVIOUS | SMARTCAST_TRIGGERED)) &&
                me->IsNonMeleeSpellCasted(false))
                retry = true;

            // checks that only apply to spells that aren't triggered
            if (!(e.action.cast.flags & SMARTCAST_TRIGGERED))
            {
                const SpellEntry* info =
                    sSpellStore.LookupEntry(e.action.cast.spell); // can be NULL

                // crowd control
                if (me->IsAffectedByThreatIgnoringCC())
                    retry = true;

                if (info)
                {
                    // silence checks
                    if (info->PreventionType == SPELL_PREVENTION_TYPE_SILENCE)
                    {
                        // blanket silence
                        if (me->HasAuraType(SPELL_AURA_MOD_SILENCE))
                            retry = true;

                        // school silence
                        if (me->IsSpellSchoolLocked(
                                (SpellSchoolMask)info->SchoolMask))
                            retry = true;
                    }
                }
            }

            if (retry)
            {
                e.timer = 1;
                return;
            }
        }

        e.active = true;          // activate events with cooldown
        switch (e.GetEventType()) // process ONLY timed events
        {
        case SMART_EVENT_UPDATE:
        case SMART_EVENT_UPDATE_OOC:
        case SMART_EVENT_UPDATE_IC:
        case SMART_EVENT_HEALTH_PCT:
        case SMART_EVENT_HEALTH_FLAT:
        case SMART_EVENT_TARGET_HEALTH_PCT:
        case SMART_EVENT_MANA_PCT:
        case SMART_EVENT_TARGET_MANA_PCT:
        case SMART_EVENT_RANGE:
        case SMART_EVENT_TARGET_CASTING:
        case SMART_EVENT_FRIENDLY_HEALTH_IC:
        case SMART_EVENT_FRIENDLY_HEALTH:
        case SMART_EVENT_FRIENDLY_IS_CC:
        case SMART_EVENT_FRIENDLY_MISSING_BUFF:
        case SMART_EVENT_HAS_AURA:
        case SMART_EVENT_TARGET_BUFFED:
        case SMART_EVENT_IS_BEHIND_TARGET:
        case SMART_EVENT_HAS_AURA_WITH_MECHANIC:
        {
            if (e.GetScriptType() == SMART_SCRIPT_TYPE_TIMED_ACTIONLIST)
            {
                ProcessEvent(e);

                e.enableTimed = false; // disable event if it is in an
                                       // ActionList and was processed once
                for (auto& elem : mTimedActionList)
                {
                    // find the first event which is not the current one and
                    // enable it
                    if (elem.event_id > e.event_id)
                    {
                        elem.enableTimed = true;
                        break;
                    }
                }
            }
            else
            {
                const ConditionList* conds =
                    sConditionMgr::Instance()->GetConditionsForSmartEvent(
                        e.entryOrGuid, e.event_id, e.source_type);
                ConditionSourceInfo info =
                    ConditionSourceInfo(GetBaseObject(), GetBaseObject());
                if (sConditionMgr::Instance()->IsObjectMeetToConditions(
                        info, conds))
                    ProcessEvent(e);
            }
            break;
        }
        }
    }
    else
        e.timer -= diff;
}

bool SmartScript::CheckTimer(SmartScriptHolder const& e) const
{
    return e.active;
}

void SmartScript::_SetEventPhase(uint32 phase)
{
    mEventPhase = phase;
    if (me && me->AI())
        if (auto smartai = dynamic_cast<SmartAI*>(me->AI()))
            smartai->UpdateBehavioralAIPhase((int)phase);
}

void SmartScript::InstallEvents()
{
    if (!mInstallEvents.empty())
    {
        for (auto& elem : mInstallEvents)
            mEvents.push_back(elem); // must be before UpdateTimers

        mInstallEvents.clear();
    }
}

void SmartScript::OnUpdate(uint32 const diff)
{
    if ((mScriptType == SMART_SCRIPT_TYPE_CREATURE ||
            mScriptType == SMART_SCRIPT_TYPE_GAMEOBJECT) &&
        !GetBaseObject())
        return;

    InstallEvents(); // before UpdateTimers

    mNewPhase = -1;

    for (auto& elem : mEvents)
        UpdateTimer(elem, diff);

    if (!mStoredEvents.empty())
        for (auto& elem : mStoredEvents)
            UpdateTimer(elem, diff);

    bool needCleanup = true;
    if (!mTimedActionList.empty())
    {
        for (auto& elem : mTimedActionList)
        {
            if ((elem).enableTimed)
            {
                UpdateTimer(elem, diff);
                needCleanup = false;
            }
        }
    }
    if (needCleanup)
        mTimedActionList.clear();

    if (mNewPhase != -1)
        _SetEventPhase(mNewPhase);

    if (!mRemIDs.empty())
    {
        for (auto& elem : mRemIDs)
        {
            RemoveStoredEvent((elem));
        }
    }
    if (mUseTextTimer && me)
    {
        if (mTextTimer < diff)
        {
            uint32 textID = mLastTextID;
            mLastTextID = 0;
            uint32 entry = mTalkerEntry;
            mTalkerEntry = 0;
            mTextTimer = 0;
            mUseTextTimer = false;
            ProcessEventsFor(SMART_EVENT_TEXT_OVER, nullptr, textID, entry);
        }
        else
            mTextTimer -= diff;
    }

    if (mKillSayCooldown)
    {
        if (mKillSayCooldown > diff)
            mKillSayCooldown -= diff;
        else
            mKillSayCooldown = 0;
    }
}

void SmartScript::FillScript(
    SmartAIEventList e, WorldObject* obj, AreaTriggerEntry const* at)
{
    if (e.empty())
    {
        if (at)
            logging.error(
                "SmartScript: EventMap for AreaTrigger %u is empty but is "
                "using SmartScript.",
                at->id);
        return;
    }
    for (auto& elem : e)
    {
#ifndef TRINITY_DEBUG
        if ((elem).event.event_flags & SMART_EVENT_FLAG_DEBUG_ONLY)
            continue;
#endif

        if ((elem).event.event_flags &
            SMART_EVENT_FLAG_DIFFICULTY_ALL) // if has instance flag add
                                             // only if
                                             // in it
        {
            if (obj && obj->GetMap()->IsDungeon())
            {
                if ((1 << (obj->GetMap()->GetSpawnMode() + 1)) &
                    (elem).event.event_flags)
                {
                    mEvents.push_back((elem));
                }
            }
            continue;
        }
        if (me && IsSmart(me) && (elem.event.type == SMART_EVENT_OOC_LOS ||
                                     elem.event.type == SMART_EVENT_IC_LOS))
            static_cast<SmartAI*>(me->AI())->SetPassive(false);
        mEvents.push_back(
            (elem)); // NOTE: 'world(0)' events still get processed
                     // in ANY instance mode
    }
    if (mEvents.empty() && obj)
        logging.error(
            "SmartScript: Entry %u has events but no events added to list "
            "because of instance flags.",
            obj->GetEntry());
    if (mEvents.empty() && at)
        logging.error(
            "SmartScript: AreaTrigger %u has events but no events added to "
            "list because of instance flags. NOTE: triggers can not handle "
            "any "
            "instance flags.",
            at->id);
}

void SmartScript::GetScript()
{
    SmartAIEventList e;
    if (me)
    {
        // Use BOTH entry and guid based scripts
        e = sSmartScriptMgr::Instance()->GetScript(
            -((int32)me->GetGUIDLow()), mScriptType);
        if (!e.empty())
            FillScript(e, me, nullptr);
        e = sSmartScriptMgr::Instance()->GetScript(
            (int32)me->GetEntry(), mScriptType);
        if (!e.empty())
            FillScript(e, me, nullptr);
    }
    else if (go)
    {
        // Use BOTH entry and guid based scripts
        e = sSmartScriptMgr::Instance()->GetScript(
            -((int32)go->GetGUIDLow()), mScriptType);
        if (!e.empty())
            FillScript(e, go, nullptr);
        e = sSmartScriptMgr::Instance()->GetScript(
            (int32)go->GetEntry(), mScriptType);
        if (!e.empty())
            FillScript(e, go, nullptr);
    }
    else if (area_trigger)
    {
        e = sSmartScriptMgr::Instance()->GetScript(
            (int32)area_trigger->id, mScriptType);
        FillScript(e, nullptr, area_trigger);
    }
}

void SmartScript::OnInitialize(WorldObject* obj, AreaTriggerEntry const* at)
{
    if (obj)
    {
        switch (obj->GetTypeId())
        {
        case TYPEID_UNIT:
            mScriptType = SMART_SCRIPT_TYPE_CREATURE;
            me = (Creature*)obj;
            LOG_DEBUG(logging,
                "SmartScript::OnInitialize: source is Creature %u",
                me->GetEntry());
            break;
        case TYPEID_GAMEOBJECT:
            mScriptType = SMART_SCRIPT_TYPE_GAMEOBJECT;
            go = (GameObject*)obj;
            LOG_DEBUG(logging,
                "SmartScript::OnInitialize: source is GameObject %u",
                go->GetEntry());
            break;
        default:
            logging.error(
                "SmartScript::OnInitialize: Unhandled TypeID !WARNING!");
            assert(false);
            return;
        }
    }
    else if (at)
    {
        mScriptType = SMART_SCRIPT_TYPE_AREATRIGGER;
        area_trigger = at;
        LOG_DEBUG(logging,
            "SmartScript::OnInitialize: source is AreaTrigger %u",
            area_trigger->id);
    }
    else
    {
        logging.error(
            "SmartScript::OnInitialize: !WARNING! Initialized objects are "
            "NULL.");
        assert(false);
        return;
    }

    GetScript(); // load copy of script

    for (auto& elem : mEvents)
        InitTimer((elem)); // calculate timers for first time use

    ProcessEventsFor(SMART_EVENT_AI_INIT);
    InstallEvents();
    ProcessEventsFor(SMART_EVENT_JUST_CREATED);
}

void SmartScript::OnMoveInLineOfSight(Unit* who)
{
    ProcessEventsFor(SMART_EVENT_OOC_LOS, who);

    if (!me)
        return;

    if (!me->getVictim())
        return;

    ProcessEventsFor(SMART_EVENT_IC_LOS, who);
}

/*
void SmartScript::UpdateAIWhileCharmed(const uint32 diff)
{
}

void SmartScript::DoAction(const int32 param)
{
}

uint32 SmartScript::GetData(uint32 id)
{
    return 0;
}

void SmartScript::SetData(uint32 id, uint32 value)
{
}

void SmartScript::SetGUID(uint64 guid, int32 id)
{
}

uint64 SmartScript::GetGUID(int32 id)
{
    return 0;
}

void SmartScript::MovepointStart(uint32 id)
{
}

void SmartScript::SetRun(bool run)
{
}

void SmartScript::SetMovePathEndAction(SMART_ACTION action)
{
}

uint32 SmartScript::DoChat(int8 id, uint64 whisperGuid)
{
    return 0;
}*/
// SmartScript end

std::vector<Creature*> SmartScript::DoFindFriendlyCC(float range)
{
    if (!me)
        return std::vector<Creature*>{};

    return maps::visitors::yield_set<Creature>{}(
        me, range, maps::checks::friendly_crowd_controlled{me});
}

std::vector<Creature*> SmartScript::DoFindFriendlyMissingBuff(
    float range, uint32 spellid)
{
    if (!me)
        return std::vector<Creature*>{};

    maps::checks::missing_buff check{me, spellid};
    return maps::visitors::yield_set<Creature>{}(me, range, check);
}

void SmartScript::StoreTargets(const ObjectList& targets, uint32 id)
{
    std::vector<ObjectGuid> guids;
    guids.reserve(targets.size());

    for (WorldObject* obj : targets)
        guids.push_back(obj->GetObjectGuid());

    mStoredTargets[id] = guids;
}

ObjectList SmartScript::GetStoredTargets(uint32 id)
{
    if (!GetBaseObject())
        return ObjectList();

    auto itr = mStoredTargets.find(id);
    if (itr == mStoredTargets.end())
        return ObjectList();

    ObjectList list;
    list.reserve(itr->second.size());

    for (auto guid : itr->second)
    {
        if (WorldObject* obj = GetBaseObject()->GetMap()->GetWorldObject(guid))
            list.push_back(obj);
    }

    return list;
}

bool SmartScript::IsSmart(Creature* c)
{
    if (!c)
        c = me;

    bool smart = true;
    if (!c || dynamic_cast<SmartAI*>(c->AI()) == nullptr)
        smart = false;

    if (!smart)
        logging.error(
            "SmartScript: Action target Creature(entry: %u) is not using "
            "SmartAI, action skipped to prevent crash.",
            c ? c->GetEntry() : (me ? me->GetEntry() : 0));

    return smart;
}

bool SmartScript::IsSmartGO(GameObject* g)
{
    if (!g)
        g = go;

    bool smart = true;
    if (!g || dynamic_cast<SmartGameObjectAI*>(g->AI()) == nullptr)
        smart = false;

    if (!smart)
        logging.error(
            "SmartScript: Action target GameObject(entry: %u) is not using "
            "SmartGameObjectAI, action skipped to prevent crash.",
            g ? g->GetEntry() : (go ? go->GetEntry() : 0));

    return smart;
}

void SmartScript::SetScript9(SmartScriptHolder& e, uint32 entry)
{
    mTimedActionList.clear();
    if (!entry)
        return;
    mTimedActionList = sSmartScriptMgr::Instance()->GetScript(
        entry, SMART_SCRIPT_TYPE_TIMED_ACTIONLIST);
    if (mTimedActionList.empty())
        return;
    for (auto i = mTimedActionList.begin(); i != mTimedActionList.end(); ++i)
    {
        if (i == mTimedActionList.begin())
        {
            i->enableTimed = true; // enable processing only for the first
                                   // action
        }
        else
            i->enableTimed = false;

        if (e.action.timedActionList.timerType == 1)
            i->event.type = SMART_EVENT_UPDATE_IC;
        else if (e.action.timedActionList.timerType > 1)
            i->event.type = SMART_EVENT_UPDATE;
        InitTimer((*i));
    }
}

Unit* SmartScript::GetLastInvoker()
{
    if (me)
        return me->GetMap()->GetUnit(mLastInvoker);
    else if (go)
        return go->GetMap()->GetUnit(mLastInvoker);

    return nullptr;
}

bool SmartScript::GetTargetPosition(
    SmartScriptHolder& e, float& x, float& y, float& z, float& o)
{
    switch (e.GetTargetType())
    {
    case SMART_TARGET_SELF:
        if (auto obj = GetBaseObject())
        {
            x = obj->GetX() + e.target.x;
            y = obj->GetY() + e.target.y;
            z = obj->GetZ() + e.target.z;
            o = obj->GetO() + e.target.o;
            return true;
        }
        break;
    case SMART_TARGET_POSITION:
        x = e.target.x;
        y = e.target.y;
        z = e.target.z;
        o = e.target.o;
        return true;
    case SMART_TARGET_RELATIVE_POSITION:
    {
        if (WorldObject* obj = GetBaseObject())
        {
            float angle = urand(e.target.relativePos.minAngle,
                              e.target.relativePos.maxAngle) *
                          (M_PI_F / 180.0f); // User Input in degrees
            if (e.target.x != 0 || e.target.y != 0 || e.target.z != 0)
            {
                auto pos = obj->GetPointXYZ(
                    G3D::Vector3(e.target.x, e.target.y, e.target.z), angle,
                    frand(e.target.relativePos.minDist,
                        e.target.relativePos.maxDist));
                x = pos.x;
                y = pos.y;
                z = pos.z;
            }
            else
            {
                auto pos =
                    obj->GetPoint(angle, frand(e.target.relativePos.minDist,
                                             e.target.relativePos.maxDist));
                x = pos.x;
                y = pos.y;
                z = pos.z;
            }

            if (e.target.relativePos.setOrientationToMe)
            {
                float ang = atan2(obj->GetY() - y, obj->GetX() - x);
                o = (ang >= 0) ? ang : 2 * M_PI_F + ang;
            }

            else
                o = frand(0, 2 * M_PI_F);
            return true;
        }
        return false;
    }
    case SMART_TARGET_SAVED_POS:
    {
        if (me)
        {
            if (auto ai = dynamic_cast<SmartAI*>(me->AI()))
                ai->get_saved_pos(x, y, z, o);
            return true;
        }
        break;
    }
    default:
        break;
    }
    return false;
}
