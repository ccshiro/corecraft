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

#ifndef MANGOS_CREATUREAI_H
#define MANGOS_CREATUREAI_H

#include "Common.h"
#include "DBCEnums.h"
#include "movement/generators.h"
#include "ObjectGuid.h"
#include "SharedDefines.h"
#include "Platform/Define.h"
#include "Policies/Singleton.h"

class BehavioralAI;
class ChatHandler;
class Creature;
class GameObject;
class Player;
class Quest;
struct SpellEntry;
class Unit;
class WorldObject;

#define TIME_INTERVAL_LOOK 5000
#define VISIBILITY_RANGE 10000
#define KILL_SAY_COOLDOWN 7 * IN_MILLISECONDS // minimum time between kill says

enum CanCastResult
{
    CAST_OK = 0,
    CAST_FAIL_IS_CASTING = 1,
    CAST_FAIL_OTHER = 2,
    CAST_FAIL_TOO_FAR = 3,
    CAST_FAIL_TOO_CLOSE = 4,
    CAST_FAIL_POWER = 5,
    CAST_FAIL_STATE = 6,
    CAST_FAIL_TARGET_AURA = 7,
    CAST_FAIL_LOS = 8,
};

enum CastFlags
{
    CAST_INTERRUPT_PREVIOUS = 0x01, // Interrupt any spell casting
    CAST_TRIGGERED = 0x02, // Triggered (this makes spell cost zero mana and
                           // have no cast time)
    CAST_FORCE_CAST =
        0x04, // Forces cast even if creature is out of mana or out of range
    CAST_NO_MELEE_IF_OOM = 0x08, // Prevents creature from entering melee if out
                                 // of mana or out of range
    CAST_FORCE_TARGET_SELF =
        0x10, // Forces the target to cast this spell on itself
    CAST_AURA_NOT_PRESENT = 0x20, // Only casts the spell if the target does not
                                  // have an aura from the spell
};

class MANGOS_DLL_SPEC CreatureAI
{
public:
    explicit CreatureAI(Creature* creature)
      : m_creature(creature), pacified_(false)
    {
    }

    virtual ~CreatureAI();

    ///== Information about AI ========================
    /**
     * Function added as a part of the port from TrinityCore
     * that added their SmartAI system. This is called
     * to initialize the AI.
     */
    virtual void InitializeAI();

    /**
     * Function added as a part of the port from TrinityCore
     * that added their SmartAI system. This is called
     * to reset the AI.
     */
    virtual void Reset() {}

    /**
     * This funcion is used to display information about the AI.
     * It is called when the .npc aiinfo command is used.
     * Use this for on-the-fly debugging
     * @param reader is a ChatHandler to send messages to.
     */
    virtual void GetAIInformation(ChatHandler& /*reader*/) {}

    /**
     * Used for inter-AI communcation.
     */
    virtual void Notify(uint32 /*id*/, Unit* /*source*/ = nullptr) {}

    ///== Reactions At =================================

    /**
     * Called if IsVisible(Unit* pWho) is true at each (relative) pWho move,
     * reaction at visibility zone enter
     * Note: The Unit* pWho can be out of Line of Sight, usually this is only
     * visibiliy (by state) and range dependendend
     * Note: This function is not called for creatures who are in evade mode
     * @param pWho Unit* who moved in the visibility range and is visisble
     */
    virtual void MoveInLineOfSight(Unit* /*pWho*/) {}

    /**
     * Called for reaction at enter to combat if not in combat yet
     * @param pEnemy Unit* of whom the Creature enters combat with, can be NULL
     */
    virtual void EnterCombat(Unit* /*pEnemy*/) {}

    /**
     * Called for reaction at stopping attack at no attackers or targets
     * This is called usually in Unit::SelectHostileTarget, if no more target
     * exists
     */
    virtual void EnterEvadeMode(bool = false) {}

    // Called when NPC leashed
    virtual void Leashed() {}

    /**
     * Called at reaching home after MoveTargetedHome
     */
    virtual void JustReachedHome() {}

    // Called at any heal cast/item used (call non implemented)
    // virtual void HealBy(Unit * /*healer*/, uint32 /*amount_healed*/) {}

    /**
     * Called at spell Damage calculation to allow script to override damage
     * @param pDoneTo Unit* to whom the spell is about to hit (MUST CHECK FOR
     * NULL BEFORE USING)
     * @param uiDamage Amount of Damage that will be dealt, can be changed here
     * @param pSpell The spell that is dealing the damage
     */
    virtual void SpellDamageCalculation(const Unit* /*pDoneTo*/,
        int32& /*iDamage*/, const SpellEntry* /*pSpell*/,
        SpellEffectIndex /*effectIndex*/)
    {
    }

    /**
     * Called at any Damage to any victim (before damage apply)
     * @param pDoneTo Unit* to whom Damage of amount uiDamage will be dealt
     * @param uiDamage Amount of Damage that will be dealt, can be changed here
     */
    virtual void DamageDeal(Unit* /*pDoneTo*/, uint32& /*uiDamage*/) {}

    /**
     * Called at any Damage from any attacker (before damage apply)
     * Note:    Use for recalculation damage or special reaction at damage
     *          for attack reaction use AttackedBy called for not DOT damage in
     * Unit::DealDamage also
     * @param pDealer Unit* who will deal Damage to the creature
     * @param uiDamage Amount of Damage that will be dealt, can be changed here
     */
    virtual void DamageTaken(Unit* /*pDealer*/, uint32& /*uiDamage*/) {}

    /**
     * Same as JustDied except it happens before the actual death state has been
     * toggled, and therefore allows last-minute casting
     * @param pKiller Unit* who killed the creature
     */
    virtual void BeforeDeath(Unit* /*pKiller*/) {}

    /**
     * Called when the creature is killed
     * @param pKiller Unit* who killed the creature
     */
    virtual void JustDied(Unit* /*pKiller*/) {}

    /**
     * Called when the corpse of this creature gets removed
     * @param uiRespawnDelay Delay (in seconds). If != 0, then this is the time
     * after which the creature will respawn, if = 0 the default respawn-delay
     * will be used
     */
    virtual void CorpseRemoved(uint32& /*uiRespawnDelay*/) {}

    /**
     * Called when a summoned creature is killed
     * @param pSummoned Summoned Creature* that got killed
     */
    virtual void SummonedCreatureJustDied(Creature* /*pSummoned*/) {}

    /**
     * Called when the creature kills a unit
     * @param pVictim Victim that got killed
     */
    virtual void KilledUnit(Unit* /*pVictim*/) {}

    /**
     * Called when owner of m_creature (if m_creature is PROTECTOR_PET) kills a
     * unit
     * @param pVictim Victim that got killed (by owner of creature)
     */
    virtual void OwnerKilledUnit(Unit* /*pVictim*/) {}

    /**
     * Called when the creature summon successfully other creature
     * @param pSummoned Creature that got summoned
     */
    virtual void JustSummoned(Creature* /*pSummoned*/) {}

    /**
     * Called when the creature summon successfully a gameobject
     * @param pGo GameObject that was summoned
     */
    virtual void JustSummoned(GameObject* /*pGo*/) {}

    /**
     * Called when the creature was summoned by another unit
     * @param pWo WorldObject* that summoned us
     */
    virtual void SummonedBy(WorldObject* /*pWo*/) {}

    /**
     * Called when a summoned creature gets TemporarySummon::UnSummon ed
     * @param pSummoned Summoned creature that despawned
     */
    virtual void SummonedCreatureDespawn(Creature* /*pSummoned*/) {}

    /**
     * Called when hit by a spell
     * @param pCaster Caster who casted the spell
     * @param pSpell The spell that hit the creature
     */
    virtual void SpellHit(Unit* /*pCaster*/, const SpellEntry* /*pSpell*/) {}

    /**
     * Called when spell hits creature's target
     * @param pTarget Target that we hit with the spell
     * @param pSpell Spell with which we hit pTarget
     */
    virtual void SpellHitTarget(Unit* /*pTarget*/, const SpellEntry* /*pSpell*/)
    {
    }

    /**
     * Called when the creature is target of hostile action: swing, hostile
     * spell landed, fear/etc)
     * @param pAttacker Unit* who attacked the creature
     */
    virtual void AttackedBy(Unit* /*pAttacker*/);

    /**
     * Called when creature is respawned (for reseting variables)
     */
    virtual void JustRespawned() {}

    /**
     * Called at waypoint reached or point movement finished
     * @param uiMovementType Type of the movement
     * @param uiData Data related to the finished movement (ie point-id)
     */
    virtual void MovementInform(movement::gen /*gen*/, uint32 /*uiData*/) {}

    /**
     * Called if a temporary summoned of m_creature reach a move point
     * @param pSummoned Summoned Creature that finished some movement
     * @param uiMotionType Type of the movement
     * @param uiData Data related to the finished movement (ie point-id)
     */
    virtual void SummonedMovementInform(
        Creature* /*pSummoned*/, movement::gen /*gen*/, uint32 /*uiData*/)
    {
    }

    /**
     * Called at text emote receive from player
     * @param pPlayer Player* who sent the emote
     * @param uiEmote ID of the emote the player used with the creature as
     * target
     */
    virtual void ReceiveEmote(Player* /*pPlayer*/, uint32 /*uiEmote*/) {}

    /**
     * Called when creature is added to world
    */
    virtual void OnSpawn() {}

    /**
     * Called when creature is struck by a white melee attack (includes misses,
     * parries, etc)
     * @param pAttacker: Unit* who attacked creature
     * @param attType: enum WeaponAttackType
     * @param damage: the damage taken
     * @param hitInfo: enum HitInfo (if attack was parried, blocked, etc)
     */
    virtual void OnTakenWhiteHit(Unit* /*pAttacker*/,
        WeaponAttackType /*attType*/, uint32 /*damage*/, uint32 /*hitInfo*/)
    {
    }

    /**
     * Called when creature strikes with a white melee attack (includes misses,
     * parries, etc)
     * @param pAttacker: Unit* creature attacked
     * @param attType: enum WeaponAttackType
     * @param damage: the damage dealt
     * @param hitInfo: enum HitInfo (if attack was parried, blocked, etc)
     */
    virtual void OnWhiteHit(Unit* /*pAttacker*/, WeaponAttackType /*attType*/,
        uint32 /*damage*/, uint32 /*hitInfo*/)
    {
    }

    ///== Triggered Actions Requested ==================

    /**
     * Called when creature attack expected (if creature can and no have current
     * victim)
     * Note: for reaction at hostile action must be called AttackedBy function.
     * Note: Usually called by MoveInLineOfSight, in Unit::SelectHostileTarget
     * or when the AI is forced to attack an enemy
     * @param pWho Unit* who is possible target
     */
    virtual void AttackStart(Unit* /*pWho*/) {}

    /**
     * Called at World update tick, by default every 100ms
     * This setting is dependend on CONFIG_UINT32_INTERVAL_MAPUPDATE
     * Note: Use this function to handle Timers, Threat-Management and
     * MeleeAttacking
     * @param uiDiff Passed time since last call
     */
    virtual void UpdateAI(const uint32 /*uiDiff*/) {}

    /* Callbacks for quests */
    /**
     * Callback for when @player gossips with creature.
     * @Return: true if you wish to override default behavior, false if you wish
     * to fall back on it.
     */
    virtual bool OnGossipHello(Player* /*player*/) { return false; }

    /**
     * Callback for when @player selects a gossip option from creature.
     * @Return: true if you wish to override default behavior, false if you
     * still wish to make use of it.
     */
    virtual bool OnGossipSelect(Player* /*player*/, uint32 /*sender*/,
        uint32 /*gossip_menu_option.id*/, uint32 /*menuId*/ = 0,
        const char* /*code*/ = nullptr)
    {
        return false;
    }

    /**
     * Callback when @quest accepted by @player.
     */
    virtual void OnQuestAccept(Player* /*player*/, Quest const* /*quest*/) {}

    /**
     * Callback when @quest rewarded by @player.
     */
    virtual void OnQuestReward(Player* /*player*/, Quest const* /*quest*/) {}

    /**
     * Callback when @spellId with @effIndex hits creature. Casted by @caster.
     * @Return: Return true to take responsibility of the following behavior.
     * False if you wish the core to handle the dummy effect.
     */
    virtual bool OnDummyEffect(
        Unit* /*caster*/, uint32 /*spellId*/, SpellEffectIndex /*effIndex*/)
    {
        return false;
    }

    /**
     * Callback when event with id @eventId eithers starts or finishes (with
     * creature as source), as indicated by @start.
     */
    virtual void OnGameEvent(bool /*start*/, uint32 /*eventId*/) {}

    /**
     * Callback when charm is applied or removed.
     */
    virtual void OnCharmed(bool /*apply*/) {}

    ///== State checks =================================

    /**
     * Check if unit is visible for MoveInLineOfSight
     * Note: This check is by default only the state-depending (visibility,
     * range), NOT LineOfSight
     * @param pWho Unit* who is checked if it is visisble for the creature
     */
    virtual bool IsVisible(Unit* /*pWho*/) const { return false; }

    // Called when victim entered water and creature can not enter water
    // TODO: rather unused
    virtual bool canReachByRangeAttack(Unit*) { return false; }

    void Pacify(bool state);
    bool IsPacified() const { return pacified_; }

    // Returns true if derivitive AI considers that the target should be ignored
    // for primary target selection
    virtual bool IgnoreTarget(Unit* target) const;

    ///== Helper functions =============================

    /// This function is used to do the actual melee damage (if possible)
    bool DoMeleeAttackIfReady();

    /// Internal helper function, to check if a spell can be cast
    CanCastResult CanCastSpell(
        Unit* pTarget, const SpellEntry* pSpell, bool isTriggered);

    /**
     * Function to cast a spell if possible
     * @param pTarget Unit* onto whom the spell should be cast
     * @param uiSpell ID of the spell that the creature will try to cast
     * @param uiCastFlags Some flags to define how to cast, see enum CastFlags
     * @param OriginalCasterGuid the original caster of the spell if required,
     * empty by default
     */
    CanCastResult DoCastSpellIfCan(Unit* pTarget, uint32 uiSpell,
        uint32 uiCastFlags = 0, ObjectGuid OriginalCasterGuid = ObjectGuid());

    CanCastResult CanCastSpell(Unit* pTarget, uint32 uiSpell, bool isTriggered,
        uint32 uiCastFlags = 0);

    virtual uint32 GetData(uint32 /*id = 0*/) { return 0; }
    virtual void SetData(uint32 /*id*/, uint32 /*value*/) {}
    virtual void SetGUID(uint64 /*guid*/, int32 /*id*/ = 0) {}
    virtual uint64 GetGUID(int32 /*id*/ = 0) { return 0; }

    ///== Fields =======================================

    /// Pointer to the Creature controlled by this AI
    Creature* const m_creature;
    bool pacified_;
};

CreatureAI* make_ai_for(Creature* c);

#endif
