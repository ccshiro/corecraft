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

#ifndef __SPELL_H
#define __SPELL_H

#include "Common.h"
#include "DBCEnums.h"
#include "GameObject.h"
#include "LootMgr.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "SharedDefines.h"
#include "Unit.h"
#include "G3D/Vector3.h"
#include "maps/map_grid.h"

class Aura;
class DynamicObj;
class Group;
class Item;
class PathFinder;
class SpellCastTargets;
class WorldPacket;
class WorldSession;

enum SpellCastFlags
{
    CAST_FLAG_NONE = 0x00000000,
    CAST_FLAG_HIDDEN_COMBATLOG = 0x00000001, // hide in combat log?
    CAST_FLAG_UNKNOWN2 = 0x00000002,
    CAST_FLAG_UNKNOWN3 = 0x00000004,
    CAST_FLAG_UNKNOWN4 = 0x00000008,
    CAST_FLAG_UNKNOWN5 = 0x00000010,
    CAST_FLAG_AMMO = 0x00000020, // Projectiles visual
    CAST_FLAG_UNKNOWN7 =
        0x00000040, // !0x41 mask used to call CGTradeSkillInfo::DoRecast
    CAST_FLAG_UNKNOWN8 = 0x00000080,
    CAST_FLAG_UNKNOWN9 = 0x00000100,
};

enum SpellNotifyPushType
{
    PUSH_IN_FRONT,
    PUSH_IN_FRONT_90,
    PUSH_IN_FRONT_15,
    PUSH_IN_BACK,
    PUSH_IN_BACK_90,
    PUSH_IN_BACK_15,
    PUSH_SELF_CENTER,
    PUSH_DEST_CENTER,
    PUSH_TARGET_CENTER
};

bool IsQuestTameSpell(uint32 spellId);

struct SpellCastTargetsReader
{
    explicit SpellCastTargetsReader(SpellCastTargets& _targets, Unit* _caster)
      : targets(_targets), caster(_caster)
    {
    }

    SpellCastTargets& targets;
    Unit* caster;
};

class SpellCastTargets
{
public:
    SpellCastTargets();
    ~SpellCastTargets();

    void read(ByteBuffer& data, Unit* caster);
    void write(ByteBuffer& data) const;

    SpellCastTargetsReader ReadForCaster(Unit* caster)
    {
        return SpellCastTargetsReader(*this, caster);
    }

    SpellCastTargets& operator=(const SpellCastTargets& target)
    {
        m_unitTarget = target.m_unitTarget;
        m_itemTarget = target.m_itemTarget;
        m_GOTarget = target.m_GOTarget;

        m_unitTargetGUID = target.m_unitTargetGUID;
        m_GOTargetGUID = target.m_GOTargetGUID;
        m_CorpseTargetGUID = target.m_CorpseTargetGUID;
        m_itemTargetGUID = target.m_itemTargetGUID;

        m_itemTargetEntry = target.m_itemTargetEntry;

        m_srcX = target.m_srcX;
        m_srcY = target.m_srcY;
        m_srcZ = target.m_srcZ;

        m_destX = target.m_destX;
        m_destY = target.m_destY;
        m_destZ = target.m_destZ;

        m_strTarget = target.m_strTarget;

        m_targetMask = target.m_targetMask;

        return *this;
    }

    ObjectGuid getUnitTargetGuid() const { return m_unitTargetGUID; }
    Unit* getUnitTarget() const { return m_unitTarget; }
    void setUnitTarget(Unit* target);
    void setDestination(float x, float y, float z);
    void setSource(float x, float y, float z);
    void setScriptTarget(Unit* target);

    ObjectGuid getGOTargetGuid() const { return m_GOTargetGUID; }
    GameObject* getGOTarget() const { return m_GOTarget; }
    void setGOTarget(GameObject* target);

    ObjectGuid getCorpseTargetGuid() const { return m_CorpseTargetGUID; }
    void setCorpseTarget(Corpse* corpse);

    ObjectGuid getItemTargetGuid() const { return m_itemTargetGUID; }
    Item* getItemTarget() const { return m_itemTarget; }
    uint32 getItemTargetEntry() const { return m_itemTargetEntry; }
    void setItemTarget(Item* item);
    void setTradeItemTarget(Player* caster);
    void updateTradeSlotItem()
    {
        if (m_itemTarget && (m_targetMask & TARGET_FLAG_TRADE_ITEM))
        {
            m_itemTargetGUID = m_itemTarget->GetObjectGuid();
            m_itemTargetEntry = m_itemTarget->GetEntry();
        }
    }

    bool IsEmpty() const
    {
        return !m_GOTargetGUID && !m_unitTargetGUID && !m_itemTarget &&
               !m_CorpseTargetGUID;
    }

    void Update(Unit* caster);

    float m_srcX, m_srcY, m_srcZ;
    float m_destX, m_destY, m_destZ;
    std::string m_strTarget;

    uint32 m_targetMask;

private:
    // objects (can be used at spell creating and after Update at casting
    Unit* m_unitTarget;
    GameObject* m_GOTarget;
    Item* m_itemTarget;

    // object GUID/etc, can be used always
    ObjectGuid m_unitTargetGUID;
    ObjectGuid m_GOTargetGUID;
    ObjectGuid m_CorpseTargetGUID;
    ObjectGuid m_itemTargetGUID;
    ObjectGuid m_scriptTargetGUID;
    uint32 m_itemTargetEntry;
};

inline ByteBuffer& operator<<(ByteBuffer& buf, SpellCastTargets const& targets)
{
    targets.write(buf);
    return buf;
}

inline ByteBuffer& operator>>(
    ByteBuffer& buf, SpellCastTargetsReader const& targets)
{
    targets.targets.read(buf, targets.caster);
    return buf;
}

enum SpellState
{
    SPELL_STATE_PREPARING = 0, // cast time delay period, non channeled spell
    SPELL_STATE_CASTING = 1,   // channeled time period spell casting state
    SPELL_STATE_FINISHED = 2,  // cast finished to success or fail
    SPELL_STATE_DELAYED = 3,   // spell casted but need time to hit target(s)
    SPELL_STATE_CHARGE_SPECIAL = 4 // waiting for path-generation to finish
};

enum SpellTargets
{
    SPELL_TARGETS_HOSTILE,
    SPELL_TARGETS_NOT_FRIENDLY,
    SPELL_TARGETS_NOT_HOSTILE,
    SPELL_TARGETS_FRIENDLY,
    SPELL_TARGETS_AOE_DAMAGE,
    SPELL_TARGETS_ALL
};

typedef std::multimap<uint64, uint64> SpellTargetTimeMap;

class Spell
{
    friend struct aoe_targets_worker;
    friend void Unit::SetCurrentCastedSpell(Spell* pSpell);

public:
    void EffectEmpty(SpellEffectIndex eff_idx);
    void EffectNULL(SpellEffectIndex eff_idx);
    void EffectUnused(SpellEffectIndex eff_idx);
    void EffectDistract(SpellEffectIndex eff_idx);
    void EffectPull(SpellEffectIndex eff_idx);
    void EffectSchoolDMG(SpellEffectIndex eff_idx);
    void EffectEnvironmentalDMG(SpellEffectIndex eff_idx);
    void EffectInstaKill(SpellEffectIndex eff_idx);
    void EffectDummy(SpellEffectIndex eff_idx);
    void EffectTeleportUnits(SpellEffectIndex eff_idx);
    void EffectApplyAura(SpellEffectIndex eff_idx);
    void EffectSendEvent(SpellEffectIndex eff_idx);
    void EffectPowerBurn(SpellEffectIndex eff_idx);
    void EffectPowerDrain(SpellEffectIndex eff_idx);
    void EffectHeal(SpellEffectIndex eff_idx);
    void EffectBind(SpellEffectIndex eff_idx);
    void EffectHealthLeech(SpellEffectIndex eff_idx);
    void EffectQuestComplete(SpellEffectIndex eff_idx);
    void EffectCreateItem(SpellEffectIndex eff_idx);
    void EffectPersistentAA(SpellEffectIndex eff_idx);
    void EffectEnergize(SpellEffectIndex eff_idx);
    void EffectOpenLock(SpellEffectIndex eff_idx);
    void EffectSummonChangeItem(SpellEffectIndex eff_idx);
    void EffectProficiency(SpellEffectIndex eff_idx);
    void EffectApplyAreaAura(SpellEffectIndex eff_idx);
    void EffectSummonType(SpellEffectIndex eff_idx);
    void EffectLearnSpell(SpellEffectIndex eff_idx);
    void EffectDispel(SpellEffectIndex eff_idx);
    void EffectDualWield(SpellEffectIndex eff_idx);
    void EffectPickPocket(SpellEffectIndex eff_idx);
    void EffectAddFarsight(SpellEffectIndex eff_idx);
    void EffectHealMechanical(SpellEffectIndex eff_idx);
    void EffectTeleUnitsFaceCaster(SpellEffectIndex eff_idx);
    void EffectLearnSkill(SpellEffectIndex eff_idx);
    void EffectAddHonor(SpellEffectIndex eff_idx);
    void EffectTradeSkill(SpellEffectIndex eff_idx);
    void EffectEnchantItemPerm(SpellEffectIndex eff_idx);
    void EffectEnchantItemTmp(SpellEffectIndex eff_idx);
    void EffectTameCreature(SpellEffectIndex eff_idx);
    void EffectSummonPet(SpellEffectIndex eff_idx);
    void EffectLearnPetSpell(SpellEffectIndex eff_idx);
    void EffectWeaponDmg(SpellEffectIndex eff_idx);
    void EffectForceCast(SpellEffectIndex eff_idx);
    void EffectTriggerSpell(SpellEffectIndex eff_idx);
    void EffectTriggerMissileSpell(SpellEffectIndex eff_idx);
    void EffectThreat(SpellEffectIndex eff_idx);
    void EffectHealMaxHealth(SpellEffectIndex eff_idx);
    void EffectInterruptCast(SpellEffectIndex eff_idx);
    void EffectSummonObjectWild(SpellEffectIndex eff_idx);
    void EffectScriptEffect(SpellEffectIndex eff_idx);
    void EffectSanctuary(SpellEffectIndex eff_idx);
    void EffectAddComboPoints(SpellEffectIndex eff_idx);
    void EffectDuel(SpellEffectIndex eff_idx);
    void EffectStuck(SpellEffectIndex eff_idx);
    void EffectSummonPlayer(SpellEffectIndex eff_idx);
    void EffectActivateObject(SpellEffectIndex eff_idx);
    void EffectEnchantHeldItem(SpellEffectIndex eff_idx);
    void EffectSummonObject(SpellEffectIndex eff_idx);
    void EffectResurrect(SpellEffectIndex eff_idx);
    void EffectParry(SpellEffectIndex eff_idx);
    void EffectBlock(SpellEffectIndex eff_idx);
    void EffectLeapForward(SpellEffectIndex eff_idx);
    void EffectLeapBack(SpellEffectIndex eff_idx);
    void EffectTransmitted(SpellEffectIndex eff_idx);
    void EffectDisEnchant(SpellEffectIndex eff_idx);
    void EffectInebriate(SpellEffectIndex eff_idx);
    void EffectFeedPet(SpellEffectIndex eff_idx);
    void EffectDismissPet(SpellEffectIndex eff_idx);
    void EffectReputation(SpellEffectIndex eff_idx);
    void EffectSelfResurrect(SpellEffectIndex eff_idx);
    void EffectSkinning(SpellEffectIndex eff_idx);
    void EffectCharge(SpellEffectIndex eff_idx);
    void EffectCharge2(SpellEffectIndex eff_idx);
    void EffectProspecting(SpellEffectIndex eff_idx);
    void EffectRedirectThreat(SpellEffectIndex eff_idx);
    void EffectSendTaxi(SpellEffectIndex eff_idx);
    void EffectKnockBack(SpellEffectIndex eff_idx);
    void EffectPlayerPull(SpellEffectIndex eff_idx);
    void EffectDispelMechanic(SpellEffectIndex eff_idx);
    void EffectSummonDeadPet(SpellEffectIndex eff_idx);
    void EffectDestroyAllTotems(SpellEffectIndex eff_idx);
    void EffectDurabilityDamage(SpellEffectIndex eff_idx);
    void EffectSkill(SpellEffectIndex eff_idx);
    void EffectTaunt(SpellEffectIndex eff_idx);
    void EffectDurabilityDamagePCT(SpellEffectIndex eff_idx);
    void EffectModifyThreatPercent(SpellEffectIndex eff_idx);
    void EffectResurrectNew(SpellEffectIndex eff_idx);
    void EffectAddExtraAttacks(SpellEffectIndex eff_idx);
    void EffectSpiritHeal(SpellEffectIndex eff_idx);
    void EffectSkinPlayerCorpse(SpellEffectIndex eff_idx);
    void EffectStealBeneficialBuff(SpellEffectIndex eff_idx);
    void EffectUnlearnSpecialization(SpellEffectIndex eff_idx);
    void EffectHealPct(SpellEffectIndex eff_idx);
    void EffectEnergisePct(SpellEffectIndex eff_idx);
    void EffectTriggerSpellWithValue(SpellEffectIndex eff_idx);
    void EffectTriggerRitualOfSummoning(SpellEffectIndex eff_idx);
    void EffectKillCreditGroup(SpellEffectIndex eff_idx);
    void EffectQuestFail(SpellEffectIndex eff_idx);
    void EffectPlaySound(SpellEffectIndex eff_idx);
    void EffectPlayMusic(SpellEffectIndex eff_idx);

    Spell(Unit* caster, SpellEntry const* info, spell_trigger_type triggered,
        ObjectGuid originalCasterGUID = ObjectGuid(),
        SpellEntry const* triggeredBy = nullptr);
    ~Spell();

    static bool attempt_pet_cast(Creature* pet, const SpellEntry* info,
        SpellCastTargets targets, bool owner_cast);
    void prepare(
        SpellCastTargets const* targets, Aura* triggeredByAura = nullptr);

    void cancel(Spell* replacedBy = nullptr);

    void update(uint32 difftime);
    void cast(bool skipCheck = false);
    void finish(bool ok = true);
    void TakePower();
    void TakeReagents();
    void TakeCastItem();
    void RefundReagents();

    SpellCastResult CheckPetCast(Unit* target);

    SpellCastResult CheckCast(bool strict);

    // handlers
    void handle_immediate();
    uint64 handle_delayed(uint64 t_offset);
    // handler helpers
    void _handle_immediate_phase();
    void _handle_finish_phase();

    // CheckCast() internals
    SpellCastResult CheckCooldowns(bool strict);
    SpellCastResult CheckRange(bool strict);
    SpellCastResult CheckCasterState(bool strict);
    SpellCastResult CheckCastTarget();
    SpellCastResult CheckItems();
    SpellCastResult CheckDbTarget();
    SpellCastResult CheckPower();
    SpellCastResult CheckCasterAuras() const;
    SpellCastResult CheckEffects(bool strict);
    SpellCastResult CheckAuras();
    SpellCastResult CheckTrade();

    int32 CalculateDamage(SpellEffectIndex i, Unit* target)
    {
        return m_caster->CalculateSpellDamage(
            target, m_spellInfo, i, &m_currentBasePoints[i]);
    }
    static uint32 CalculatePowerCost(SpellEntry const* spellInfo, Unit* caster,
        Spell const* spell = nullptr, Item* castItem = nullptr);

    bool HaveTargetsForEffect(SpellEffectIndex effect) const;
    void Delayed();
    void DelayedChannel();
    uint32 getState() const { return m_spellState; }
    void setState(uint32 state) { m_spellState = state; }

    void DoCreateItem(SpellEffectIndex eff_idx, uint32 itemtype);
    void DoSummon(SpellEffectIndex eff_idx);
    void DoSummonWild(SpellEffectIndex eff_idx, uint32 forceFaction = 0);
    void DoSummonGuardian(SpellEffectIndex eff_idx, uint32 forceFaction = 0);
    void DoSummonTotem(SpellEffectIndex eff_idx, uint8 slot_dbc = 0);
    void DoSummonCritter(SpellEffectIndex eff_idx, uint32 forceFaction = 0);

    void WriteSpellGoTargets(WorldPacket* data);
    void WriteAmmoToPacket(WorldPacket* data);

    typedef std::list<Unit*> UnitList;
    void FillTargetMap();
    void SetTargetMap(
        SpellEffectIndex effIndex, uint32 targetMode, UnitList& targetUnitMap);

    void FillAreaTargets(UnitList& targetUnitMap, float radius,
        SpellNotifyPushType pushType, SpellTargets spellTargets,
        WorldObject* originalCaster = nullptr);
    void FillRaidOrPartyTargets(UnitList& targetUnitMap, Unit* member,
        float radius, bool raid, bool withPets, bool withcaster);

    template <typename T>
    WorldObject* FindCorpseUsing();

    bool CheckTarget(Unit* target, SpellEffectIndex eff);

    // Uses SendMessageToSet, but forces caster to get packet if OOR of self
    void SendMessageToSet(WorldPacket&& data);
    static void MANGOS_DLL_SPEC SendCastResult(Player* caster,
        SpellEntry const* spellInfo, uint8 cast_count, SpellCastResult result);
    void SendCastResult(SpellCastResult result);
    void SendSpellStart();
    void SendSpellGo();
    void SendSpellCooldown();
    void SendLogExecute();
    void SendInterrupted(uint8 result);
    void SendChannelUpdate(uint32 time);
    void SendChannelStart(uint32 duration);
    void SendResurrectRequest(Player* target);
    void SendPlaySpellVisual(uint32 SpellID);

    void HandleEffects(Unit* pUnitTarget, Item* pItemTarget,
        GameObject* pGOTarget, SpellEffectIndex i);
    void HandleDurationDR(Unit* target = nullptr, bool isReflected = false,
        uint32 effect_mask = 0);
    void HandleThreatSpells();
    // void HandleAddAura(Unit* Target);

    SpellEntry const* m_spellInfo;
    SpellEntry const* m_triggeredBySpellInfo;
    int32 m_currentBasePoints
        [MAX_EFFECT_INDEX]; // cache SpellEntry::CalculateSimpleValue and use
                            // for set custom base points

    uint8 m_cast_count;
    SpellCastTargets m_targets;

    int32 GetCastTime() const { return m_casttime; }
    uint32 GetCastedTime() { return m_timer; }
    bool IsAutoRepeat() const { return m_autoRepeat; }
    void SetAutoRepeat(bool rep) { m_autoRepeat = rep; }
    void ReSetTimer() { m_timer = m_casttime > 0 ? m_casttime : 0; }
    bool IsNextMeleeSwingSpell() const
    {
        return m_spellInfo->HasAttribute(SPELL_ATTR_ON_NEXT_SWING_1) ||
               m_spellInfo->HasAttribute(SPELL_ATTR_ON_NEXT_SWING_2);
    }
    bool IsRangedSpell() const
    {
        return m_spellInfo->HasAttribute(SPELL_ATTR_RANGED);
    }
    bool IsChannelActive() const
    {
        return m_caster->GetUInt32Value(UNIT_CHANNEL_SPELL) != 0;
    }
    bool IsMeleeAttackResetSpell() const
    {
        return !trigger_type_.triggered() &&
               (m_spellInfo->InterruptFlags & SPELL_INTERRUPT_FLAG_AUTOATTACK);
    }

    bool IsDeletable() const
    {
        return !m_referencedFromCurrentSpell && !m_executedCurrently &&
               ref_counter_.empty();
    }
    void SetReferencedFromCurrent(bool yes)
    {
        m_referencedFromCurrentSpell = yes;
    }
    void SetExecutedCurrently(bool yes) { m_executedCurrently = yes; }
    uint64 GetDelayStart() const { return m_delayStart; }
    void SetDelayStart(uint64 m_time) { m_delayStart = m_time; }
    uint64 GetDelayMoment() const { return m_delayMoment; }
    void SetDelayMoment(uint64 delay) { m_delayMoment = delay; }

    bool IsNeedSendToClient() const; // use for hide spell cast for client in
                                     // case when cast not have client side
                                     // affect (animation or log entries)
    bool IsTriggeredSpellWithRedundentData() const; // use for ignore some spell
                                                    // data for triggered spells
                                                    // like cast time, some
                                                    // triggered spells have
                                                    // redundent copy data from
                                                    // main spell for client use
                                                    // purpose

    CurrentSpellTypes GetCurrentContainer();

    // caster types:
    // formal spell caster, in game source of spell affects cast
    Unit* GetCaster() const { return m_caster; }
    ObjectGuid GetCasterGUID() const { return m_casterGUID; }
    // real source of cast affects, explicit caster, or DoT/HoT applier, or GO
    // owner, or wild GO itself. Can be NULL
    WorldObject* GetAffectiveCasterObject() const;
    // limited version returning NULL in cases wild gameobject caster object,
    // need for Aura (auras currently not support non-Unit caster)
    Unit* GetAffectiveCaster() const
    {
        return m_originalCasterGUID ? m_originalCaster : m_caster;
    }
    // m_originalCasterGUID can store GO guid, and in this case this is visual
    // caster
    WorldObject* GetCastingObject() const;

    uint32 GetPowerCost() const { return m_powerCost; }

    void UpdatePointers(); // must be used at call Spell code after time delay
                           // (non triggered spell cast/update spell call/etc)

    bool CheckTargetCreatureType(Unit* target) const;

    void AddTriggeredSpell(SpellEntry const* spellInfo)
    {
        m_TriggerSpells.push_back(spellInfo);
    }
    void AddPrecastSpell(SpellEntry const* spellInfo)
    {
        m_preCastSpells.push_back(spellInfo);
    }
    void AddTriggeredSpell(uint32 spellId);
    void AddPrecastSpell(uint32 spellId);
    void CastPreCastSpells(Unit* target);
    void CastTriggerSpells();

    void CleanupTargetList();

    void set_cast_item(Item* target);
    Item* get_cast_item() const { return m_CastItem; }
    // clears reference in item, use set_cast_item(nullptr) to not do that
    void ClearCastItem();

    bool IsReflectable() const { return m_canReflect; }
    bool IsReflected() const { return m_reflected; }
    void SetGrounded() { m_wasGrounded = true; }

    bool GetSpreadingRadius(float maxRadius, float& moddedRadius) const;

    static void SelectMountByAreaAndSkill(Unit* target,
        SpellEntry const* parentSpell, uint32 spellId75, uint32 spellId150,
        uint32 spellId225, uint32 spellId300, uint32 spellIdSpecial);
    // Gets a buff that fits the requirements of Priest's Consume Magic
    static AuraHolder* consume_magic_buff(Unit* target);

    bool IsInstant() const { return m_instant; }

    void DoAllEffectOnTarget(Unit* target);
    void DoFinishPhase(); // Called after all units have processed their own
                          // queue, but before deletion
    MaNGOS::ref_counter& ref_counter() { return ref_counter_; }

    // HACK: Elemental mastery gets consumed on cast, we need to save the crit
    // chance increase for later
    bool ElementalMasteryUsed;
    // HACK: Divine Favor + spell queue would make it available to affect
    // already finished spells (see patch 1.7.0, 1.11.0 and 2.3.0)
    bool DivineFavorUsed;

    uint32 waiting_for_path;
    bool path_gen_finished;
    std::vector<G3D::Vector3> pregenerated_path;

    uint32 casted_timestamp() const { return casted_timestamp_; }

protected:
    bool IsWandAttack() const;
    bool HasGlobalCooldown();
    void TriggerGlobalCooldown();
    // Mangos has given CancelGlobalCooldown() more responsibility than it
    // should have
    // (i.e. it checks stuff it shouldn't), keepHandsOff == true disables this
    void CancelGlobalCooldown(bool keepHandsOff = false);

    void SendLoot(ObjectGuid guid, LootType loottype, LockType lockType);
    bool IgnoreItemRequirements()
        const; // some item use spells have unexpected reagent data
    void UpdateOriginalCasterPointer();

    Unit* m_caster;
    ObjectGuid m_casterGUID;

    ObjectGuid m_originalCasterGUID; // real source of cast (aura caster/etc),
                                     // used for spell targets selection
    // e.g. damage around area spell trigered by victim aura and da,age emeies
    // of aura caster
    Unit* m_originalCaster; // cached pointer for m_originalCaster, updated at
                            // Spell::UpdatePointers()

    Spell** m_selfContainer; // pointer to our spell container (if applicable)

    // Spell data
    SpellSchoolMask m_spellSchoolMask; // Spell school (can be overwrite for
                                       // some spells (wand shoot for example)
    WeaponAttackType m_attackType;     // For weapon based attack
    uint32 m_powerCost; // Calculated spell cost     initialized only in
                        // Spell::prepare
    Item* m_CastItem;
    int32 m_casttime;      // Calculated spell cast time initialized only in
                           // Spell::prepare
    int32 m_durationUnmod; // This duration is not modified, and contains the
                           // original duration calculation
    int32 m_durationMax; // m_duration cannot exceed this, if it does it will be
                         // lowered, this can be equal to or less than
                         // m_durationUnmod
    int32 m_duration;  // This duration is modified, and is used for the current
                       // target we're processing
    bool m_canReflect; // can reflect this spell?
    bool m_reflected;
    bool m_autoRepeat;
    bool m_stealthedOnCast; // True if caster had a SPELL_AURA_MOD_STEALTH aura
                            // on cast
    bool
        m_scriptTarget; // Currently selected pool of targets are script targets

    uint8 m_delayAtDamageCount;
    int32 GetNextDelayAtDamageMsTime()
    {
        return m_delayAtDamageCount < 5 ?
                   1000 - (m_delayAtDamageCount++) * 200 :
                   200;
    }

    // Delayed spells system
    uint64 m_delayStart;  // time of spell delay start, filled by event handler,
                          // zero = just started
    uint64 m_delayMoment; // moment of next delay call, used internally
    bool m_immediateHandled; // were immediate actions handled? (used by delayed
                             // spells only)

    // These vars are used in both delayed spell system and modified immediate
    // spell system
    bool m_referencedFromCurrentSpell; // mark as references to prevent deleted
                                       // and access by dead pointers
    bool m_executedCurrently; // mark as executed to prevent deleted and access
                              // by dead pointers
    bool m_needSpellLog;      // need to send spell log?
    uint8 m_applyMultiplierMask; // marks which effects are multiplied;
                                 // coefficient exists in TargetInfo

    // Current targets, to be used in SpellEffects (MUST BE USED ONLY IN SPELL
    // EFFECTS)
    Unit* unitTarget;
    Item* itemTarget;
    GameObject* gameObjTarget;
    AuraHolder* m_spellAuraHolder; // spell aura holder for current target,
                                   // created only if spell has aura applying
                                   // effect
    int32 damage;

    // this is set in Spell Hit, but used in Apply Aura handler
    DiminishingLevels m_diminishLevel;
    DiminishingGroup m_diminishGroup;

    // -------------------------------------------
    GameObject* focusObject;

    // Damage and healing in effects need just calculate
    int32 m_damage;                 // Damage   in effects count here
    int32 m_healing;                // Healing in effects count here
    float health_leech_multiplier_; // Percent of damage returned as health

    //******************************************
    // Spell trigger system
    //******************************************
    bool m_canTrigger; // Can start trigger (trigger_type_.triggered() can`t use
                       // for this)
    uint8 m_negativeEffectMask; // Use for avoid sent negative spell procs for
                                // additional positive effects only targets
    uint32 m_procAttacker;      // Attacker trigger flags
    uint32 m_procVictim;        // Victim   trigger flags
    void prepareDataForTriggerSystem();

    //*****************************************
    // Spell target subsystem
    //*****************************************
    // Targets store structures and data
    struct TargetInfo
    {
        ObjectGuid targetGUID;
        uint64 timeDelay;
        uint32 HitInfo;
        uint32 damage;
        float resist;               // -1 if not applicalbe, otherwise [0,1]
        float damageMultipliers[3]; // by effect: damage multiplier
        SpellMissInfo missCondition : 8;
        SpellMissInfo reflectResult : 8;
        uint8 effectMask : 8;
        bool processed : 1;
        bool friendly : 1;
        bool vanished_on_cast : 1;
    };
    uint8 m_needAliveTargetMask; // Mask req. alive targets

    struct GOTargetInfo
    {
        ObjectGuid targetGUID;
        uint64 timeDelay;
        uint8 effectMask : 8;
        bool processed : 1;
    };

    struct ItemTargetInfo
    {
        Item* item;
        uint8 effectMask;
    };

    typedef std::list<TargetInfo> TargetList;
    typedef std::list<GOTargetInfo> GOTargetList;
    typedef std::list<ItemTargetInfo> ItemTargetList;

    TargetList m_UniqueTargetInfo;
    GOTargetList m_UniqueGOTargetInfo;
    ItemTargetList m_UniqueItemInfo;

    void AddUnitTarget(Unit* target, SpellEffectIndex effIndex);
    void AddUnitTarget(ObjectGuid unitGuid, SpellEffectIndex effIndex);
    void AddGOTarget(GameObject* target, SpellEffectIndex effIndex);
    void AddGOTarget(ObjectGuid goGuid, SpellEffectIndex effIndex);
    void AddItemTarget(Item* target, SpellEffectIndex effIndex);

    void DoUnitEffect(TargetInfo* target);
    void DoUnitEffects();
    void DoMissThreat(Unit* target, Unit* real_caster);
    void DoImmediateEffectsOnTarget(TargetInfo* target);
    void DoAllEffectOnTarget(TargetInfo* target);
    void HandleDelayedSpellLaunch(TargetInfo* target);
    void InitializeDamageMultipliers();
    void ResetEffectDamageAndHeal();
    void DoSpellHitOnUnit(Unit* unit, TargetInfo* targetInfo, uint32 effectMask,
        bool isReflected = false);
    void DoAllEffectOnTarget(GOTargetInfo* target);
    void DoAllEffectOnTarget(ItemTargetInfo* target);
    bool IsAliveUnitPresentInTargetList();
    SpellCastResult CanOpenLock(SpellEffectIndex effIndex, uint32 lockid,
        SkillType& skillid, int32& reqSkillValue, int32& skillValue);

    void DropComboPointsIfNeeded(bool finish_phase = true);

    bool DoImmunePowerException(TargetInfo* target);

    // -------------------------------------------

    // List For Triggered Spells
    typedef std::list<SpellEntry const*> SpellInfoList;
    SpellInfoList m_TriggerSpells; // casted by caster to same targets settings
                                   // in m_targets at success finish of current
                                   // spell
    SpellInfoList m_preCastSpells; // casted by caster to each target at spell
                                   // hit before spell effects apply

    uint32 m_spellState;
    uint32 m_timer;

    float m_castPositionX;
    float m_castPositionY;
    float m_castPositionZ;
    float m_castOrientation;
    spell_trigger_type trigger_type_;

    // if need this can be replaced by Aura copy
    // we can't store original aura link to prevent access to deleted auras
    // and in same time need aura data and after aura deleting.
    SpellEntry const* m_triggeredByAuraSpell;

    // For spells that return reagents, we need to make sure they actually took
    // some reagents:
    bool m_reagentsIgnoredDueToPrepare;

    bool m_wasGrounded;

    bool m_instant;

    bool ignore_interrupt_; // Forces "Interrupted" to not be sent if true, has
                            // no effect otherwise

    MaNGOS::ref_counter ref_counter_;
    bool finish_ok_;
    bool send_cast_result_to_pet_owner_;
    bool pet_cast_;
    ObjectGuid summoned_target_;

    uint32 casted_timestamp_;

private:
    // === Effect helper functions ===

    // Used by EffectLeapForward for mage's blink
    void EffectHelperBlink();

    // Used by EffectDispel and EffectStealBeneficialBuff
    void DispelHelper(SpellEffectIndex eff_idx, bool steal_buffs);
    void StealAura(AuraHolder* holder, Unit* target);
    void DispelAura(AuraHolder* holder, uint32 stacks, Unit* target);
};

enum ReplenishType
{
    REPLENISH_UNDEFINED = 0,
    REPLENISH_HEALTH = 20,
    REPLENISH_MANA = 21,
    REPLENISH_RAGE = 22
};

typedef void (Spell::*pEffect)(SpellEffectIndex eff_idx);

class SpellEvent : public BasicEvent
{
public:
    SpellEvent(Spell* spell);
    virtual ~SpellEvent();

    virtual bool Execute(uint64 e_time, uint32 p_time) override;
    virtual void Abort(uint64 e_time) override;
    virtual bool IsDeletable() const override;

protected:
    Spell* m_Spell;
};

#endif
