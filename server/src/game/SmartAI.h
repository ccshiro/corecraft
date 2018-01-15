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

#ifndef TRINITY_SMARTAI_H
#define TRINITY_SMARTAI_H

#include "BehavioralAI.h"
#include "Common.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "GameObjectAI.h"
#include "movement/generators.h"
#include "SmartScript.h"
#include "SmartScriptMgr.h"
#include "Spell.h"
#include "Unit.h"

enum SmartEscortState
{
    SMART_ESCORT_NONE = 0x000,      // nothing in progress
    SMART_ESCORT_ESCORTING = 0x001, // escort is in progress
    SMART_ESCORT_RETURNING = 0x002, // escort is returning after being in combat
    SMART_ESCORT_RETURNED = 0x004, // escort is back at OOC pos, and waiting for
                                   // rest of group to arrive
    SMART_ESCORT_PAUSED =
        0x008 // will not proceed with waypoints before state is removed
};

enum SmartEscortVars
{
    SMART_ESCORT_MAX_PLAYER_DIST = 50,
    SMART_MAX_AID_DIST = SMART_ESCORT_MAX_PLAYER_DIST / 2,
    SMART_ESCORT_FAILED_DESPAWN_TIME = 4000
};

class SmartAI : public CreatureAI
{
public:
    ~SmartAI(){};
    explicit SmartAI(Creature* c);

    // Start moving to the desired MovePoint
    void StartPath(bool run = false, uint32 path = 0, bool repeat = false,
        Unit* invoker = nullptr);
    void SetEscortDespawnTime(int despawn) { mEscortDespawn = despawn; }
    bool LoadPath(uint32 entry);
    void PausePath(uint32 delay, bool forced = false);
    void StopPath(uint32 DespawnTime = 0, uint32 quest = 0, bool fail = false);
    void EndPath(bool fail = false, bool force = false);
    void ResumePath();
    void UpdatePathRunMode(bool run) { mEscortRun = run; }
    WayPoint* GetNextWayPoint();
    bool HasEscortState(uint32 uiEscortState)
    {
        return (mEscortState & uiEscortState);
    }
    void AddEscortState(uint32 uiEscortState) { mEscortState |= uiEscortState; }
    void RemoveEscortState(uint32 uiEscortState)
    {
        mEscortState &= ~uiEscortState;
    }
    void SetAutoAttack(bool on) { mCanAutoAttack = on; }
    void SetCombatMove(bool on);
    bool CanCombatMove() { return mCanCombatMove; }
    void SetFollow(Unit* target, float dist = 0.0f, float angle = 0.0f);
    void SetSparring() { mSparring = true; }
    bool IsSparring() const { return mSparring; }
    void DropSparring(Unit* who);
    void Disengage(float radius, bool toggleAutoAttack);

    void SetScript9(SmartScriptHolder& e, uint32 entry, Unit* invoker);
    SmartScript* GetScript() { return &mScript; }
    bool IsEscortInvokerInRange();

    void Notify(uint32 id, Unit* source = nullptr) override;

    // Debug information
    void GetAIInformation(ChatHandler& reader) override;

    // Called when creature is spawned or respawned
    void JustRespawned() override;

    // Called after InitializeAI(), EnterEvadeMode() for resetting variables
    void Reset() override;

    // Called at reaching home after evade
    void JustReachedHome() override;

    // Called for reaction at enter to combat if not in combat yet (enemy can be
    // NULL)
    void EnterCombat(Unit* enemy) override;

    // Called for reaction at stopping attack at no attackers or targets
    void EnterEvadeMode(bool by_group = false) override;

    // Called when NPC leashed
    void Leashed() override;

    // Same as JustDied except it happens before the actual death state has been
    // toggled, and therefore allows last-minute casting
    void BeforeDeath(Unit* killer) override;

    // Called when the creature is killed
    void JustDied(Unit* killer) override;

    // Called when the creature kills a unit
    void KilledUnit(Unit* victim) override;

    // Called when the creature summon successfully other creature
    void JustSummoned(Creature* creature) override;

    // Tell creature to attack and follow the victim
    void AttackStart(Unit* who) override;

    // Called if IsVisible(Unit* who) is true at each *who move, reaction at
    // visibility zone enter
    void MoveInLineOfSight(Unit* who) override;

    // Called to check if we can see his moving in front of us
    bool IsVisible(Unit* pWho) const override;

    // Called when hit by a spell
    void SpellHit(Unit* unit, const SpellEntry* spellInfo) override;

    // Called when spell hits a target
    void SpellHitTarget(Unit* target, const SpellEntry* spellInfo) override;

    // Called at any Damage from any attacker (before damage apply)
    void DamageTaken(Unit* doneBy, uint32& damage) override;

    // Called when the creature receives heal
    void HealReceived(Unit* doneBy, uint32& addhealth);

    // Called at World update tick
    void UpdateAI(const uint32 diff) override;

    // Called at text emote receive from player
    void ReceiveEmote(Player* player, uint32 textEmote) override;

    // Called when creature is added to world
    void OnSpawn() override;

    // Called at waypoint reached or point movement finished
    void MovementInform(movement::gen MovementType, uint32 Data) override;

    // Called when creature is summoned by another WorldObject
    void SummonedBy(WorldObject* summoner) override;

    // Called at any Damage to any victim (before damage apply)
    void DamageDealt(
        Unit* doneTo, uint32& damage, DamageEffectType /*damagetype*/);

    // Called when a summoned creature dissapears (UnSommoned)
    void SummonedCreatureDespawn(Creature* unit) override;

    // called when the corpse of this creature gets removed
    void CorpseRemoved(uint32& respawnDelay) override;

    // Called at World update tick if creature is charmed
    void UpdateAIWhileCharmed(const uint32 diff);

    // Called when gets initialized, when creature is added to world
    void InitializeAI() override;

    // Called when creature gets charmed by another unit
    void OnCharmed(bool apply) override;

    // Called when victim is in line of sight
    bool CanAIAttack(const Unit* who) const;

    // Called when the creature is target of hostile action: swing, hostile
    // spell landed, fear/etc)
    void AttackedBy(Unit* attacker) override;

    // Used in scripts to share variables
    void DoAction(const int32 param = 0);

    // Used in scripts to share variables
    uint32 GetData(uint32 id = 0) override;

    // Used in scripts to share variables
    void SetData(uint32 id, uint32 value) override;

    // Used in scripts to share variables
    void SetGUID(uint64 guid, int32 id = 0) override;

    // Used in scripts to share variables
    uint64 GetGUID(int32 id = 0) override;

    bool IgnoreTarget(Unit* target) const override;

    // Called at movepoint reached
    void MovepointReached(uint32 id);

    // Makes the creature run/walk
    void SetRun(bool run = true);
    bool GetRun() const { return mRun; }

    void SetFly(bool fly = true);

    void SetSwim(bool swim = true);

    void SetInvincibilityHpLevel(uint32 level)
    {
        mInvincibilityHpLevel = level;
    }
    uint32 GetInvincibilityHpLevel() const { return mInvincibilityHpLevel; }

    bool OnGossipHello(Player* player) override;
    bool OnGossipSelect(Player* player, uint32 sender, uint32 action,
        uint32 menuId = 0, const char* code = nullptr) override;
    void OnQuestAccept(Player* player, Quest const* quest) override;
    void OnQuestSelect(Player* player, Quest const* quest);
    void OnQuestComplete(Player* player, Quest const* quest);
    void OnQuestReward(Player* player, Quest const* quest) override;
    bool OnDummyEffect(
        Unit* caster, uint32 spellId, SpellEffectIndex effIndex) override;
    void OnGameEvent(bool start, uint32 eventId) override;

    void ResetInternal(SmartResetType type);

    uint32 mEscortQuestID;

    void RemoveAuras();

    void OnSpellClick(Unit* clicker);

    void ToggleBehavioralAI(bool state);
    void ChangeBehavioralAI(uint32 behavior);

    void DisableCombatReactions(bool disable)
    {
        mCombatReactionsDisabled = disable;
    }

    bool use_pet_behavior() const;

    bool OOCWpReached() const { return mOOCWPReached; }

    void save_pos(float x, float y, float z, float o)
    {
        mSavedPos[0] = x;
        mSavedPos[1] = y;
        mSavedPos[2] = z;
        mSavedPos[3] = o;
    }

    void get_saved_pos(float& x, float& y, float& z, float& o)
    {
        x = mSavedPos[0];
        y = mSavedPos[1];
        z = mSavedPos[2];
        o = mSavedPos[3];
    }

    void SetPassive(bool b) { mPassive = b; }
    void UpdatePassive();

    void SetGroup(int id) { mCreatureGroup = id; }
    int GetGroup() const { return mCreatureGroup; }
    void ClearGroup();

    void UpdateBehavioralAIPhase(int phase) { mBehavioralAI.SetPhase(phase); }

    BehavioralAI& GetBehavioralAI() { return mBehavioralAI; }

private:
    void UpdatePath(const uint32 diff);
    SmartScript mScript;
    WPPath* mWayPoints;
    uint32 mEscortState;
    std::set<ObjectGuid> mEscorters;
    bool mHasEscorters;
    uint32 mNextWPID;
    uint32 mLastWPIDReached;
    bool mWPReached;
    bool mOOCWPReached;
    uint32 mWPPauseTimer;
    WayPoint* mLastWP;
    Position mLastOOCPos; // set on enter combat
    uint32 GetWPCount() { return mWayPoints ? mWayPoints->size() : 0; }
    int mEscortDespawn;
    bool mCanRepeatPath;
    bool mRun;
    bool mEscortRun;
    bool mCanAutoAttack;
    bool mCanCombatMove;
    bool mForcedPaused;
    uint32 mInvincibilityHpLevel;
    bool AssistPlayerInCombat(Unit* who);

    uint32 mEscortInvokerCheckTimer;
    bool mSparring;
    BehavioralAI mBehavioralAI;
    bool mBehavioralAIMovingUs;
    bool mCombatReactionsDisabled;
    bool mDisengageToggleAutoAttack;
    bool mPassive;

    float mSavedPos[4];

    int mCreatureGroup;
};

class SmartGameObjectAI : public GameObjectAI
{
public:
    SmartGameObjectAI(GameObject* g) : GameObjectAI(g), go(g) {}
    ~SmartGameObjectAI() {}

    void UpdateAI(uint32 diff) override;
    void InitializeAI() override;
    void Reset() override;
    SmartScript* GetScript() { return &mScript; }

    bool OnGossipHello(Player* player) override;
    bool OnGossipSelect(Player* player, uint32 sender, uint32 action,
        uint32 menuId = 0, const char* code = nullptr) override;
    void OnQuestAccept(Player* player, Quest const* quest) override;
    void OnQuestReward(Player* player, Quest const* quest) override;
    uint32 GetDialogStatus(Player* player) override;
    void SetData(uint32 id, uint32 value) override;
    void SetScript9(SmartScriptHolder& e, uint32 entry, Unit* invoker);
    void OnGameEvent(bool start, uint32 eventId) override;
    void OnStateChanged(uint32 state, Unit* unit) override;

protected:
    GameObject* const go;
    SmartScript mScript;
};

#endif
