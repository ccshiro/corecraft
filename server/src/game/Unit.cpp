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

#include "Unit.h"
#include "BattleGround.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "CreatureGroupMgr.h"
#include "DBCStores.h"
#include "DynamicObject.h"
#include "FirstKills.h"
#include "Formulas.h"
#include "Group.h"
#include "InstanceData.h"
#include "logging.h"
#include "MapManager.h"
#include "MapPersistentStateMgr.h"
#include "movement/HomeMovementGenerator.h"
#include "movement/IdleMovementGenerator.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "Pet.h"
#include "Player.h"
#include "QuestDef.h"
#include "SpecialVisCreature.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "TemporarySummon.h"
#include "Totem.h"
#include "Transport.h"
#include "Util.h"
#include "VMapFactory.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "buff_stacking.h"
#include "loot_distributor.h"
#include "pet_behavior.h"
#include "pet_template.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "maps/checks.h"
#include "maps/visitors.h"
#include "movement/MoveSpline.h"
#include "movement/MoveSplineInit.h"
#include <math.h>
#include <stdarg.h>

float baseMoveSpeed[MAX_MOVE_TYPE] = {
    2.5f,      // MOVE_WALK
    7.0f,      // MOVE_RUN
    4.5f,      // MOVE_RUN_BACK
    4.722222f, // MOVE_SWIM
    2.5f,      // MOVE_SWIM_BACK
    3.141594f, // MOVE_TURN_RATE
    7.0f,      // MOVE_FLIGHT
    4.5f,      // MOVE_FLIGHT_BACK
};

////////////////////////////////////////////////////////////
// Methods of class GlobalCooldownMgr

bool GlobalCooldownMgr::HasGlobalCooldown(SpellEntry const* spellInfo) const
{
    auto itr = m_GlobalCooldowns.find(spellInfo->StartRecoveryCategory);
    return itr != m_GlobalCooldowns.end() && itr->second.duration &&
           WorldTimer::getMSTimeDiff(itr->second.cast_time,
               WorldTimer::getMSTime()) < itr->second.duration;
}

void GlobalCooldownMgr::AddGlobalCooldown(
    SpellEntry const* spellInfo, uint32 gcd)
{
    m_GlobalCooldowns[spellInfo->StartRecoveryCategory] =
        GlobalCooldown(gcd, WorldTimer::getMSTime());
}

void GlobalCooldownMgr::CancelGlobalCooldown(SpellEntry const* spellInfo)
{
    m_GlobalCooldowns[spellInfo->StartRecoveryCategory].duration = 0;
}

proc_amount::proc_amount(bool damage, uint32 total, Unit* target, uint32 absorb)
  : damage_(damage), forced_(false), total_(total), over_(0), absorb_(absorb)
{
    if (damage_)
    {
        if (total_ > target->GetHealth())
            over_ = total_ - target->GetHealth();
    }
    else
    {
        uint32 diff = target->GetMaxHealth() - target->GetHealth();
        if (total_ > diff)
            over_ = total_ - diff;
    }

    // add absorb to total after over has been calculated
    if (damage_)
        total_ += absorb_;
}

////////////////////////////////////////////////////////////
// Methods of class Unit

Unit::Unit()
  : movespline{new movement::MoveSpline()}, movement_gens{this},
    guarding_holders_{0}, m_charmInfo{nullptr}, m_ThreatManager{this},
    m_HostileRefManager{this}
{
    m_objectType |= TYPEMASK_UNIT;
    m_objectTypeId = TYPEID_UNIT;
    // 2.3.2 - 0x70
    m_updateFlag =
        (UPDATEFLAG_HIGHGUID | UPDATEFLAG_LIVING | UPDATEFLAG_HAS_POSITION);

    m_attackTimer[BASE_ATTACK] = 0;
    m_attackTimer[OFF_ATTACK] = 0;
    m_attackTimer[RANGED_ATTACK] = 0;
    m_modAttackSpeedPct[BASE_ATTACK] = 1.0f;
    m_modAttackSpeedPct[OFF_ATTACK] = 1.0f;
    m_modAttackSpeedPct[RANGED_ATTACK] = 1.0f;

    m_state = 0;
    m_deathState = ALIVE;

    for (auto& elem : m_currentSpells)
        elem = nullptr;

    m_castCounter = 0;

    // m_Aura = NULL;
    // m_AurasCheck = 2000;
    // m_removeAuraTimer = 4;
    m_AuraFlags = 0;

    m_Visibility = VISIBILITY_ON;
    m_AINotifyScheduled = false;

    m_detectInvisibilityMask = 0;
    m_invisibilityMask = 0;
    m_transform = 0;
    m_canModifyStats = false;

    for (auto& elem : m_spellImmune)
        elem.clear();
    for (auto& elem : m_auraModifiersGroup)
    {
        elem[BASE_VALUE] = 0.0f;
        elem[BASE_PCT] = 1.0f;
        elem[TOTAL_VALUE] = 0.0f;
        elem[TOTAL_PCT] = 1.0f;
    }
    for (auto& elem : ap_buffs_)
        elem = 0;

    // implement 50% base damage from offhand
    m_auraModifiersGroup[UNIT_MOD_DAMAGE_OFFHAND][TOTAL_PCT] = 0.5f;

    for (auto& elem : m_weaponDamage)
    {
        elem[MINDAMAGE] = BASE_MINDAMAGE;
        elem[MAXDAMAGE] = BASE_MAXDAMAGE;
    }
    for (auto& elem : m_createStats)
        elem = 0.0f;

    m_attacking = nullptr;
    m_attackingGuid.Clear();
    m_modMeleeHitChance = 0.0f;
    m_modRangedHitChance = 0.0f;
    m_modSpellHitChance = 0.0f;
    m_baseSpellCritChance = 5;

    _min_combat_timer.SetInterval(0);

    m_lastManaUseTimer = 0;

    // m_victimThreat = 0.0f;
    for (auto& elem : m_threatModifier)
        elem = 1.0f;
    m_isSorted = true;
    for (auto& elem : m_speed_rate)
        elem = 1.0f;

    // remove aurastates allowing special moves
    for (auto& elem : m_reactiveTimer)
        elem = 0;

    m_confuseCount = 0;
    m_fleeCount = 0;

    attackerStateLock = false;

    m_diminishingTimer.SetInterval(5000);
    stealth_update_timer_.SetInterval(STEALTH_UPDATE_TIME);

    m_AutoRepeatFirstCast = true;

    m_polyRegenTimer = -1;

    interrupt_mask_ = 0;
}

Unit::~Unit()
{
    // set current spells as deletable
    for (auto& elem : m_currentSpells)
    {
        if (elem)
        {
            elem->SetReferencedFromCurrent(false);
            elem = nullptr;
        }
    }

    // Delete any remaining AuraHolder
    for (auto& pair : m_auraHolders)
        delete pair.second;

    // Delete any still queued aura holders
    for (auto& holder : m_queuedHolderAdds)
        delete holder;

    delete m_charmInfo;
    delete movespline;

    // those should be already removed at "RemoveFromWorld()" call
    assert(m_gameObj.size() == 0);
    assert(m_dynObjGUIDs.size() == 0);
}

void Unit::Update(uint32 update_diff, uint32 p_time)
{
    if (!IsInWorld())
        return;

    // WARNING! Order of execution here is important, do not change.
    // Spells must be processed with event system BEFORE they go to
    // _UpdateSpells.
    // Or else we may have some SPELL_STATE_FINISHED spells stalled in pointers,
    // that is bad.
    m_Events.Update(update_diff);
    _UpdateSpells(update_diff);

    if (m_lastManaUseTimer)
    {
        if (update_diff >= m_lastManaUseTimer)
            m_lastManaUseTimer = 0;
        else
            m_lastManaUseTimer -= update_diff;
    }

    // Update attack timer of main-hand and off-hand (ranged is player only and
    // is updated in Player::Update)
    if (uint32 base_att = getAttackTimer(BASE_ATTACK))
    {
        setAttackTimer(BASE_ATTACK,
            (update_diff >= base_att ? 0 : base_att - update_diff));
    }
    if (uint32 off_att = getAttackTimer(OFF_ATTACK))
    {
        setAttackTimer(
            OFF_ATTACK, (update_diff >= off_att ? 0 : off_att - update_diff));
    }

    // update abilities available only for fraction of time
    UpdateReactives(update_diff);

    m_diminishingTimer.Update(update_diff);
    if (m_diminishingTimer.Passed())
    {
        UpdateDiminishing();
        m_diminishingTimer.Reset();
    }

    // Update mage's polymorph regen
    if (m_polyRegenTimer != -1)
    {
        if (m_polyRegenTimer <= (int32)update_diff)
        {
            SetHealth(GetHealth() + GetMaxHealth() * REGEN_POLY_PCT);
            m_polyRegenTimer = REGEN_POLY_TIME;
        }
        else
            m_polyRegenTimer -= (int32)update_diff;
    }

    ModifyAuraState(AURA_STATE_HEALTHLESS_20_PERCENT,
        isAlive() && GetHealth() < GetMaxHealth() * 0.20f);
    ModifyAuraState(AURA_STATE_HEALTHLESS_35_PERCENT,
        isAlive() && GetHealth() < GetMaxHealth() * 0.35f);
    UpdateSplineMovement(p_time);
    movement_gens.update(p_time);

    // Update stealth
    stealth_update_timer_.Update(update_diff);
    if (stealth_update_timer_.Passed() &&
        (HasAuraType(SPELL_AURA_MOD_STEALTH) || !stealth_detected_by_.empty()))
    {
        update_stealth();
        stealth_update_timer_.Reset();
    }

    // Update spell queue
    if (World::batch_ready[(int)BatchUpdates::spells])
        update_spell_queue();
}

bool Unit::UpdateMeleeAttackingState()
{
    Unit* victim = getVictim();
    if (!victim || IsCastedSpellPreventingMovementOrAttack())
        return false;

    Player* player = (GetTypeId() == TYPEID_PLAYER ? (Player*)this : nullptr);
    auto has_oh = haveOffhandWeapon();
    bool ready = getAttackTimer(BASE_ATTACK) == 0 ||
                 (has_oh && getAttackTimer(OFF_ATTACK) == 0);

    if (!ready && (!player || player->LastSwingErrorMsg() == 0))
        return false;

    int swing_error = 0;
    if (!CanReachWithMeleeAttack(victim))
    {
        swing_error = 1;
    }
    // 120 degrees of radiant range
    else if (movement_gens.top_id() != movement::gen::chase &&
             !HasInArc(2 * M_PI_F / 3, victim))
    {
        swing_error = 2;
    }
    else if (ready)
    {
        if (getAttackTimer(BASE_ATTACK) == 0)
        {
            // prevent base and off attack in same time, delay attack at 0.2 sec
            if (has_oh && getAttackTimer(OFF_ATTACK) < ATTACK_DISPLAY_DELAY)
                setAttackTimer(OFF_ATTACK, ATTACK_DISPLAY_DELAY);

            // Build data for a main hand swing
            WhiteAttack mhAttack;
            mhAttack.weaponAttackType = BASE_ATTACK;
            mhAttack.extraAttackType = EXTRA_ATTACK_NONE;
            mhAttack.onlyTriggerOnNormalSwing = true;

            // Queue it, toggle the lock and do the update
            QueueWhiteAttack(mhAttack);
            attackerStateLock = true;
            AttackerStateUpdate(victim, true);

            // Release the lock and reset the timer
            attackerStateLock = false;
            resetAttackTimer(BASE_ATTACK);
        }
        if (has_oh && getAttackTimer(OFF_ATTACK) == 0)
        {
            // prevent base and off attack in same time, delay attack at 0.2 sec
            if (getAttackTimer(BASE_ATTACK) < ATTACK_DISPLAY_DELAY)
                setAttackTimer(BASE_ATTACK, ATTACK_DISPLAY_DELAY);

            // Build data for an off hand swing
            WhiteAttack ohAttack;
            ohAttack.weaponAttackType = OFF_ATTACK;
            ohAttack.extraAttackType = EXTRA_ATTACK_NONE;
            ohAttack.onlyTriggerOnNormalSwing = true;

            // Queue it, toggle the lock and do the update
            QueueWhiteAttack(ohAttack);
            attackerStateLock = true;
            AttackerStateUpdate(victim, true);

            // Release the lock and reset the timer
            attackerStateLock = false;
            resetAttackTimer(OFF_ATTACK);
        }
    }

    if (player && swing_error != 0)
    {
        if (getAttackTimer(BASE_ATTACK) < 500)
            setAttackTimer(BASE_ATTACK, 500);
        if (has_oh && getAttackTimer(OFF_ATTACK) < 500)
            setAttackTimer(OFF_ATTACK, 500);
    }

    if (player && swing_error != player->LastSwingErrorMsg())
    {
        if (swing_error == 1)
            player->SendAttackSwingNotInRange();
        else if (swing_error == 2)
            player->SendAttackSwingBadFacingAttack();
        player->SwingErrorMsg(swing_error);
    }

    if (!ready)
        return false;

    return swing_error == 0;
}

bool Unit::CanUseEquippedWeapon(WeaponAttackType attackType) const
{
    ShapeshiftForm form = GetShapeshiftForm();

    // Druids cannot use weapons in cat or bear form (but can in moonkin and
    // tree form)
    if (form == FORM_CAT || form == FORM_BEAR || form == FORM_DIREBEAR)
        return false;

    // Shamans cannot use weapon in ghost wolf
    if (form == FORM_GHOSTWOLF)
        return false;

    switch (attackType)
    {
    default:
    case BASE_ATTACK:
        return !HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_DISARMED);
    case OFF_ATTACK:
    case RANGED_ATTACK:
        return true;
    }
}

bool Unit::haveOffhandWeapon() const
{
    if (!CanUseEquippedWeapon(OFF_ATTACK))
        return false;

    if (GetTypeId() == TYPEID_PLAYER)
        return ((Player*)this)->GetWeaponForAttack(OFF_ATTACK, true, true);
    else
    {
        uint8 itemClass = GetByteValue(UNIT_VIRTUAL_ITEM_INFO + (1 * 2) + 0,
            VIRTUAL_ITEM_INFO_0_OFFSET_CLASS);
        if (itemClass == ITEM_CLASS_WEAPON ||
            static_cast<const Creature*>(this)->GetCreatureInfo()->flags_extra &
                CREATURE_FLAG_USE_DUAL_WIELD)
            return true;

        return false;
    }
}

void Unit::SendHeartBeat()
{
    m_movementInfo.time = WorldTimer::getMSTime();
    WorldPacket data(MSG_MOVE_HEARTBEAT, 64);
    data << GetPackGUID();
    data << m_movementInfo;
    SendMessageToSet(&data, true);
}

void Unit::resetAttackTimer(WeaponAttackType type)
{
    m_attackTimer[type] =
        uint32(GetAttackTime(type) * m_modAttackSpeedPct[type]);
}

float Unit::GetMeleeReach(const Unit* victim, float flat_mod) const
{
    float range;

    // For player vs player the distance seems to be exactly 5 yards
    if (victim->GetTypeId() == TYPEID_PLAYER && GetTypeId() == TYPEID_PLAYER)
    {
        range = 5.0f + flat_mod;
    }
    else
    {
        // The measured values show BASE_MELEE_OFFSET in (1.3224, 1.342)
        range = GetFloatValue(UNIT_FIELD_COMBATREACH) +
                victim->GetFloatValue(UNIT_FIELD_COMBATREACH) +
                BASE_MELEERANGE_OFFSET + flat_mod;
    }

    // NOTE: Combat reach can actually be less than ATTACK_DISTANCE
    //       Confirmed on retail (WoD).

    // The client (& server) adds +2.65 range if both targets are moving forward
    // or sideways
    // This also stops applying i target has less than ~71.4% of default run
    // speed (the breakpoint is 5 yards per second)
    static const auto flags =
        MOVEFLAG_FORWARD | MOVEFLAG_STRAFE_LEFT | MOVEFLAG_STRAFE_RIGHT;
    bool me_moving = GetTypeId() == TYPEID_PLAYER ?
                         m_movementInfo.HasMovementFlag(flags) :
                         is_moving(true);
    bool target_moving = victim->GetTypeId() == TYPEID_PLAYER ?
                             victim->m_movementInfo.HasMovementFlag(flags) :
                             victim->is_moving(true);
    if (me_moving && target_moving && GetSpeed(MOVE_RUN) >= 5.0f &&
        victim->GetSpeed(MOVE_RUN) >= 5.0f)
        range += 2.65f;

    return range;
}

bool Unit::CanReachWithMeleeAttack(
    const Unit* pVictim, float flat_mod /*= 0.0f*/) const
{
    assert(pVictim);

    float reach = GetMeleeReach(pVictim, flat_mod);

    // This check is not related to bounding radius
    float dx = GetX() - pVictim->GetX();
    float dy = GetY() - pVictim->GetY();
    float dz = GetZ() - pVictim->GetZ();

    return dx * dx + dy * dy + dz * dz < reach * reach;
}

/* Called by DealDamage for auras that have a chance to be dispelled on damage
 * taken. */
// NOTE: This is actually not correct. Fears & Roots that have a chance on
// breaking have procflags
// that specify they should proc & that proc would cause breakage. But that's a
// lot of hazzle to do,
// so we simply make sure we only remove those with such proc flags
void Unit::RemoveSpellbyDamageTaken(
    AuraType auraType, uint32 damage, bool dot_damage)
{
    if (!HasAuraType(auraType))
        return;

    const Auras& auras = GetAurasByType(auraType);
    std::vector<AuraHolder*> holders;
    holders.reserve(auras.size());
    for (const auto& aura : auras)
    {
        if ((aura)->GetSpellProto() &&
            ((aura)->GetSpellProto()->procFlags & PROC_FLAG_TAKEN_MELEE_HIT ||
                (aura)->GetSpellProto()->procFlags &
                    PROC_FLAG_TAKEN_MELEE_SPELL_HIT ||
                (aura)->GetSpellProto()->procFlags &
                    PROC_FLAG_TAKEN_RANGED_SPELL_HIT ||
                (aura)->GetSpellProto()->procFlags &
                    PROC_FLAG_TAKEN_AOE_SPELL_HIT ||
                (aura)->GetSpellProto()->procFlags &
                    PROC_FLAG_TAKEN_NEGATIVE_SPELL_HIT ||
                (aura)->GetSpellProto()->procFlags &
                    PROC_FLAG_ON_TAKE_PERIODIC))
        {
            // Insert for deletion
            holders.push_back((aura)->GetHolder());
        }
    }

    if (holders.empty())
        return;

    // The chance to dispel an aura depends on the damage taken with respect to
    // the casters level.
    uint32 max_dmg = getLevel() > 8 ? 25 * getLevel() - 150 : 50;

    // Fear has certain modifiers to it that affects damage breakage.
    if (auraType == SPELL_AURA_MOD_FEAR)
    {
        // Patch 1.11: "[..], the chance for a damage over time spell to break
        // Fear is now significantly lower."
        if (dot_damage)
            max_dmg *= 2;
        // Patch 1.11: "Note that Fear continues to be roughly three times as
        // likely to break on player targets as on non-player targets."
        if (GetTypeId() == TYPEID_UNIT)
            max_dmg *= 3;
    }

    float chance = float(damage) / max_dmg * 100.0f;

    if (roll_chance_f(chance))
        for (auto& holder : holders)
            RemoveAuraHolder(holder, AURA_REMOVE_BY_DEFAULT);
}

void Unit::DealDamageMods(Unit* pVictim, uint32& damage, uint32* absorb)
{
    if (!pVictim->isAlive() || pVictim->IsTaxiFlying() ||
        (pVictim->GetTypeId() == TYPEID_UNIT &&
            ((Creature*)pVictim)->IsInEvadeMode()))
    {
        if (absorb)
            *absorb += damage;
        damage = 0;
        return;
    }

    // You don't lose health from damage taken from another player while in a
    // sanctuary You still see it in the combat log though
    if (!IsAllowedDamageInArea(pVictim))
    {
        if (absorb)
            *absorb += damage;
        damage = 0;
    }

    uint32 originalDamage = damage;

    // Script Event damage Deal
    if (GetTypeId() == TYPEID_UNIT && ((Creature*)this)->AI())
        ((Creature*)this)->AI()->DamageDeal(pVictim, damage);
    // Script Event damage taken
    if (pVictim->GetTypeId() == TYPEID_UNIT && ((Creature*)pVictim)->AI())
        ((Creature*)pVictim)->AI()->DamageTaken(this, damage);

    if (absorb && originalDamage > damage)
        *absorb += (originalDamage - damage);
}

uint32 Unit::DealDamage(Unit* pVictim, uint32 damage,
    CleanDamage const* cleanDamage, DamageEffectType damagetype,
    SpellSchoolMask damageSchoolMask, SpellEntry const* spellProto,
    bool durabilityLoss, bool absorb)
{
    bool canCauseAggro =
        !(spellProto && spellProto->HasAttribute(SPELL_ATTR_EX_NO_THREAT));
    if (pVictim != this && damagetype != DOT)
    {
        if (canCauseAggro)
        {
            SetInCombatWith(pVictim);
            pVictim->SetInCombatWith(this);

            if (Player* attackedPlayer =
                    pVictim->GetCharmerOrOwnerPlayerOrPlayerItself())
                SetContestedPvP(attackedPlayer);
        }
        else if (!(!pVictim->isInCombat() && spellProto &&
                     spellProto->HasAttribute(SPELL_ATTR_EX3_NO_INITIAL_AGGRO)))
            pVictim->SetInCombatState(true);
    }

    bool canCauseBreakage = true, canPushBack = true;
    if (spellProto &&
        spellProto->AttributesEx4 & SPELL_ATTR_EX4_NO_PUSHBACK_OR_CC_BREAKAGE)
    {
        canCauseBreakage = false;
        canPushBack = false;
    }

    // Rogue's Setup for dodged attacks
    if (cleanDamage && pVictim->GetTypeId() == TYPEID_PLAYER &&
        cleanDamage->hitOutCome == MELEE_HIT_DODGE &&
        pVictim->getClass() == CLASS_ROGUE)
        ((Player*)pVictim)->HandleRogueSetupTalent(this);

    if (!damage && !absorb)
        return 0;

    // Victim Invis breaks from any damage or from absorbed damage
    if (canCauseBreakage)
    {
        pVictim->remove_auras_on_event(
            AURA_INTERRUPT_FLAG_DAMAGE, spellProto ? spellProto->Id : 0);
    }

    // Stand up from anything that aren't dots
    if ((damagetype != DOT && pVictim->GetTypeId() == TYPEID_PLAYER &&
            !pVictim->IsStandState()) &&
        canCauseBreakage)
        pVictim->SetStandState(UNIT_STAND_STATE_STAND);

    if ((!spellProto ||
            !IsAuraAddedBySpell(SPELL_AURA_MOD_FEAR, spellProto->Id)) &&
        canCauseBreakage)
        pVictim->RemoveSpellbyDamageTaken(
            SPELL_AURA_MOD_FEAR, damage, damagetype == DOT);

    // root type spells do not dispel the root effect
    if ((!spellProto ||
            !(spellProto->Mechanic == MECHANIC_ROOT ||
                IsAuraAddedBySpell(SPELL_AURA_MOD_ROOT, spellProto->Id))) &&
        canCauseBreakage)
        pVictim->RemoveSpellbyDamageTaken(
            SPELL_AURA_MOD_ROOT, damage, damagetype == DOT);

    LOG_DEBUG(logging, "DealDamageStart");

    uint32 health = pVictim->GetHealth();
    LOG_DEBUG(logging, "deal dmg:%d to health:%d ", damage, health);

    // duel ends when player has 1 or less hp
    bool duel_hasEnded = false;
    if (pVictim->GetTypeId() == TYPEID_PLAYER && ((Player*)pVictim)->duel &&
        damage >= (health - 1))
    {
        // prevent kill only if killed in duel and killed by opponent or
        // opponent controlled creature
        if (((Player*)pVictim)->duel->opponent == this ||
            ((Player*)pVictim)->duel->opponent ==
                GetCharmerOrOwnerPlayerOrPlayerItself())
            damage = health - 1;
        // prevent spell damage that kills ourselves in a duel (e.g., shadow
        // word: pain, but don't do it for every damage type -- e.g. falling
        // damage)
        if (pVictim == this && spellProto)
            damage = health - 1;

        duel_hasEnded = true;
    }

    // Rage from Damage made (only from direct weapon damage)
    if (cleanDamage && damagetype == DIRECT_DAMAGE && this != pVictim &&
        GetTypeId() == TYPEID_PLAYER && (getPowerType() == POWER_RAGE))
    {
        switch (cleanDamage->attackType)
        {
        case BASE_ATTACK:
            ((Player*)this)
                ->RewardRage(
                    damage, cleanDamage->hitOutCome, BASE_ATTACK, true);
            break;
        case OFF_ATTACK:
            ((Player*)this)
                ->RewardRage(damage, cleanDamage->hitOutCome, OFF_ATTACK, true);
            break;
        case RANGED_ATTACK:
            break;
        }
    }

    if (GetTypeId() == TYPEID_PLAYER && this != pVictim && damage)
    {
        Player* killer = ((Player*)this);

        // in bg, count dmg if victim is also a player
        if (pVictim->GetTypeId() == TYPEID_PLAYER)
        {
            if (BattleGround* bg = killer->GetBattleGround())
            {
                // FIXME: kept by compatibility. don't know in BG if the
                // restriction apply.
                bg->UpdatePlayerScore(killer, SCORE_DAMAGE_DONE, damage);
            }
        }
    }

    // Creature tapping for loot is set and handled here
    Player* tapper =
        GetTypeId() == TYPEID_PLAYER ? static_cast<Player*>(this) : nullptr;

    uint32 dmg = damage > health ? health : damage;
    if (pVictim->GetTypeId() == TYPEID_UNIT &&
        !static_cast<Creature*>(pVictim)->IsPet())
    {
        Creature* c_v = static_cast<Creature*>(pVictim);
        if (tapper)
        {
            if (c_v->GetLootDistributor() && pVictim->isInCombat())
                c_v->GetLootDistributor()->recipient_mgr()->attempt_add_tap(
                    tapper);

            if (c_v->getLevel() > MaNGOS::XP::GetGrayLevel(tapper->getLevel()))
                c_v->legit_dmg_taken += dmg;
            c_v->player_dmg_taken += dmg;
        }
        else if (GetTypeId() == TYPEID_UNIT &&
                 static_cast<Creature*>(this)->IsPlayerPet())
        {
            if (c_v->getLevel() > MaNGOS::XP::GetGrayLevel(getLevel()))
                c_v->legit_dmg_taken += dmg;
            c_v->player_dmg_taken += dmg;
        }

        if (HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PVP) ||
            (c_v->GetLootDistributor() &&
                !c_v->GetLootDistributor()->recipient_mgr()->empty()))
            c_v->total_dmg_taken += dmg;
    }

    // Honor gain is based on damage done, cache the damage
    if (pVictim != this && pVictim->GetTypeId() == TYPEID_PLAYER &&
        !static_cast<Player*>(pVictim)->InArena())
        if (auto p = GetCharmerOrOwnerPlayerOrPlayerItself())
            static_cast<Player*>(pVictim)->honor_damage_taken(
                p, health <= damage ? health : damage);

    if (health <= damage)
    {
        LOG_DEBUG(logging, "DealDamage: victim just died");

        Kill(pVictim, durabilityLoss, spellProto);
    }
    else // if (health <= damage)
    {
        LOG_DEBUG(logging, "DealDamageAlive");

        if (damage)
            pVictim->ModifyHealth(-(int32)damage);

        if (damagetype != DOT && canCauseAggro)
        {
            if (!getVictim() &&
                (spellProto == nullptr ||
                    spellProto->HasAttribute(SPELL_ATTR_ABILITY)) &&
                !player_or_pet()) // Don't execute this code for non-NPCs
            {
                // if not have main target then attack state with target
                // (including AI call)
                // start melee attacks only after melee hit
                Attack(pVictim, (damagetype == DIRECT_DAMAGE));
            }

            // if damage pVictim call AI reaction
            pVictim->AttackedBy(this);
        }

        // polymorphed, hex and other negative transformed cases
        uint32 morphSpell = pVictim->getTransForm();
        if (morphSpell && !IsPositiveSpell(morphSpell) && damage &&
            canCauseBreakage)
        {
            if (SpellEntry const* morphEntry =
                    sSpellStore.LookupEntry(morphSpell))
            {
                if (IsSpellHaveAura(morphEntry, SPELL_AURA_MOD_CONFUSE))
                    pVictim->remove_auras(morphSpell);
            }
        }

        if ((damagetype == DIRECT_DAMAGE ||
                damagetype == SPELL_DIRECT_DAMAGE) &&
            canCauseBreakage)
        {
            // Okay this is all kinds of retarded, maim is the only spell with
            // this flag and they
            // do this retarded check to see that maim doesn't remove itself
            // instead of properly doing it
            // so it would work correctly when more auras have that flag
            if (!spellProto ||
                !(spellProto->AuraInterruptFlags &
                    AURA_INTERRUPT_FLAG_DIRECT_DAMAGE))
                pVictim->remove_auras_if([](AuraHolder* h)
                    {
                        return h->GetSpellProto()->AuraInterruptFlags &
                               AURA_INTERRUPT_FLAG_DIRECT_DAMAGE;
                    });
        }
        if (pVictim->GetTypeId() != TYPEID_PLAYER &&
            pVictim->CanHaveThreatList() && damage && canCauseAggro &&
            // For DOT threat we must be on target's threat-list already
            (damagetype != DOT || pVictim->getThreatManager().hasTarget(this)))
        {
            float threat =
                damage *
                sSpellMgr::Instance()->GetSpellThreatMultiplier(spellProto);
            pVictim->AddThreat(this, threat,
                (cleanDamage && cleanDamage->hitOutCome == MELEE_HIT_CRIT),
                damageSchoolMask, spellProto);
        }
        else if (pVictim->GetTypeId() == TYPEID_PLAYER &&
                 damage) // victim is a player
        {
            // Reward rage from taking damage
            bool isValidDamage =
                (damagetype == DIRECT_DAMAGE ||
                    damagetype == SPELL_DIRECT_DAMAGE || damagetype == DOT);
            if (isValidDamage && this != pVictim &&
                (pVictim->getPowerType() == POWER_RAGE))
                ((Player*)pVictim)->RewardRage(damage, 0, 0, false);

            // Chance to lose durability on equipment (HIT TAKEN)
            if (durabilityLoss &&
                rand_norm_f() <
                    sWorld::Instance()->getConfig(
                        CONFIG_FLOAT_RATE_GEAR_DURABILITY_LOSS_DAMAGE))
                static_cast<Player*>(pVictim)->rand_equip_dura();
        }

        // With direct damage or direct spell damage we have a chance to lose
        // durability on our weapons
        if (GetTypeId() == TYPEID_PLAYER && damage &&
            (damagetype == DIRECT_DAMAGE || damagetype == SPELL_DIRECT_DAMAGE))
        {
            // Chance to lose durability on damage or spell hit done (HIT DONE)
            if (durabilityLoss &&
                rand_norm_f() <
                    sWorld::Instance()->getConfig(
                        CONFIG_FLOAT_RATE_WEAP_DURABILITY_LOSS_DAMAGE))
            {
                if (spellProto &&
                    spellProto->EquippedItemClass ==
                        ITEM_CLASS_WEAPON) // Spells with a weapon req.
                                           // Including firing weapons (auto
                                           // shot for hunters, and throw/shoot
                                           // for others)
                {
                    uint32 mask = spellProto->EquippedItemSubClassMask;

                    if (mask & 1 << ITEM_SUBCLASS_WEAPON_BOW ||
                        mask & 1 << ITEM_SUBCLASS_WEAPON_GUN ||
                        mask & 1 << ITEM_SUBCLASS_WEAPON_THROWN ||
                        mask & 1 << ITEM_SUBCLASS_WEAPON_CROSSBOW ||
                        mask & 1 << ITEM_SUBCLASS_WEAPON_WAND)
                    {
                        // Attack was done using a ranged weapon
                        static_cast<Player*>(this)->weap_dura_loss(
                            RANGED_ATTACK);
                    }
                    else
                    {
                        // For other spells we just pick main-hand, this is
                        // probably not 100% correct; for example for Shiv &
                        // Mutilate,
                        // but shouldn't matter too much. And besides, we don't
                        // have a way to know which is which atm
                        static_cast<Player*>(this)->weap_dura_loss(BASE_ATTACK);
                    }
                }
                else if (cleanDamage) // For white attacks and melee swings
                {
                    switch (cleanDamage->attackType)
                    {
                    case BASE_ATTACK:
                        static_cast<Player*>(this)->weap_dura_loss(BASE_ATTACK);
                        break;
                    case OFF_ATTACK:
                        static_cast<Player*>(this)->weap_dura_loss(OFF_ATTACK);
                        break;
                    case RANGED_ATTACK:
                        static_cast<Player*>(this)->weap_dura_loss(
                            RANGED_ATTACK);
                        break; // NOTE: This can probably never trigger, as auto
                               // shot is a spell
                    }
                }
                else // Spells without a weapon requirement. (E.g., frostbolt or
                     // moonfire)
                {
                    static_cast<Player*>(this)->weap_dura_loss();
                }
            }
        }

        if (damagetype != NODAMAGE && pVictim->GetTypeId() == TYPEID_PLAYER)
        {
            bool cantInterrupt = damagetype == DOT || !canPushBack;
            // This is for all spells that do ranged channeled AOE with a
            // targetting circle (blizzard, volley, rain of fire, hurricane)
            if (spellProto)
                cantInterrupt = cantInterrupt ||
                                (spellProto->EffectImplicitTargetA[0] ==
                                        TARGET_DYNAMIC_OBJECT_COORDINATES &&
                                    spellProto->EffectImplicitTargetB[0] ==
                                        TARGET_ALL_ENEMY_IN_AREA_INSTANT) ||
                                (spellProto->EffectImplicitTargetA[1] ==
                                        TARGET_DYNAMIC_OBJECT_COORDINATES &&
                                    spellProto->EffectImplicitTargetB[1] ==
                                        TARGET_ALL_ENEMY_IN_AREA_INSTANT) ||
                                (spellProto->EffectImplicitTargetA[2] ==
                                        TARGET_DYNAMIC_OBJECT_COORDINATES &&
                                    spellProto->EffectImplicitTargetB[2] ==
                                        TARGET_ALL_ENEMY_IN_AREA_INSTANT);

            // TODO: Are normal spells and channeled spells really different in
            // regards to DOTs? leaving it like it is now but this needs some
            // investigation
            if (!cantInterrupt)
            {
                for (uint32 i = CURRENT_FIRST_NON_MELEE_SPELL;
                     i < CURRENT_MAX_SPELL; ++i)
                {
                    // skip channeled spell (processed differently below)
                    if (i == CURRENT_CHANNELED_SPELL)
                        continue;

                    if (Spell* spell =
                            pVictim->GetCurrentSpell(CurrentSpellTypes(i)))
                    {
                        if (spell->getState() == SPELL_STATE_PREPARING)
                        {
                            if (spell->m_spellInfo->InterruptFlags &
                                SPELL_INTERRUPT_FLAG_ABORT_ON_DMG)
                                pVictim->InterruptSpell(CurrentSpellTypes(i));
                            else if (damage)
                                spell->Delayed();
                        }
                    }
                }
            }

            Spell* spell = pVictim->m_currentSpells[CURRENT_CHANNELED_SPELL];
            if (spell && canPushBack)
            {
                if (spell->getState() == SPELL_STATE_CASTING)
                {
                    uint32 channelInterruptFlags =
                        spell->m_spellInfo->ChannelInterruptFlags;
                    if ((channelInterruptFlags & CHANNEL_FLAG_DELAY) &&
                        damagetype !=
                            DOT) // don't delay channeled spells with DoTs
                    {
                        if (pVictim != this && damage) // don't shorten the
                                                       // duration of channeling
                                                       // if you damage yourself
                            spell->DelayedChannel();
                    }
                    else if ((channelInterruptFlags &
                                 (CHANNEL_FLAG_DAMAGE | CHANNEL_FLAG_DAMAGE2)))
                    {
                        LOG_DEBUG(logging, "Spell %u canceled at damage!",
                            spell->m_spellInfo->Id);
                        pVictim->InterruptSpell(CURRENT_CHANNELED_SPELL);
                    }
                }
                else if (spell->getState() == SPELL_STATE_DELAYED)
                // break channeled spell in delayed state on damage
                {
                    LOG_DEBUG(logging, "Spell %u canceled at damage!",
                        spell->m_spellInfo->Id);
                    pVictim->InterruptSpell(CURRENT_CHANNELED_SPELL);
                }
            }
        }

        // last damage from duel opponent
        if (duel_hasEnded)
        {
            assert(pVictim->GetTypeId() == TYPEID_PLAYER);
            Player* he = (Player*)pVictim;

            assert(he->duel);

            he->SetHealth(1);

            he->CastSpell(he, 7267, true); // beg
            he->DuelComplete(DUEL_WON);
        }
    }

    LOG_DEBUG(logging, "DealDamageEnd returned %d damage", damage);

    return damage;
}

void Unit::Kill(Unit* victim, bool durabilityLoss, const SpellEntry* spellProto)
{
    // Prevent killing unit twice (and giving reward from kill twice)
    if (!victim->GetHealth())
        return;

    // find player: owner of controlled `this` or `this` itself maybe
    // for loot will be sued only if group_tap==NULL
    Player* player_tap = GetCharmerOrOwnerPlayerOrPlayerItself();
    Group* group_tap = nullptr;

    // find owner of pVictim, used for creature cases, AI calls
    Unit* pOwner = victim->GetCharmerOrOwner();

    // BeforeDeath event - to allow casting of spells
    if (victim->GetTypeId() == TYPEID_UNIT)
        if (((Creature*)victim)->AI())
            ((Creature*)victim)->AI()->BeforeDeath(this);

    // in creature kill case group/player tap stored for creature
    if (victim->GetTypeId() == TYPEID_UNIT)
    {
        Creature* c_v = static_cast<Creature*>(victim);
        if (c_v->GetLootDistributor())
        {
            group_tap = c_v->GetLootDistributor()->recipient_mgr()->group();

            if (Player* recipient = c_v->GetLootDistributor()
                                        ->recipient_mgr()
                                        ->first_valid_player())
                player_tap = recipient;
        }
    }
    // in player kill case group tap selected by player_tap (killer-player
    // itself, or charmer, or owner, etc)
    else
    {
        if (player_tap)
            group_tap = player_tap->GetGroup();
    }
    // If the kill happens in a battleground, the BG group should be the
    // group_tap
    if (GetMap()->IsBattleGroundOrArena() && player_tap)
    {
        if (BattleGround* bg = player_tap->GetBattleGround())
            group_tap = bg->GetBgRaid(player_tap->GetBGTeam());
    }

    // Players needs to do at least 50% of the damage to be tappers. Otherwise,
    // if an
    // NPC/Pet kills it, the player will get no loot or rewards.
    if (victim->GetTypeId() == TYPEID_UNIT &&
        !static_cast<Creature*>(victim)->IsPet() &&
        static_cast<Creature*>(victim)->total_dmg_taken / 2.0f >
            static_cast<Creature*>(victim)->player_dmg_taken)
    {
        if (auto ld = static_cast<Creature*>(victim)->GetLootDistributor())
            ld->recipient_mgr()->reset();
        player_tap = nullptr;
        group_tap = nullptr;
    }

    // Proc damage and spells for the KILLER (not the player tap)
    ProcDamageAndSpell(
        victim, PROC_FLAG_KILL, PROC_FLAG_KILLED, PROC_EX_NONE, proc_amount());
    // If killer is a pet owned by a player treat it as his kill too
    if (GetTypeId() == TYPEID_UNIT &&
        static_cast<Creature*>(this)->IsPlayerPet())
        if (auto owner = static_cast<Creature*>(this)->GetOwner())
            owner->ProcDamageAndSpell(victim, PROC_FLAG_KILL, PROC_FLAG_KILLED,
                PROC_EX_NONE, proc_amount());

    // Send packets about kill (if a player tap was found)
    if (player_tap)
    {
        WorldPacket data(SMSG_PARTYKILLLOG, (8 + 8)); // send event PARTY_KILL
        data << GetObjectGuid();                      // unit with killing blow
        data << victim->GetObjectGuid();              // victim

        if (group_tap)
        {
            group_tap->BroadcastPacket(&data, false,
                group_tap->GetMemberGroup(player_tap->GetObjectGuid()),
                player_tap->GetObjectGuid(), player_tap, 200.0f);
        }

        player_tap->SendDirectMessage(std::move(data));
    }

    // Reward a HK to anyone involved in killing this player
    if (victim->GetTypeId() == TYPEID_PLAYER)
        static_cast<Player*>(victim)->hk_distribute_honor();

    // Reward player, his pets, and group/raid members
    if (player_tap != victim)
    {
        if (group_tap)
            group_tap->RewardGroupAtKill(victim, player_tap);
        else if (player_tap)
            player_tap->RewardSinglePlayerAtKill(victim);

        // Award First Kill status if this was the first time this boss was
        // slayed
        // Note: since rank is only used for respawn time (instance bosses have
        // none) they've tackily
        // named it worldboss rather than just boss. But 3 is the rank set for
        // ALL bosses.
        if (group_tap && group_tap->isRaidGroup() &&
            victim->GetTypeId() == TYPEID_UNIT &&
            ((Creature*)victim)->GetCreatureInfo()->rank ==
                CREATURE_ELITE_WORLDBOSS)
            sFirstKills::Instance()->ProcessKilledBoss(victim, group_tap);
    }

    LOG_DEBUG(logging, "DealDamageAttackStop");

    // FIXME: This is a hack that's part of making Mind Control work properly in
    // PvE scenarios
    Unit* mc_caster;
    if (victim->GetTypeId() == TYPEID_UNIT &&
        victim->hasUnitState(UNIT_STAT_CONTROLLED) &&
        (mc_caster = victim->GetCharmer()) != nullptr)
    {
        // Transfer hostile references to caster
        for (auto& elem : victim->getHostileRefManager())
        {
            Unit* attacker;
            if ((attacker = elem.getSource()->getOwner()) != nullptr &&
                attacker->GetTypeId() == TYPEID_UNIT)
                static_cast<Creature*>(attacker)->AI()->AttackStart(mc_caster);
        }
    }

    // stop combat
    victim->CombatStop(false, false, true);
    victim->getHostileRefManager().deleteReferences();

    bool damageFromSpiritOfRedemtionTalent =
        spellProto && spellProto->Id == 27795;

    // if talent known but not triggered (check priest class for speedup check)
    Aura* spiritOfRedemtionTalentReady = nullptr;
    if (!damageFromSpiritOfRedemtionTalent && // not called from
                                              // SPELL_AURA_SPIRIT_OF_REDEMPTION
        victim->GetTypeId() == TYPEID_PLAYER &&
        victim->getClass() == CLASS_PRIEST)
    {
        const Auras& dummyAuras = victim->GetAurasByType(SPELL_AURA_DUMMY);
        for (const auto& dummyAura : dummyAuras)
        {
            if ((dummyAura)->GetSpellProto()->SpellIconID == 1654)
            {
                spiritOfRedemtionTalentReady = dummyAura;
                break;
            }
        }
    }

    if (!spiritOfRedemtionTalentReady)
    {
        LOG_DEBUG(logging, "SET JUST_DIED");
        victim->SetDeathState(JUST_DIED);
    }

    LOG_DEBUG(logging, "DealDamageHealth1");

    if (spiritOfRedemtionTalentReady)
    {
        // save value before aura remove
        uint32 ressSpellId = victim->GetUInt32Value(PLAYER_SELF_RES_SPELL);
        if (!ressSpellId)
            ressSpellId = ((Player*)victim)->GetResurrectionSpellId();

        victim->remove_auras_if(
            [victim](AuraHolder* h)
            {
                return !(h->IsPassive() && h->GetCaster() == victim &&
                           victim->player_or_pet()) &&
                       !h->IsDeathPersistent();
            },
            AURA_REMOVE_BY_DEATH);

        // restore for use at real death
        victim->SetUInt32Value(PLAYER_SELF_RES_SPELL, ressSpellId);

        // FORM_SPIRITOFREDEMPTION and related auras
        victim->CastSpell(
            victim, 27827, true, nullptr, spiritOfRedemtionTalentReady);
        victim->CastSpell(victim, 32343, true); // FIXME: Actually an NPC dummy
                                                // spell, but it has the same
                                                // visual as SoR should have
    }
    else
        victim->SetHealth(0);

    // remember victim PvP death for corpse type and corpse reclaim delay
    // at original death (not at SpiritOfRedemtionTalent timeout)
    if (victim->GetTypeId() == TYPEID_PLAYER &&
        !damageFromSpiritOfRedemtionTalent)
        ((Player*)victim)->SetPvPDeath(player_tap != nullptr);

    // Call KilledUnit for creatures
    if (GetTypeId() == TYPEID_UNIT && ((Creature*)this)->AI())
        ((Creature*)this)->AI()->KilledUnit(victim);

    // Call AI OwnerKilledUnit (for any current summoned
    // minipet/guardian/protector)
    PetOwnerKilledUnit(victim);

    // 10% durability loss on death
    // clean InHateListOf
    if (victim->GetTypeId() == TYPEID_PLAYER)
    {
        if (spellProto && spellProto->Id == 32409)
            durabilityLoss = false;
        // Durability loss cannot happen when killed by another player, or if
        // you die in any which way inside a battleground
        if (durabilityLoss && (!player_tap || player_tap == victim) &&
            !static_cast<Player*>(victim)->InBattleGround())
        {
            LOG_DEBUG(logging, "We are dead, loosing 10 percents durability");
            static_cast<Player*>(victim)->durability(true, -0.1, false);
            // durability lost message
            WorldPacket data(SMSG_DURABILITY_DAMAGE_DEATH, 0);
            static_cast<Player*>(victim)->GetSession()->send_packet(
                std::move(data));
        }
    }
    else // creature died
    {
        LOG_DEBUG(logging, "DealDamageNotPlayer");
        Creature* cVictim = (Creature*)victim;

        if (!cVictim->IsPet())
            cVictim->DeleteThreatList();
        if (!cVictim->IsPet() ||
            static_cast<Pet*>(cVictim)->get_template()->ctemplate_flags &
                PET_CLFAGS_ALLOW_LOOTING)
            cVictim->PrepareCorpseLoot();

        // Call creature just died function
        if (cVictim->AI())
            cVictim->AI()->JustDied(this);

        if (cVictim->IsTemporarySummon())
        {
            TemporarySummon* pSummon = (TemporarySummon*)cVictim;
            if (pSummon->GetSummonerGuid().IsCreature())
                if (Creature* pSummoner = cVictim->GetMap()->GetCreature(
                        pSummon->GetSummonerGuid()))
                    if (pSummoner->AI())
                        pSummoner->AI()->SummonedCreatureJustDied(cVictim);
        }
        else if (pOwner && pOwner->GetTypeId() == TYPEID_UNIT)
        {
            if (((Creature*)pOwner)->AI())
                ((Creature*)pOwner)->AI()->SummonedCreatureJustDied(cVictim);
        }

        if (InstanceData* mapInstance = cVictim->GetInstanceData())
            mapInstance->OnCreatureDeath(cVictim);

        if (cVictim->GetGroup() != nullptr)
            cVictim->GetMap()->GetCreatureGroupMgr().ProcessGroupEvent(
                cVictim->GetGroup()->GetId(), CREATURE_GROUP_EVENT_DEATH,
                cVictim);

        // Dungeon specific stuff, only applies to players killing creatures
        if (cVictim->GetInstanceId())
        {
            Map* m = cVictim->GetMap();
            Player* creditedPlayer = GetCharmerOrOwnerPlayerOrPlayerItself();
            // TODO: do instance binding anyway if the charmer/owner is offline

            if (m->IsDungeon() && creditedPlayer)
            {
                if (m->IsRaidOrHeroicDungeon())
                {
                    if (cVictim->GetCreatureInfo()->flags_extra &
                        CREATURE_FLAG_EXTRA_INSTANCE_BIND)
                        static_cast<DungeonMap*>(m)->PermBindAllPlayers(
                            cVictim);
                }
                else
                {
                    DungeonPersistentState* save =
                        ((DungeonMap*)m)->GetPersistanceState();
                    // the reset time is set but not added to the scheduler
                    // until the players leave the instance
                    time_t resettime = cVictim->GetRespawnTimeEx() + 2 * HOUR;
                    if (save && save->GetResetTime() < resettime)
                        save->SetResetTime(resettime);
                }
            }
        }

        // Curse of Doom: If curse of doom kills a target that yields experience
        // it has a chance to spawn a Doomguard (only for non-player NPCs)
        if (GetTypeId() == TYPEID_PLAYER && spellProto &&
            (spellProto->Id == 30910 || spellProto->Id == 603) &&
            !victim->GetCharmerOrOwnerGuid().IsPlayer() &&
            MaNGOS::XP::Gain(static_cast<Player*>(this), victim) > 0 &&
            roll_chance_i(
                25)) // TODO: Unknown chance, in WotLK it was made to be 100%
        {
            CastSpell(victim, 18662, true);
        }

        if (group_tap)
            group_tap->ClearTargetIcon(cVictim->GetObjectGuid());
    }

    // last damage from non duel opponent or opponent controlled creature
    if (victim->GetTypeId() == TYPEID_PLAYER && ((Player*)victim)->duel)
    {
        Player* he = (Player*)victim;

        assert(he->duel);
        he->DuelComplete(DUEL_INTERUPTED);
    }

    // battleground things (do this at the end, so the death state flag will be
    // properly set to handle in the bg->handlekill)
    if (victim->GetTypeId() == TYPEID_PLAYER && victim != player_tap)
    {
        Player* killed = ((Player*)victim);
        if (killed->InBattleGround())
        {
            if (BattleGround* bg = killed->GetBattleGround())
                if (player_tap)
                    bg->HandleKillPlayer(killed, player_tap);
        }
        else if (killed != this && player_tap)
        {
            // selfkills are not handled in outdoor pvp scripts
            if (OutdoorPvP* outdoorPvP =
                    sOutdoorPvPMgr::Instance()->GetScript(killed->GetZoneId()))
                outdoorPvP->HandlePlayerKill(player_tap, killed);
        }
    }
    else if (victim->GetTypeId() == TYPEID_UNIT)
    {
        if (player_tap)
            if (BattleGround* bg = player_tap->GetBattleGround())
                bg->HandleKillUnit((Creature*)victim, player_tap);
        // Notify the outdoor pvp script
        if (OutdoorPvP* outdoorPvP = sOutdoorPvPMgr::Instance()->GetScript(
                player_tap ? player_tap->GetZoneId() : GetZoneId()))
            outdoorPvP->HandleCreatureDeath(static_cast<Creature*>(victim));
    }
}

struct PetOwnerKilledUnitHelper
{
    explicit PetOwnerKilledUnitHelper(Unit* pVictim) : m_victim(pVictim) {}
    void operator()(Unit* pTarget) const
    {
        if (pTarget->GetTypeId() == TYPEID_UNIT)
        {
            if (((Creature*)pTarget)->AI())
                ((Creature*)pTarget)->AI()->OwnerKilledUnit(m_victim);
        }
    }

    Unit* m_victim;
};

void Unit::PetOwnerKilledUnit(Unit* pVictim)
{
    // for minipet and guardians (including protector)
    CallForAllControlledUnits(PetOwnerKilledUnitHelper(pVictim),
        CONTROLLED_MINIPET | CONTROLLED_GUARDIANS);
}

void Unit::CastStop(uint32 except_spellid)
{
    for (uint32 i = CURRENT_FIRST_NON_MELEE_SPELL; i < CURRENT_MAX_SPELL; ++i)
        if (m_currentSpells[i] &&
            m_currentSpells[i]->m_spellInfo->Id != except_spellid)
            InterruptSpell(CurrentSpellTypes(i), false);
}

void Unit::CastSpell(Unit* Victim, uint32 spellId, spell_trigger_type triggered,
    Item* castItem, Aura* triggeredByAura, ObjectGuid originalCaster,
    SpellEntry const* triggeredBy)
{
    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellId);

    if (!spellInfo)
    {
        if (triggeredByAura)
            logging.error(
                "CastSpell: unknown spell id %i by caster: %s triggered by "
                "aura %u (eff %u)",
                spellId, GetGuidStr().c_str(), triggeredByAura->GetId(),
                triggeredByAura->GetEffIndex());
        else
            logging.error("CastSpell: unknown spell id %i by caster: %s",
                spellId, GetGuidStr().c_str());
        return;
    }

    CastSpell(Victim, spellInfo, triggered, castItem, triggeredByAura,
        originalCaster, triggeredBy);
}

void Unit::CastSpell(Unit* Victim, SpellEntry const* spellInfo,
    spell_trigger_type triggered, Item* castItem, Aura* triggeredByAura,
    ObjectGuid originalCaster, SpellEntry const* triggeredBy)
{
    if (!spellInfo)
    {
        if (triggeredByAura)
            logging.error(
                "CastSpell: unknown spell by caster: %s triggered by aura %u "
                "(eff %u)",
                GetGuidStr().c_str(), triggeredByAura->GetId(),
                triggeredByAura->GetEffIndex());
        else
            logging.error(
                "CastSpell: unknown spell by caster: %s", GetGuidStr().c_str());
        return;
    }

    if (triggeredByAura)
    {
        if (!originalCaster)
            originalCaster = triggeredByAura->GetCasterGuid();

        triggeredBy = triggeredByAura->GetSpellProto();
    }

    auto spell =
        new Spell(this, spellInfo, triggered, originalCaster, triggeredBy);

    SpellCastTargets targets;
    targets.setUnitTarget(Victim);
    if (castItem)
        spell->set_cast_item(castItem);
    spell->prepare(&targets, triggeredByAura);
}

void Unit::CastCustomSpell(Unit* Victim, uint32 spellId, int32 const* bp0,
    int32 const* bp1, int32 const* bp2, spell_trigger_type triggered,
    Item* castItem, Aura* triggeredByAura, ObjectGuid originalCaster,
    SpellEntry const* triggeredBy)
{
    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellId);

    if (!spellInfo)
    {
        if (triggeredByAura)
            logging.error(
                "CastCustomSpell: unknown spell id %i by caster: %s triggered "
                "by aura %u (eff %u)",
                spellId, GetGuidStr().c_str(), triggeredByAura->GetId(),
                triggeredByAura->GetEffIndex());
        else
            logging.error("CastCustomSpell: unknown spell id %i by caster: %s",
                spellId, GetGuidStr().c_str());
        return;
    }

    CastCustomSpell(Victim, spellInfo, bp0, bp1, bp2, triggered, castItem,
        triggeredByAura, originalCaster, triggeredBy);
}

void Unit::CastCustomSpell(Unit* Victim, SpellEntry const* spellInfo,
    int32 const* bp0, int32 const* bp1, int32 const* bp2,
    spell_trigger_type triggered, Item* castItem, Aura* triggeredByAura,
    ObjectGuid originalCaster, SpellEntry const* triggeredBy)
{
    if (!spellInfo)
    {
        if (triggeredByAura)
            logging.error(
                "CastCustomSpell: unknown spell by caster: %s triggered by "
                "aura %u (eff %u)",
                GetGuidStr().c_str(), triggeredByAura->GetId(),
                triggeredByAura->GetEffIndex());
        else
            logging.error("CastCustomSpell: unknown spell by caster: %s",
                GetGuidStr().c_str());
        return;
    }

    if (triggeredByAura)
    {
        if (!originalCaster)
            originalCaster = triggeredByAura->GetCasterGuid();

        triggeredBy = triggeredByAura->GetSpellProto();
    }

    auto spell =
        new Spell(this, spellInfo, triggered, originalCaster, triggeredBy);

    if (bp0)
        spell->m_currentBasePoints[EFFECT_INDEX_0] = *bp0;

    if (bp1)
        spell->m_currentBasePoints[EFFECT_INDEX_1] = *bp1;

    if (bp2)
        spell->m_currentBasePoints[EFFECT_INDEX_2] = *bp2;

    SpellCastTargets targets;
    targets.setUnitTarget(Victim);
    if (castItem)
        spell->set_cast_item(castItem);
    spell->prepare(&targets, triggeredByAura);
}

bool Unit::IsCastingRitual()
{
    GameObjectList::iterator ite;
    for (ite = m_gameObj.begin(); ite != m_gameObj.end(); ++ite)
    {
        if ((*ite)->GetGOInfo()->type == GAMEOBJECT_TYPE_SUMMONING_RITUAL)
            return true;
    }
    return false;
}

GameObject* Unit::GetCastedRitual()
{
    GameObjectList::iterator itr;
    for (itr = m_gameObj.begin(); itr != m_gameObj.end(); ++itr)
    {
        if ((*itr)->GetGOInfo()->type == GAMEOBJECT_TYPE_SUMMONING_RITUAL)
            return *itr;
    }
    return nullptr;
}

// used for scripting
void Unit::CastSpell(float x, float y, float z, uint32 spellId,
    spell_trigger_type triggered, Item* castItem, Aura* triggeredByAura,
    ObjectGuid originalCaster, SpellEntry const* triggeredBy)
{
    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellId);

    if (!spellInfo)
    {
        if (triggeredByAura)
            logging.error(
                "CastSpell(x,y,z): unknown spell id %i by caster: %s triggered "
                "by aura %u (eff %u)",
                spellId, GetGuidStr().c_str(), triggeredByAura->GetId(),
                triggeredByAura->GetEffIndex());
        else
            logging.error("CastSpell(x,y,z): unknown spell id %i by caster: %s",
                spellId, GetGuidStr().c_str());
        return;
    }

    CastSpell(x, y, z, spellInfo, triggered, castItem, triggeredByAura,
        originalCaster, triggeredBy);
}

// used for scripting
void Unit::CastSpell(float x, float y, float z, SpellEntry const* spellInfo,
    spell_trigger_type triggered, Item* castItem, Aura* triggeredByAura,
    ObjectGuid originalCaster, SpellEntry const* triggeredBy)
{
    if (!spellInfo)
    {
        if (triggeredByAura)
            logging.error(
                "CastSpell(x,y,z): unknown spell by caster: %s triggered by "
                "aura %u (eff %u)",
                GetGuidStr().c_str(), triggeredByAura->GetId(),
                triggeredByAura->GetEffIndex());
        else
            logging.error("CastSpell(x,y,z): unknown spell by caster: %s",
                GetGuidStr().c_str());
        return;
    }

    if (triggeredByAura)
    {
        if (!originalCaster)
            originalCaster = triggeredByAura->GetCasterGuid();

        triggeredBy = triggeredByAura->GetSpellProto();
    }

    auto spell =
        new Spell(this, spellInfo, triggered, originalCaster, triggeredBy);

    SpellCastTargets targets;
    targets.setDestination(x, y, z);
    if (castItem)
        spell->set_cast_item(castItem);
    spell->prepare(&targets, triggeredByAura);
}

void Unit::CalculateSpellDamage(SpellNonMeleeDamage* damageInfo, int32 damage,
    SpellEntry const* spellInfo, WeaponAttackType attackType,
    uint32 TargetCount, float crit_mod, const Spell* spell)
{
    SpellSchoolMask damageSchoolMask = damageInfo->schoolMask;
    Unit* pVictim = damageInfo->target;

    if (damage < 0)
        return;

    if (!pVictim)
        return;
    if ((!this->isAlive() &&
            !spellInfo->HasAttribute(SPELL_ATTR_CUSTOM_CASTABLE_WHILE_DEAD)) ||
        !pVictim->isAlive())
        return;

    // Check spell crit chance
    bool crit = IsSpellCrit(
        pVictim, spellInfo, damageSchoolMask, attackType, crit_mod, spell);

    // damage bonus (per damage class)
    switch (spellInfo->DmgClass)
    {
    // Melee and Ranged Spells
    case SPELL_DAMAGE_CLASS_RANGED:
    case SPELL_DAMAGE_CLASS_MELEE:
    {
        // Calculate damage bonus
        damage = MeleeDamageBonusDone(
            pVictim, damage, attackType, spellInfo, SPELL_DIRECT_DAMAGE);
        damage = pVictim->MeleeDamageBonusTaken(
            this, damage, attackType, spellInfo, SPELL_DIRECT_DAMAGE);

        // if crit add critical bonus
        if (crit)
        {
            damageInfo->HitInfo |= SPELL_HIT_TYPE_CRIT;
            damage = SpellCriticalDamageBonus(spellInfo, damage, pVictim);
            // Resilience - reduce crit damage
            if (pVictim->GetTypeId() == TYPEID_PLAYER)
                damage -=
                    ((Player*)pVictim)->GetMeleeCritDamageReduction(damage);
        }
    }
    break;
    // Magical Attacks
    case SPELL_DAMAGE_CLASS_NONE:
    case SPELL_DAMAGE_CLASS_MAGIC:
    {
        // Calculate damage bonus
        damage = SpellDamageBonusDone(
            pVictim, spellInfo, damage, SPELL_DIRECT_DAMAGE);

        // Distribute aoe damage for damage capped spells
        if (uint32 cap = spellInfo->aoe_cap)
        {
            if ((damage * TargetCount) > cap)
                damage = cap / TargetCount;
        }

        damage = pVictim->SpellDamageBonusTaken(
            this, spellInfo, damage, SPELL_DIRECT_DAMAGE);

        // If crit add critical bonus
        if (crit)
        {
            damageInfo->HitInfo |= SPELL_HIT_TYPE_CRIT;
            damage = SpellCriticalDamageBonus(spellInfo, damage, pVictim);
            // Resilience - reduce crit damage
            if (pVictim->GetTypeId() == TYPEID_PLAYER)
                damage -=
                    ((Player*)pVictim)->GetSpellCritDamageReduction(damage);
        }
    }
    break;
    }

    // damage mitigation
    if (damage > 0)
    {
        // physical damage => armor
        if (damageSchoolMask & SPELL_SCHOOL_MASK_NORMAL &&
            !spellInfo->HasAttribute(SPELL_ATTR_CUSTOM_IGNORES_ARMOR))
            damage = calculate_armor_reduced_damage(pVictim, damage);
    }
    else
        damage = 0;
    damageInfo->damage = damage;
}

void Unit::DealSpellDamage(SpellNonMeleeDamage* damageInfo, bool durabilityLoss)
{
    if (!damageInfo)
        return;

    Unit* pVictim = damageInfo->target;

    if (!pVictim)
        return;

    if (!pVictim->isAlive() || pVictim->IsTaxiFlying() ||
        (pVictim->GetTypeId() == TYPEID_UNIT &&
            ((Creature*)pVictim)->IsInEvadeMode()))
        return;

    SpellEntry const* spellProto = sSpellStore.LookupEntry(damageInfo->SpellID);
    if (spellProto == nullptr)
    {
        logging.error(
            "Unit::DealSpellDamage have wrong damageInfo->SpellID: %u",
            damageInfo->SpellID);
        return;
    }

    // You don't lose health from damage taken from another player while in a
    // sanctuary
    // You still see it in the combat log though
    if (!IsAllowedDamageInArea(pVictim))
        return;

    // update at damage Judgement aura duration that applied by attacker at
    // victim
    if (damageInfo->damage && spellProto->Id == 35395)
    {
        loop_auras([this](AuraHolder* holder)
            {
                const SpellEntry* info = holder->GetSpellProto();
                if (info->AttributesEx3 & 0x40000 &&
                    info->SpellFamilyName == SPELLFAMILY_PALADIN &&
                    holder->GetCasterGuid() == GetObjectGuid())
                    holder->RefreshHolder();
                return true; // continue
            });
    }

    // Call default DealDamage (send critical in hit info for threat
    // calculation)
    CleanDamage cleanDamage(0, BASE_ATTACK,
        damageInfo->HitInfo & SPELL_HIT_TYPE_CRIT ? MELEE_HIT_CRIT :
                                                    MELEE_HIT_NORMAL);
    DealDamage(pVictim, damageInfo->damage, &cleanDamage, SPELL_DIRECT_DAMAGE,
        damageInfo->schoolMask, spellProto, durabilityLoss, damageInfo->absorb);
}

// TODO for melee need create structure as in
void Unit::CalculateMeleeDamage(Unit* pVictim, uint32 damage,
    CalcDamageInfo* damageInfo, WeaponAttackType attackType)
{
    damageInfo->attacker = this;
    damageInfo->target = pVictim;
    damageInfo->damageSchoolMask = GetMeleeDamageSchoolMask();
    damageInfo->attackType = attackType;
    damageInfo->damage = 0;
    damageInfo->cleanDamage = 0;
    damageInfo->absorb = 0;
    damageInfo->resist = 0;
    damageInfo->blocked_amount = 0;

    damageInfo->TargetState = VICTIMSTATE_UNAFFECTED;
    damageInfo->HitInfo = HITINFO_NORMALSWING;
    damageInfo->procAttacker = PROC_FLAG_NONE;
    damageInfo->procVictim = PROC_FLAG_NONE;
    damageInfo->procEx = PROC_EX_NONE;
    damageInfo->hitOutCome = MELEE_HIT_EVADE;

    if (!pVictim)
        return;
    if (!this->isAlive() || !pVictim->isAlive())
        return;

    // Select HitInfo/procAttacker/procVictim flag based on attack type
    switch (attackType)
    {
    case BASE_ATTACK:
        damageInfo->procAttacker = PROC_FLAG_SUCCESSFUL_MELEE_HIT;
        damageInfo->procVictim = PROC_FLAG_TAKEN_MELEE_HIT;
        damageInfo->HitInfo = HITINFO_NORMALSWING2;
        break;
    case OFF_ATTACK:
        damageInfo->procAttacker =
            PROC_FLAG_SUCCESSFUL_MELEE_HIT | PROC_FLAG_SUCCESSFUL_OFFHAND_HIT;
        damageInfo->procVictim =
            PROC_FLAG_TAKEN_MELEE_HIT; //|PROC_FLAG_TAKEN_OFFHAND_HIT // not
        // used
        damageInfo->HitInfo = HITINFO_LEFTSWING;
        break;
    case RANGED_ATTACK:
        damageInfo->procAttacker = PROC_FLAG_SUCCESSFUL_RANGED_HIT;
        damageInfo->procVictim = PROC_FLAG_TAKEN_RANGED_HIT;
        damageInfo->HitInfo = HITINFO_UNK3; // test (dev note: test what?
                                            // HitInfo flag possibly not
                                            // confirmed.)
        break;
    default:
        break;
    }

    // Physical Immune check
    if (damageInfo->target->IsImmunedToDamage(damageInfo->damageSchoolMask))
    {
        damageInfo->HitInfo |= HITINFO_NORMALSWING;
        damageInfo->TargetState = VICTIMSTATE_IS_IMMUNE;

        damageInfo->procEx |= PROC_EX_IMMUNE;
        damageInfo->damage = 0;
        damageInfo->cleanDamage = 0;
        return;
    }
    damage += CalculateDamage(damageInfo->attackType, false);

    // Add melee damage bonus
    damage = MeleeDamageBonusDone(
        damageInfo->target, damage, damageInfo->attackType);
    damage = damageInfo->target->MeleeDamageBonusTaken(
        this, damage, damageInfo->attackType);

    // Only physical damge is affected by melee scaling & armor
    if (damageInfo->damageSchoolMask <= 1)
    {
        // Calculate armor reduction
        damageInfo->damage =
            calculate_armor_reduced_damage(damageInfo->target, damage);
    }
    else
    {
        damageInfo->damage = damage;
        // Resistance is calculated below
    }

    damageInfo->cleanDamage += damage - damageInfo->damage;

    damageInfo->hitOutCome =
        RollMeleeOutcomeAgainst(damageInfo->target, damageInfo->attackType);

    // Disable parry or dodge for ranged attack
    if (damageInfo->attackType == RANGED_ATTACK)
    {
        if (damageInfo->hitOutCome == MELEE_HIT_PARRY)
            damageInfo->hitOutCome = MELEE_HIT_NORMAL;
        if (damageInfo->hitOutCome == MELEE_HIT_DODGE)
            damageInfo->hitOutCome = MELEE_HIT_MISS;
    }

    // Disable blocking for elemental based melee damage
    if (damageInfo->damageSchoolMask > 1)
    {
        if (damageInfo->hitOutCome == MELEE_HIT_BLOCK)
            damageInfo->hitOutCome = MELEE_HIT_NORMAL;
    }

    switch (damageInfo->hitOutCome)
    {
    case MELEE_HIT_EVADE:
    {
        damageInfo->HitInfo |= HITINFO_MISS | HITINFO_SWINGNOHITSOUND;
        damageInfo->TargetState = VICTIMSTATE_EVADES;

        damageInfo->procEx |= PROC_EX_EVADE;
        damageInfo->damage = 0;
        damageInfo->cleanDamage = 0;
        return;
    }
    case MELEE_HIT_MISS:
    {
        damageInfo->HitInfo |= HITINFO_MISS;
        damageInfo->TargetState = VICTIMSTATE_UNAFFECTED;

        damageInfo->procEx |= PROC_EX_MISS;
        damageInfo->damage = 0;
        damageInfo->cleanDamage = 0;
        break;
    }
    case MELEE_HIT_NORMAL:
        damageInfo->TargetState = VICTIMSTATE_NORMAL;
        damageInfo->procEx |= PROC_EX_NORMAL_HIT;
        break;
    case MELEE_HIT_CRIT:
    {
        damageInfo->HitInfo |= HITINFO_CRITICALHIT;
        damageInfo->TargetState = VICTIMSTATE_NORMAL;

        damageInfo->procEx |= PROC_EX_CRITICAL_HIT;
        // Crit bonus calc
        damageInfo->damage += damageInfo->damage;
        int32 mod = 0;
        // Apply SPELL_AURA_MOD_ATTACKER_RANGED_CRIT_DAMAGE or
        // SPELL_AURA_MOD_ATTACKER_MELEE_CRIT_DAMAGE
        if (damageInfo->attackType == RANGED_ATTACK)
            mod += damageInfo->target->GetTotalAuraModifier(
                SPELL_AURA_MOD_ATTACKER_RANGED_CRIT_DAMAGE);
        else
            mod += damageInfo->target->GetTotalAuraModifier(
                SPELL_AURA_MOD_ATTACKER_MELEE_CRIT_DAMAGE);

        mod += GetTotalAuraModifierByMiscMask(
            SPELL_AURA_MOD_CRIT_DAMAGE_BONUS, SPELL_SCHOOL_MASK_NORMAL);

        uint32 crTypeMask = damageInfo->target->GetCreatureTypeMask();

        // Increase crit damage from SPELL_AURA_MOD_CRIT_PERCENT_VERSUS
        mod += GetTotalAuraModifierByMiscMask(
            SPELL_AURA_MOD_CRIT_PERCENT_VERSUS, crTypeMask);
        if (mod != 0)
            damageInfo->damage =
                int32((damageInfo->damage) * float((100.0f + mod) / 100.0f));

        // Resilience - reduce crit damage
        if (pVictim->GetTypeId() == TYPEID_PLAYER)
        {
            uint32 resilienceReduction =
                ((Player*)pVictim)
                    ->GetMeleeCritDamageReduction(damageInfo->damage);
            damageInfo->damage -= resilienceReduction;
            damageInfo->cleanDamage += resilienceReduction;
        }
        break;
    }
    case MELEE_HIT_PARRY:
        damageInfo->TargetState = VICTIMSTATE_PARRY;
        damageInfo->procEx |= PROC_EX_PARRY;
        damageInfo->cleanDamage += damageInfo->damage;
        damageInfo->damage = 0;
        break;

    case MELEE_HIT_DODGE:
        damageInfo->TargetState = VICTIMSTATE_DODGE;
        damageInfo->procEx |= PROC_EX_DODGE;
        damageInfo->cleanDamage += damageInfo->damage;
        damageInfo->damage = 0;
        break;
    case MELEE_HIT_BLOCK:
    {
        damageInfo->TargetState = VICTIMSTATE_NORMAL;
        damageInfo->procEx |= PROC_EX_BLOCK;
        damageInfo->blocked_amount = damageInfo->target->GetShieldBlockValue();
        if (damageInfo->blocked_amount >= damageInfo->damage)
        {
            damageInfo->TargetState = VICTIMSTATE_BLOCKS;
            damageInfo->blocked_amount = damageInfo->damage;
        }
        else
            damageInfo->procEx |= PROC_EX_NORMAL_HIT; // Partial blocks can
                                                      // still cause attacker
                                                      // procs

        damageInfo->damage -= damageInfo->blocked_amount;
        damageInfo->cleanDamage += damageInfo->blocked_amount;
        break;
    }
    case MELEE_HIT_GLANCING:
    {
        damageInfo->HitInfo |= HITINFO_GLANCING;
        damageInfo->TargetState = VICTIMSTATE_NORMAL;
        damageInfo->procEx |= PROC_EX_NORMAL_HIT;
        damageInfo->cleanDamage +=
            damageInfo->damage - uint32(0.76f * damageInfo->damage);
        damageInfo->damage = uint32(0.76f * damageInfo->damage);
        break;
    }
    case MELEE_HIT_CRUSHING:
    {
        damageInfo->HitInfo |= HITINFO_CRUSHING;
        damageInfo->TargetState = VICTIMSTATE_NORMAL;
        damageInfo->procEx |= PROC_EX_NORMAL_HIT;
        // 150% normal damage
        damageInfo->damage += (damageInfo->damage / 2);
        break;
    }
    default:
        break;
    }

    // Calculate absorb resist
    if (int32(damageInfo->damage) > 0)
    {
        damageInfo->procVictim |= PROC_FLAG_TAKEN_ANY_DAMAGE;

        damageInfo->damage = damageInfo->target->do_resist_absorb_helper(this,
            damageInfo->damage, nullptr, true, damageInfo->damageSchoolMask,
            &damageInfo->absorb, &damageInfo->resist);

        if (damageInfo->absorb)
        {
            damageInfo->HitInfo |= HITINFO_ABSORB;
            damageInfo->procEx |= PROC_EX_ABSORB;
        }
        if (damageInfo->resist)
            damageInfo->HitInfo |= HITINFO_RESIST;
    }
    else
    {
        // Impossible to get negative results
        assert(damageInfo->damage == 0);
    }
}

void Unit::DealMeleeDamage(CalcDamageInfo* damageInfo, bool durabilityLoss,
    ExtraAttackType extraAttackType, uint32 extraAttackId)
{
    if (damageInfo == nullptr)
        return;
    Unit* pVictim = damageInfo->target;

    if (!pVictim)
        return;

    if (!pVictim->isAlive() || pVictim->IsTaxiFlying() ||
        (pVictim->GetTypeId() == TYPEID_UNIT &&
            ((Creature*)pVictim)->IsInEvadeMode()))
        return;

    // You don't lose health from damage taken from another player while in a
    // sanctuary
    // You still see it in the combat log though
    if (!IsAllowedDamageInArea(pVictim))
        return;

    // Hmmmm dont like this emotes client must by self do all animations
    if (damageInfo->HitInfo & HITINFO_CRITICALHIT)
        pVictim->HandleEmoteCommand(EMOTE_ONESHOT_WOUNDCRITICAL);
    if (damageInfo->blocked_amount &&
        damageInfo->TargetState != VICTIMSTATE_BLOCKS)
        pVictim->HandleEmoteCommand(EMOTE_ONESHOT_PARRYSHIELD);

    if (damageInfo->TargetState == VICTIMSTATE_PARRY)
    {
        // Get attack timers
        float offtime = float(pVictim->getAttackTimer(OFF_ATTACK));
        float basetime = float(pVictim->getAttackTimer(BASE_ATTACK));
        // Reduce attack time
        if (pVictim->haveOffhandWeapon() && offtime < basetime)
        {
            float percent20 = pVictim->GetAttackTime(OFF_ATTACK) * 0.20f;
            float percent60 = 3.0f * percent20;
            if (offtime > percent20 && offtime <= percent60)
            {
                pVictim->setAttackTimer(OFF_ATTACK, uint32(percent20));
            }
            else if (offtime > percent60)
            {
                offtime -= 2.0f * percent20;
                pVictim->setAttackTimer(OFF_ATTACK, uint32(offtime));
            }
        }
        else
        {
            float percent20 = pVictim->GetAttackTime(BASE_ATTACK) * 0.20f;
            float percent60 = 3.0f * percent20;
            if (basetime > percent20 && basetime <= percent60)
            {
                pVictim->setAttackTimer(BASE_ATTACK, uint32(percent20));
            }
            else if (basetime > percent60)
            {
                basetime -= 2.0f * percent20;
                pVictim->setAttackTimer(BASE_ATTACK, uint32(basetime));
            }
        }
    }

    // Call default DealDamage
    CleanDamage cleanDamage(damageInfo->cleanDamage, damageInfo->attackType,
        damageInfo->hitOutCome);
    DealDamage(pVictim, damageInfo->damage, &cleanDamage, DIRECT_DAMAGE,
        damageInfo->damageSchoolMask, nullptr, durabilityLoss,
        damageInfo->absorb);

    // If this is a creature and it attacks from behind it has a probability to
    // daze its victim
    if (damageInfo->damage != 0 &&
        (damageInfo->hitOutCome == MELEE_HIT_CRIT ||
            damageInfo->hitOutCome == MELEE_HIT_CRUSHING ||
            damageInfo->hitOutCome == MELEE_HIT_NORMAL ||
            damageInfo->hitOutCome == MELEE_HIT_GLANCING) &&
        GetTypeId() != TYPEID_PLAYER &&
        !((Creature*)this)->GetCharmerOrOwnerGuid() &&
        !pVictim->HasInArc(M_PI_F, this))
    {
        // 20% base chance
        float prob = 0.2f;

        // the base chance is reduced for players under level 30, a level 10
        // player has a 7% chance
        if (pVictim->getLevel() < 30)
            prob = 0.0065f * pVictim->getLevel() + 0.005f;

        // base % applies when defense skill == attack skill
        // if you're crit immune to a mob, you're also daze immune, i.e.
        // versus a level 70 mob (350 attack skill) you need 475 defense skill
        uint32 def = pVictim->GetDefenseSkillValue();
        uint32 att = GetUnitMeleeSkill();
        float diff = (float)att - (float)def;

        // each 6.25 skills of att higher reduces chance by 1%,
        // and each 6.25 skill under increases chance by 1%
        prob += diff / 625.0f;

        // probability falls in range: [0,40]%
        prob = estd::rangify(0.0f, 0.4f, prob);

        if (rand_norm_f() < prob)
            CastSpell(pVictim, 1604, true);
    }

    // update at damage Judgement aura duration that applied by attacker at
    // victim
    if (damageInfo->damage)
    {
        pVictim->loop_auras([this](AuraHolder* holder)
            {
                const SpellEntry* info = holder->GetSpellProto();
                if (info->AttributesEx3 & SPELL_ATTR_EX3_CANT_MISS &&
                    info->SpellFamilyName == SPELLFAMILY_PALADIN &&
                    holder->GetCasterGuid() == GetObjectGuid())
                    holder->RefreshHolder();
                return true; // continue
            });
    }

    // If not miss & victim is not immune to damage (e.g., we're hitting someone
    // who's under the effect of Divine Shield)
    if (!(damageInfo->HitInfo & HITINFO_MISS) &&
        !pVictim->IsImmunedToDamage(damageInfo->damageSchoolMask))
    {
        // on weapon hit casts
        if (GetTypeId() == TYPEID_PLAYER && pVictim->isAlive())
            ((Player*)this)
                ->CastItemCombatSpell(pVictim, damageInfo->attackType,
                    extraAttackType, extraAttackId, nullptr);

        // victim's damage shield (does not apply on parry/dodge)
        if (damageInfo->hitOutCome != MELEE_HIT_PARRY &&
            damageInfo->hitOutCome != MELEE_HIT_DODGE &&
            damageInfo->hitOutCome != MELEE_HIT_MISS)
        {
            const Auras& dmgShields =
                pVictim->GetAurasByType(SPELL_AURA_DAMAGE_SHIELD);
            for (const auto& dmgShield : dmgShields)
            {
                uint32 damage = (dmgShield)->GetModifier()->m_amount;
                const SpellEntry* spell_proto = (dmgShield)->GetSpellProto();

                if (HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE))
                    continue;

                if (HasFlag(
                        UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_PLAYER_ATTACKABLE) &&
                    pVictim->player_controlled())
                    continue;

                // Damage Shields should abide by spell immunity
                if (IsImmunedToDamage(GetSpellSchoolMask(spell_proto)))
                {
                    pVictim->SendSpellMiss(
                        this, spell_proto->Id, SPELL_MISS_IMMUNE);
                    continue;
                }

                // Binary resists are probably possible, whereas partial resists
                // are probably not (no field for that exists in package)
                float miss_chance =
                    pVictim->SpellMissChanceCalc(this, spell_proto);
                if (roll_chance_f(miss_chance))
                {
                    pVictim->SendSpellMiss(
                        this, spell_proto->Id, SPELL_MISS_RESIST);
                    continue;
                }

                // Calculate absorbed damage
                auto absorb = do_absorb(
                    pVictim, damage, GetSpellSchoolMask(spell_proto), false);
                damage -= absorb;

                pVictim->DealDamageMods(this, damage, nullptr);

                // Apply spell mods
                if (Player* modOwner = GetSpellModOwner())
                    modOwner->ApplySpellMod(
                        spell_proto->Id, SPELLMOD_DAMAGE, damage);

                // TODO: Is there some way we can inform the client about
                //       absorb?
                if (damage > 0)
                {
                    WorldPacket data(
                        SMSG_SPELLDAMAGESHIELD, (8 + 8 + 4 + 4 + 4));
                    data << pVictim->GetObjectGuid();
                    data << GetObjectGuid();
                    data << uint32(spell_proto->Id);
                    data << uint32(damage); // Damage
                    data << uint32(spell_proto->SchoolMask);
                    pVictim->SendMessageToSet(&data, true);
                }

                pVictim->DealDamage(this, damage, nullptr, SPELL_DIRECT_DAMAGE,
                    GetSpellSchoolMask(spell_proto), spell_proto, true, absorb);
            }
        }
    }
}

void Unit::HandleEmoteCommand(uint32 emote_id)
{
    WorldPacket data(SMSG_EMOTE, 4 + 8);
    data << uint32(emote_id);
    data << GetObjectGuid();
    SendMessageToSet(&data, true);
}

void Unit::HandleEmoteState(uint32 emote_id)
{
    SetUInt32Value(UNIT_NPC_EMOTESTATE, emote_id);
}

void Unit::HandleEmote(uint32 emote_id)
{
    if (!emote_id)
        HandleEmoteState(0);
    else if (EmotesEntry const* emoteEntry = sEmotesStore.LookupEntry(emote_id))
    {
        if (emoteEntry->EmoteType) // 1,2 states, 0 command
            HandleEmoteState(emote_id);
        else
            HandleEmoteCommand(emote_id);
    }
}

uint32 Unit::calculate_armor_reduced_damage(Unit* victim, uint32 damage)
{
    uint32 newdamage = 0;
    float armor = (float)victim->GetArmor();

    // Ignore enemy armor by SPELL_AURA_MOD_TARGET_RESISTANCE aura
    armor += GetTotalAuraModifierByMiscMask(
        SPELL_AURA_MOD_TARGET_RESISTANCE, SPELL_SCHOOL_MASK_NORMAL);

    if (armor < 0.0f)
        armor = 0.0f;

    float levelModifier = (float)getLevel();
    if (levelModifier > 59)
        levelModifier = levelModifier + (4.5f * (levelModifier - 59));

    float tmpvalue = 0.1f * armor / (8.5f * levelModifier + 40);
    tmpvalue = tmpvalue / (1.0f + tmpvalue);

    if (tmpvalue < 0.0f)
        tmpvalue = 0.0f;
    if (tmpvalue > 0.75f)
        tmpvalue = 0.75f;

    newdamage = uint32(damage - (damage * tmpvalue));

    return (newdamage > 1) ? newdamage : 1;
}

uint32 Unit::do_spell_block(Unit* attacker, uint32 damage,
    const SpellEntry* spell_info, WeaponAttackType attack_type)
{
    uint32 blocked_damage = 0;
    bool blocked = false;

    // Get blocked status
    switch (spell_info->DmgClass)
    {
    // Melee and Ranged Spells
    case SPELL_DAMAGE_CLASS_RANGED:
    case SPELL_DAMAGE_CLASS_MELEE:
        blocked = IsSpellBlocked(attacker, spell_info, attack_type);
        break;
    default:
        break;
    }

    if (blocked)
    {
        blocked_damage = GetShieldBlockValue();
        if (damage < blocked_damage)
            blocked_damage = damage;
        damage -= blocked_damage;
    }

    return blocked_damage;
}

// Apply resistance rules to school damage
// Returns: how many damage points were resisted
uint32 Unit::do_resistance(Unit* caster, uint32 damage, uint32 school_mask)
{
    if (damage == 0)
        return 0;

    if ((school_mask & SPELL_SCHOOL_MASK_NORMAL) != 0)
        return 0;

    return damage * calculate_partial_resistance(caster, school_mask);
}

// Apply resistance rules to spell damage
// Returns: how many damage points were resisted
uint32 Unit::do_resistance(
    Unit* caster, uint32 damage, const SpellEntry* spell_info)
{
    if (damage == 0)
        return 0;

    // Binary Resistance is checked in Unit::roll_binary_spell()
    if (!IsPartiallyResistable(spell_info))
        return 0;

    float resist =
        calculate_partial_resistance(caster, GetSpellSchoolMask(spell_info));

    return damage * resist;
}

// Apply absorb to damage
// Note: This will consume remaining damage on active absorb auras
//       And cause spells like reflective shield to trigger
// Params:
//     @can_reflect_damage: If auras to reflect some damage on absorb
//     exists, and this is true, the reflect damage will be applied to
//     attacker.
// Returns: how many damage points were absorbed
uint32 Unit::do_absorb(Unit* attacker, uint32 start_damage, uint32 school_mask,
    bool can_reflect_damage)
{
    if (start_damage == 0)
        return 0;

    int32 damage = start_damage;

    // Absorbs without mana cost
    // Make copy of container, content will change
    Auras school_absorb = GetAurasByType(SPELL_AURA_SCHOOL_ABSORB);
    for (auto& aura : school_absorb)
    {
        if (damage <= 0)
            break;

        Modifier* mod = aura->GetModifier();
        auto absorb_strength = mod->m_amount; // how much can be absorbed
        auto aura_info = aura->GetSpellProto();

        if (!(mod->m_miscvalue & school_mask))
            continue;

        // Handle custom absorb auras
        switch (aura_info->SpellFamilyName)
        {
        case SPELLFAMILY_GENERIC:
        {
            // Reflective Shield (Lady Malande boss)
            if (aura_info->Id == 41475 && can_reflect_damage)
            {
                int32 dmg = 0;
                if (damage < absorb_strength)
                    dmg = damage / 2;
                else
                    dmg = absorb_strength / 2;
                CastCustomSpell(attacker, 33619, &dmg, nullptr, nullptr, true,
                    nullptr, aura);
                break;
            }
            // Argussian Compass
            if (aura_info->Id == 39228)
            {
                // Max absorb stored in 1 dummy effect
                int32 max_absorb =
                    aura_info->CalculateSimpleValue(EFFECT_INDEX_1);
                if (max_absorb < absorb_strength)
                    absorb_strength = max_absorb;
                break;
            }
            break;
        }
        case SPELLFAMILY_ROGUE:
        {
            // Cheat Death: handled in Unit::do_death_prevention
            if (aura_info->SpellIconID == 2109)
                continue;
            break;
        }
        case SPELLFAMILY_PRIEST:
        {
            // Reflective Shield
            if (aura_info->IsFitToFamilyMask(0x1) && can_reflect_damage)
            {
                if (attacker == this)
                    break;
                Unit* caster = aura->GetCaster();
                if (!caster)
                    break;
                const Auras& override_class_scripts =
                    caster->GetAurasByType(SPELL_AURA_OVERRIDE_CLASS_SCRIPTS);
                for (const auto& ocs_aura : override_class_scripts)
                {
                    switch (ocs_aura->GetModifier()->m_miscvalue)
                    {
                    case 5065: // Rank 1
                    case 5064: // Rank 2
                    case 5063: // Rank 3
                    case 5062: // Rank 4
                    case 5061: // Rank 5
                    {
                        int32 dmg = 0;
                        if (damage >= absorb_strength)
                            dmg = ocs_aura->GetModifier()->m_amount *
                                  absorb_strength / 100;
                        else
                            dmg = ocs_aura->GetModifier()->m_amount * damage /
                                  100;
                        CastCustomSpell(attacker, 33619, &dmg, nullptr, nullptr,
                            true, nullptr, aura);
                        break;
                    }
                    default:
                        break;
                    }
                }
            }
            break;
        }
        default:
            break;
        }

        if (damage < absorb_strength)
            absorb_strength = damage;
        damage -= absorb_strength;

        // Reduce remaining shield strength
        mod->m_amount -= absorb_strength;
        if (aura->GetHolder()->DropAuraCharge())
            mod->m_amount = 0;
        if (mod->m_amount <= 0)
            RemoveAuraHolder(aura->GetHolder(), AURA_REMOVE_BY_SHIELD_BREAK);
    }

    // Absorb by mana cost
    // Make copy of container, content will change
    Auras mana_shields = GetAurasByType(SPELL_AURA_MANA_SHIELD);
    for (auto& aura : mana_shields)
    {
        if (damage == 0)
            break;

        Modifier* mod = aura->GetModifier();
        auto absorb_strength = mod->m_amount; // how much can be absorbed
        auto aura_info = aura->GetSpellProto();

        if (!(mod->m_miscvalue & school_mask))
            continue;

        if (damage < absorb_strength)
            absorb_strength = damage;

        float mana_coeff = aura_info->EffectMultipleValue[aura->GetEffIndex()];
        if (mana_coeff)
        {
            if (Player* modOwner = GetSpellModOwner())
                modOwner->ApplySpellMod(
                    aura->GetId(), SPELLMOD_MULTIPLE_VALUE, mana_coeff);

            int32 max_absorb = int32(GetPower(POWER_MANA) / mana_coeff);
            if (absorb_strength > max_absorb)
                absorb_strength = max_absorb;

            int32 mana_cost = int32(absorb_strength * mana_coeff);
            ApplyPowerMod(POWER_MANA, mana_cost, false);
        }

        mod->m_amount -= absorb_strength;
        if (mod->m_amount <= 0)
            RemoveAuraHolder(aura->GetHolder(), AURA_REMOVE_BY_SHIELD_BREAK);

        damage -= absorb_strength;
    }

    // Cheat Death % absorb
    if (GetTypeId() == TYPEID_PLAYER && getClass() == CLASS_ROGUE &&
        has_aura(45182, SPELL_AURA_DUMMY))
    {
        auto holder = get_aura(45182);
        Aura* aura;
        if (holder && (aura = holder->GetAura(EFFECT_INDEX_0)) != nullptr)
        {
            if (GetTypeId() == TYPEID_PLAYER &&
                aura->GetModifier()->m_miscvalue & school_mask)
            {
                // Cheat Death reduction scales with resilience
                // Absorbed amount is just crit_reduc * 4
                float crit_reduc =
                    static_cast<Player*>(this)->GetRatingBonusValue(
                        CR_CRIT_TAKEN_SPELL) *
                    2;
                float absorb = crit_reduc * 4;
                if (absorb > 90)
                    absorb = 90;
                damage -= damage * (absorb / 100.0f);
            }
        }
    }

    return start_damage - damage;
}

// Apply sharing of damage (where some of it is absorbed by another unit)
// Note: This will potentially damage other units
// Returns: damage points absorbed by damage sharing
uint32 Unit::do_damage_sharing(
    Unit* attacker, uint32 start_damage, uint32 school_mask)
{
    if (attacker == this || start_damage == 0)
        return 0;

    int32 damage = start_damage;

    // Make copy of containers, content will change
    Auras splits = GetAurasByType(SPELL_AURA_SPLIT_DAMAGE_FLAT);
    Auras tmp = GetAurasByType(SPELL_AURA_SPLIT_DAMAGE_PCT);
    splits.insert(splits.end(), tmp.begin(), tmp.end());
    for (auto aura : splits)
    {
        if (damage == 0)
            break;

        Modifier* mod = aura->GetModifier();
        auto split_strength = mod->m_amount; // how much can be absorbed
        auto aura_info = aura->GetSpellProto();
        Unit* caster = aura->GetCaster();

        if (!(mod->m_miscvalue & school_mask))
            continue;

        if (!caster || caster == this || !caster->IsInWorld() ||
            !caster->isAlive())
            continue;

        if (aura_info->EffectApplyAuraName[aura->GetEffIndex()] ==
            SPELL_AURA_SPLIT_DAMAGE_PCT)
            split_strength = damage * (split_strength / 100.0f);

        if (damage < split_strength)
            split_strength = damage;

        damage -= split_strength;

        if (caster->IsImmunedToDamage((SpellSchoolMask)school_mask))
        {
            // Blizzard just sets damage = 0 for immunity, but that becomes
            // absorbed
            // for us which is not correct either (should say 0 in the log), so
            // we
            // just send immunity here
            attacker->SendSpellMiss(caster, aura_info->Id, SPELL_MISS_IMMUNE);
        }
        else
        {
            // Calculate damage
            uint32 split_damage = split_strength;

            // Scale split with target's absorb
            auto split_absorb =
                caster->do_absorb(caster, damage, school_mask, false);
            caster->DealDamageMods(caster, split_damage, &split_absorb);
            split_damage -=
                split_absorb > split_damage ? split_damage : split_absorb;

            // Deal damage & send combat log info
            auto clean =
                CleanDamage(split_damage, BASE_ATTACK, MELEE_HIT_NORMAL);
            attacker->DealDamage(caster, split_damage, &clean, DIRECT_DAMAGE,
                (SpellSchoolMask)school_mask, aura_info, false,
                split_absorb > 0);
            attacker->SendSpellNonMeleeDamageLog(caster, aura_info->Id,
                split_damage, (SpellSchoolMask)school_mask, split_absorb, 0,
                false, 0);
        }
    }

    return start_damage - damage;
}

// Applies death prevention if damage is enough to kill target
// Note: Only handles Rogue's Cheat Death at the moment
// Returns: how many damage points were absorbed
uint32 Unit::do_death_prevention(uint32 damage)
{
    if (GetTypeId() != TYPEID_PLAYER || getClass() != CLASS_ROGUE)
        return 0;

    if (damage < GetHealth())
        return 0;

    if (static_cast<Player*>(this)->HasSpellCooldown(31231))
        return 0;

    const SpellEntry* info = nullptr;

    const Auras& school_absorb = GetAurasByType(SPELL_AURA_SCHOOL_ABSORB);
    for (auto& aura : school_absorb)
    {
        auto aura_info = aura->GetSpellProto();
        if (aura_info->SpellFamilyName == SPELLFAMILY_ROGUE &&
            aura_info->SpellIconID == 2109)
        {
            if (aura_info->SpellIconID == 2109)
            {
                if (roll_chance_i(aura->GetModifier()->m_amount))
                    info = aura_info;
                break;
            }
        }
    }

    if (!info)
        return 0;

    CastSpell(this, 31231, true);
    ((Player*)this)
        ->AddSpellCooldown(31231, 0, WorldTimer::time_no_syscall() + 60);

    // Drop down to 10% health if you're above, no damage otherwise
    uint32 health10 = GetMaxHealth() / 10;
    auto new_damage = GetHealth() > health10 ? GetHealth() - health10 : 0;

    return damage - new_damage;
}

// Helper function to apply multiple dos that are commonly used together
// Applies: resist, absorb, damage sharing and death prevention
// Params:
//     @spell_info & @school_mask: Only need one or the other
// Returns: remaining damage
uint32 Unit::do_resist_absorb_helper(Unit* attacker, uint32 damage,
    const SpellEntry* spell_info, bool can_reflect_damage, uint32 school_mask,
    uint32* absorb_out, uint32* resist_out)
{
    if (absorb_out)
        *absorb_out = 0;
    if (resist_out)
        *resist_out = 0;

    if (damage == 0)
        return 0;

    uint32 resist = 0;
    if (spell_info)
    {
        school_mask = GetSpellSchoolMask(spell_info);
        resist = do_resistance(attacker, damage, spell_info);
    }
    else
        resist = do_resistance(attacker, damage, school_mask);

    if (resist > 0)
    {
        damage -= resist;
        if (resist_out)
            *resist_out += resist;
    }

    if (damage > 0)
    {
        auto absorb =
            do_absorb(attacker, damage, school_mask, can_reflect_damage);
        damage -= absorb;
        if (absorb_out)
            *absorb_out += absorb;
    }

    if (damage > 0)
    {
        auto shared = do_damage_sharing(attacker, damage, school_mask);
        damage -= shared;
        if (absorb_out)
            *absorb_out += shared;
    }

    if (damage > 0)
    {
        auto prevented = do_death_prevention(damage);
        damage -= prevented;
        if (absorb_out && prevented != 0)
            *absorb_out += prevented;
    }

    return damage;
}

float Unit::calculate_partial_resistance(Unit* caster, uint32 school_mask)
{
    /* Table of average reduction -> probability */
    // data derived from:
    // http://web.archive.org/web/20090206211016/http://www.worldofwarcraft.com/info/basics/resistances.html
    static const float resist_table[][5] = {
        // 0%  25%   50%   75%   100%
        {1.00, 0.00, 0.00, 0.00, 0.00}, // avg reduction = 0%
        {0.96, 0.04, 0.00, 0.00, 0.00}, // avg reduction = 1%
        {0.93, 0.06, 0.01, 0.00, 0.00}, // avg reduction = 2%
        {0.89, 0.10, 0.01, 0.00, 0.00}, // etc
        {0.86, 0.12, 0.02, 0.00, 0.00}, {0.83, 0.14, 0.03, 0.00, 0.00},
        {0.80, 0.16, 0.04, 0.00, 0.00}, {0.77, 0.18, 0.05, 0.00, 0.00},
        {0.74, 0.20, 0.06, 0.00, 0.00}, {0.71, 0.22, 0.07, 0.00, 0.00},
        {0.69, 0.23, 0.07, 0.01, 0.00}, {0.66, 0.25, 0.08, 0.01, 0.00},
        {0.63, 0.27, 0.09, 0.01, 0.00}, {0.60, 0.29, 0.10, 0.01, 0.00},
        {0.57, 0.31, 0.11, 0.01, 0.00},
        {0.54, 0.33, 0.11, 0.02, 0.00}, // avg reduction = 15%
        {0.51, 0.35, 0.12, 0.02, 0.00}, {0.49, 0.37, 0.12, 0.02, 0.00},
        {0.46, 0.39, 0.13, 0.02, 0.00}, {0.43, 0.41, 0.14, 0.02, 0.00},
        {0.40, 0.43, 0.14, 0.03, 0.00}, {0.37, 0.45, 0.15, 0.03, 0.00},
        {0.35, 0.46, 0.16, 0.03, 0.00}, {0.32, 0.47, 0.17, 0.04, 0.00},
        {0.30, 0.48, 0.18, 0.04, 0.00}, {0.28, 0.49, 0.19, 0.04, 0.00},
        {0.26, 0.49, 0.20, 0.04, 0.01}, {0.24, 0.50, 0.21, 0.04, 0.01},
        {0.22, 0.50, 0.22, 0.05, 0.01}, {0.21, 0.49, 0.23, 0.06, 0.01},
        {0.20, 0.49, 0.24, 0.06, 0.01}, // avg reduction = 30%
        {0.19, 0.48, 0.25, 0.07, 0.01}, {0.17, 0.48, 0.27, 0.07, 0.01},
        {0.16, 0.47, 0.28, 0.08, 0.01}, {0.15, 0.45, 0.30, 0.09, 0.01},
        {0.14, 0.44, 0.32, 0.09, 0.01}, {0.12, 0.43, 0.34, 0.10, 0.01},
        {0.12, 0.41, 0.36, 0.10, 0.01}, {0.10, 0.40, 0.38, 0.11, 0.01},
        {0.10, 0.38, 0.39, 0.12, 0.01}, {0.09, 0.36, 0.41, 0.13, 0.01},
        {0.09, 0.34, 0.42, 0.14, 0.01}, {0.09, 0.32, 0.43, 0.15, 0.01},
        {0.08, 0.30, 0.45, 0.16, 0.01}, {0.07, 0.28, 0.47, 0.17, 0.01},
        {0.07, 0.26, 0.48, 0.18, 0.01}, // avg reduction = 45%
        {0.07, 0.24, 0.48, 0.19, 0.02}, {0.07, 0.22, 0.49, 0.20, 0.02},
        {0.07, 0.20, 0.50, 0.21, 0.02}, {0.06, 0.19, 0.50, 0.22, 0.03},
        {0.05, 0.19, 0.50, 0.23, 0.03}, {0.04, 0.18, 0.51, 0.24, 0.03},
        {0.03, 0.18, 0.50, 0.25, 0.04}, {0.03, 0.17, 0.50, 0.26, 0.04},
        {0.02, 0.17, 0.49, 0.27, 0.05}, {0.02, 0.16, 0.48, 0.28, 0.06},
        {0.01, 0.16, 0.47, 0.29, 0.07}, {0.01, 0.15, 0.46, 0.30, 0.08},
        {0.01, 0.15, 0.44, 0.31, 0.09}, {0.01, 0.15, 0.42, 0.32, 0.10},
        {0.01, 0.14, 0.40, 0.34, 0.11}, // avg reduction = 60%
        {0.01, 0.14, 0.38, 0.35, 0.12}, {0.01, 0.13, 0.37, 0.36, 0.13},
        {0.01, 0.13, 0.34, 0.37, 0.15}, {0.01, 0.12, 0.33, 0.38, 0.16},
        {0.01, 0.12, 0.31, 0.39, 0.17}, {0.01, 0.11, 0.30, 0.40, 0.18},
        {0.01, 0.10, 0.28, 0.42, 0.19}, {0.01, 0.10, 0.25, 0.43, 0.21},
        {0.01, 0.09, 0.24, 0.44, 0.22}, {0.01, 0.08, 0.23, 0.46, 0.22},
        {0.01, 0.07, 0.22, 0.47, 0.23}, {0.01, 0.06, 0.20, 0.49, 0.24},
        {0.01, 0.05, 0.19, 0.51, 0.24}, {0.01, 0.04, 0.17, 0.53, 0.25},
        {0.01, 0.03, 0.16, 0.55, 0.25}, // avg reduction = 75%
    };

    // Victim's school resistance
    float school_res = (float)GetResistance(
        GetFirstSchoolInMask((SpellSchoolMask)school_mask));
    // Spell penetration
    school_res += (float)caster->GetTotalAuraModifierByMiscMask(
        SPELL_AURA_MOD_TARGET_RESISTANCE, school_mask);
    if (school_res < 0)
        school_res = 0;

    float avg_reduc = school_res * (0.15f / caster->getLevel());
    int avg_reduc_idx = avg_reduc * 100.0f;
    avg_reduc_idx = estd::rangify(0, 75, avg_reduc_idx);

    // NPCs gain 2% average reduction extra for partial resistance per level
    // they're above the caster; note that this resistance cannot be
    // modified by
    // spell penetration
    if (GetTypeId() == TYPEID_UNIT)
    {
        int diff = getLevel() - caster->getLevel();
        while (diff-- > 0 && avg_reduc_idx < 75)
            avg_reduc_idx += (avg_reduc_idx == 74) ? 1 : 2;
    }

    float prob[5] = {0};
    for (int i = 0; i < 5; ++i)
        prob[i] = resist_table[avg_reduc_idx][i];

    int resist = 0;
    float rand = rand_norm_f();
    float prob_sum = prob[0];
    while (rand >= prob_sum && resist < 5)
        prob_sum += prob[++resist];

    return resist * 0.25f;
}

bool Unit::roll_binary_spell(Unit* caster, const SpellEntry* spell)
{
    if (spell->HasAttribute(SPELL_ATTR_EX3_CANT_MISS))
        return false;

    auto school_mask = GetSpellSchoolMask(spell);
    if ((school_mask & SPELL_SCHOOL_MASK_NORMAL) != 0)
        return false;

    if (spell->DmgClass == SPELL_DAMAGE_CLASS_NONE)
        return false;

    if (IsPartiallyResistable(spell))
        return false;

    // Victim's school resistance
    float school_res = (float)GetResistance(GetFirstSchoolInMask(school_mask));
    // Spell penetration
    school_res += (float)caster->GetTotalAuraModifierByMiscMask(
        SPELL_AURA_MOD_TARGET_RESISTANCE, school_mask);
    if (school_res < 0)
        school_res = 0;

    float avg_reduc = school_res * (0.15f / caster->getLevel());
    avg_reduc = estd::rangify(0.0f, 0.75f, avg_reduc);

    float rand = rand_norm_f();
    return rand < avg_reduc;
}

void Unit::DoWhiteAttack(Unit* pVictim, const WhiteAttack& whiteAttack)
{
    if (hasUnitState(UNIT_STAT_CAN_NOT_REACT) ||
        HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PACIFIED))
        return;
    if (!pVictim->isAlive())
        return;
    if (IsCastedSpellPreventingMovementOrAttack())
        return;

    // Ignored range attacks
    if (whiteAttack.weaponAttackType == RANGED_ATTACK)
        return;

    // If we have a pending melee ability (such as heroic strike), we use
    // that
    // instead if, and only if,
    // these 2 criterias hold true: 1) It's a Main Hand attack, and 2) it's
    // NOT
    // an extra attack
    if (whiteAttack.extraAttackType == EXTRA_ATTACK_NONE &&
        whiteAttack.weaponAttackType == BASE_ATTACK &&
        m_currentSpells[CURRENT_MELEE_SPELL])
    {
        // If we don't have enough power to use it, cancel the cast instead
        if (m_currentSpells[CURRENT_MELEE_SPELL]->CheckPower() == SPELL_CAST_OK)
        {
            m_currentSpells[CURRENT_MELEE_SPELL]->cast();
            return;
        }
        else
        {
            m_currentSpells[CURRENT_MELEE_SPELL]->cancel();
        }
    }

    // Attack can be redirected to another target
    pVictim = SelectMagnetTarget(pVictim);

    // Calculate Damage and Apply Mods
    CalcDamageInfo damageInfo;
    CalculateMeleeDamage(pVictim, 0, &damageInfo, whiteAttack.weaponAttackType);
    DealDamageMods(pVictim, damageInfo.damage, &damageInfo.absorb);

    // Send Attacker State to client
    SendAttackStateUpdate(&damageInfo);

    // Proc spells off of this attack

    // Crits are considered normal hits (even though they deal critical
    // damage)
    // for procs if we sit down
    uint32 proc_ex = damageInfo.procEx;
    if (pVictim->IsSitState() && proc_ex & PROC_EX_CRITICAL_HIT)
    {
        // Switch to a normal hit
        proc_ex &= ~PROC_EX_CRITICAL_HIT;
        proc_ex |= PROC_EX_NORMAL_HIT;
    }

    proc_amount procamnt(true, damageInfo.damage, pVictim, damageInfo.absorb);

    // Deal the actual damage
    DealMeleeDamage(&damageInfo, true, whiteAttack.extraAttackType,
        whiteAttack.extraAttackSpellId);

    ProcDamageAndSpell(damageInfo.target, damageInfo.procAttacker,
        damageInfo.procVictim, proc_ex, procamnt, damageInfo.attackType,
        nullptr, whiteAttack.extraAttackType, whiteAttack.extraAttackSpellId);
    // Note: we don't want to use the changed proc_ex here
    if (damageInfo.procEx & PROC_EX_NORMAL_HIT &&
        pVictim->GetTypeId() == TYPEID_PLAYER)
        static_cast<Player*>(pVictim)->DoResilienceCritProc(
            this, damageInfo.damage, damageInfo.attackType, nullptr);

    LOG_DEBUG(logging,
        "DoWhiteAttack: %s attacked %s for %u dmg, "
        "absorbed %u, blocked %u, resisted %u.",
        GetGuidStr().c_str(), pVictim->GetGuidStr().c_str(), damageInfo.damage,
        damageInfo.absorb, damageInfo.blocked_amount, damageInfo.resist);

    // Invokes an AI reaction if victim is an NPC unit
    pVictim->AttackedBy(this);

    if (pVictim->GetTypeId() == TYPEID_UNIT && GetTypeId() == TYPEID_PLAYER)
        static_cast<Creature*>(pVictim)->ResetKitingLeashPos();
    remove_auras_on_event(AURA_INTERRUPT_FLAG_MELEE_ATTACK);

    if (pVictim->GetTypeId() == TYPEID_UNIT &&
        static_cast<Creature*>(pVictim)->AI())
    {
        static_cast<Creature*>(pVictim)->AI()->OnTakenWhiteHit(
            this, damageInfo.attackType, damageInfo.damage, damageInfo.HitInfo);
    }

    if (GetTypeId() == TYPEID_UNIT && static_cast<Creature*>(this)->AI())
    {
        static_cast<Creature*>(this)->AI()->OnWhiteHit(pVictim,
            damageInfo.attackType, damageInfo.damage, damageInfo.HitInfo);
    }

    // Invoke pet behavior's struck party member for anyone with a pet in
    // our
    // party (including us)
    if (pVictim->GetTypeId() == TYPEID_PLAYER)
    {
        if (Group* group = static_cast<Player*>(pVictim)->GetGroup())
        {
            for (auto p : group->members(true))
            {
                if (group->SameSubGroup(static_cast<Player*>(pVictim), p) &&
                    !pVictim->IsHostileTo(p))
                {
                    if (Pet* pet = p->GetPet())
                        if (pet->behavior())
                            pet->behavior()->struck_party_member(
                                this, static_cast<Player*>(pVictim));
                }
            }
        }
        else
        {
            if (Pet* pet = pVictim->GetPet())
                if (pet->behavior())
                    pet->behavior()->struck_party_member(
                        this, static_cast<Player*>(pVictim));
        }
    }
}

/* AttackerStateUpdate has a locking mechanism that works like this:
 Set attackerStateLock to true before AttackerStateUpdate call and false after,
 as well as do not call AttackerStateUpdate when attackerStateLock is true.
 */
void Unit::AttackerStateUpdate(Unit* pVictim, bool isNormalSwing)
{
    if (m_whiteAttacksQueue.size() == 0)
        return;

    bool removeAllPendingAttacks = false;
    if (hasUnitState(UNIT_STAT_CAN_NOT_REACT) ||
        HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PACIFIED))
        removeAllPendingAttacks = true;
    if (!pVictim->isAlive())
        removeAllPendingAttacks = true;
    if (IsCastedSpellPreventingMovementOrAttack())
        removeAllPendingAttacks = true;

    // The above stages causes all pending attacks to be invalidated
    if (removeAllPendingAttacks)
    {
        m_whiteAttacksQueue.clear();
        return;
    }

    // Make sure victim is targeted when doing melee swing (might not be
    // after instant spell cast)
    SetTargetGuid(pVictim->GetObjectGuid());

    // When we begin our swing we have a set of on-next attacks that need to be
    // executed on this particular swing, any that get added after the swing is
    // initiated should not be executed
    if (isNormalSwing)
    {
        std::vector<WhiteAttack> onNextAttacks;
        for (auto itr = m_whiteAttacksQueue.begin();
             itr != m_whiteAttacksQueue.end();)
        {
            if ((*itr).onlyTriggerOnNormalSwing)
            {
                onNextAttacks.push_back(*itr);
                itr = m_whiteAttacksQueue.erase(itr);
            }
            else
                ++itr;
        }

        // Do all On-Next Attacks
        for (auto& onNextAttack : onNextAttacks)
            DoWhiteAttack(pVictim, onNextAttack);
    }

    // Execute all pending instant attacks
    for (auto itr = m_whiteAttacksQueue.begin();
         itr != m_whiteAttacksQueue.end();)
    {
        WhiteAttack attack = *itr;

        // On-Next-Attack attacks should not get swung at this point
        if (attack.onlyTriggerOnNormalSwing)
        {
            ++itr;
            continue;
        }

        // When we do an attack the queue could be modified,
        // therefore we erase the attack before we execute it
        m_whiteAttacksQueue.erase(itr);
        DoWhiteAttack(pVictim, attack);

        // Start from the beginning as more attacks might've been added
        itr = m_whiteAttacksQueue.begin();
    }
}

void Unit::QueueWhiteAttack(WhiteAttack whiteAttack)
{
    m_whiteAttacksQueue.push_back(whiteAttack);
}

MeleeHitOutcome Unit::RollMeleeOutcomeAgainst(
    const Unit* pVictim, WeaponAttackType attType) const
{
    // This is only wrapper

    // Miss chance based on melee
    int32 miss_chance = MeleeMissChanceCalc(pVictim, attType) * 100;

    // Critical hit chance
    int32 crit_chance = GetUnitCriticalChance(attType, pVictim) * 100;

    // stunned target cannot dodge and this is check in GetUnitDodgeChance()
    // (returned 0 in this case)
    int32 dodge_chance = pVictim->GetUnitDodgeChance() * 100;
    int32 block_chance = pVictim->GetUnitBlockChance() * 100;
    int32 parry_chance = pVictim->GetUnitParryChance() * 100;

    return RollMeleeOutcomeAgainst(pVictim, attType, crit_chance, miss_chance,
        dodge_chance, parry_chance, block_chance);
}

MeleeHitOutcome Unit::RollMeleeOutcomeAgainst(const Unit* pVictim,
    WeaponAttackType attType, int32 crit_chance, int32 miss_chance,
    int32 dodge_chance, int32 parry_chance, int32 block_chance) const
{
    if (pVictim->GetTypeId() == TYPEID_UNIT &&
        ((Creature*)pVictim)->IsInEvadeMode())
        return MELEE_HIT_EVADE;

    int32 attackerMaxSkillValueForLevel = GetMaxSkillValueForLevel(pVictim);
    int32 victimMaxSkillValueForLevel = pVictim->GetMaxSkillValueForLevel(this);

    int32 attackerWeaponSkill = GetWeaponSkillValue(attType, pVictim);
    int32 victimDefenseSkill = pVictim->GetDefenseSkillValue(this);

    // bonus from skills is 0.04%
    int32 skillBonus = 4 * (attackerWeaponSkill - victimMaxSkillValueForLevel);
    int32 sum = 0, tmp = 0;
    int32 roll = urand(0, 10000);

    // Bosses have built-in chance; they don't get the level 73 benefit (0.6%)
    if (GetTypeId() == TYPEID_UNIT &&
        static_cast<const Creature*>(this)->GetCreatureInfo()->type_flags &
            CREATURE_TYPEFLAGS_BOSS_NPC)
        skillBonus -= (skillBonus < 60) ? skillBonus : 60;

    LOG_DEBUG(logging,
        "RollMeleeOutcomeAgainst: skill bonus of %d for attacker", skillBonus);
    LOG_DEBUG(logging,
        "RollMeleeOutcomeAgainst: rolled %d, miss %d, dodge %d, parry %d, "
        "block %d, crit %d",
        roll, miss_chance, dodge_chance, parry_chance, block_chance,
        crit_chance);

    tmp = miss_chance;

    if (tmp > 0 && roll < (sum += tmp))
    {
        LOG_DEBUG(logging, "RollMeleeOutcomeAgainst: MISS");
        return MELEE_HIT_MISS;
    }

    // Always crit against a target in /sit or /sleep
    if (pVictim->GetTypeId() == TYPEID_PLAYER && !pVictim->IsStandState())
    {
        LOG_DEBUG(logging, "RollMeleeOutcomeAgainst: CRIT (sitting victim)");
        return MELEE_HIT_CRIT;
    }

    bool from_behind = !pVictim->HasInArc(M_PI_F, this);

    LOG_DEBUG(logging, "RollMeleeOutcomeAgainst: attack came from %s.",
        from_behind ? "back" : "front");

    bool isCasting = pVictim->IsNonMeleeSpellCasted(false, false, true);
    LOG_DEBUG(logging, "RollMeleeOutcomeAgaisnt: victim is casting.");

    // Dodge chance

    // only players can't dodge if attacker is behind
    if ((pVictim->GetTypeId() != TYPEID_PLAYER || !from_behind) && !isCasting)
    {
        // Reduce dodge chance by attacker expertise rating
        if (GetTypeId() == TYPEID_PLAYER)
            dodge_chance -= int32(
                ((Player*)this)->GetExpertiseDodgeOrParryReduction(attType) *
                100);

        // Modify dodge chance by attacker
        // SPELL_AURA_MOD_COMBAT_RESULT_CHANCE
        dodge_chance +=
            GetTotalAuraModifierByMiscValue(
                SPELL_AURA_MOD_COMBAT_RESULT_CHANCE, VICTIMSTATE_DODGE) *
            100;

        tmp = dodge_chance;
        if ((tmp > 0) // check if unit _can_ dodge
            && ((tmp -= skillBonus) > 0) && roll < (sum += tmp))
        {
            LOG_DEBUG(logging, "RollMeleeOutcomeAgainst: DODGE <%d, %d)",
                sum - tmp, sum);
            return MELEE_HIT_DODGE;
        }
    }

    // parry chances
    // check if attack comes from behind, nobody can parry or block if attacker
    // is behind
    if (!from_behind && !isCasting &&
        (pVictim->GetTypeId() != TYPEID_PLAYER ||
            static_cast<const Player*>(pVictim)->CanParry()))
    {
        // Reduce parry chance by attacker expertise rating
        if (GetTypeId() == TYPEID_PLAYER)
            parry_chance -= int32(
                ((Player*)this)->GetExpertiseDodgeOrParryReduction(attType) *
                100);

        // Skill bonus modification
        parry_chance -= skillBonus;

        // 10% Bonus for high level NPCs and ?? NPCs
        if (pVictim->GetTypeId() == TYPEID_UNIT && skillBonus <= -60)
            parry_chance += 1000;

        if (pVictim->GetTypeId() == TYPEID_PLAYER ||
            !(((Creature*)pVictim)->GetCreatureInfo()->flags_extra &
                CREATURE_FLAG_EXTRA_NO_PARRY))
        {
            if (parry_chance > 0 && (roll < (sum += parry_chance)))
            {
                LOG_DEBUG(logging, "RollMeleeOutcomeAgainst: PARRY <%d, %d)",
                    sum - parry_chance, sum);
                return MELEE_HIT_PARRY;
            }
        }
    }

    // Max 25% chance to score a glancing blow against mobs that are higher
    // level (can do only players and pets and not with ranged weapon)
    if (attType != RANGED_ATTACK &&
        (GetTypeId() == TYPEID_PLAYER || ((Creature*)this)->IsPet()) &&
        pVictim->GetTypeId() != TYPEID_PLAYER &&
        !((Creature*)pVictim)->IsPet() &&
        getLevel() < pVictim->GetLevelForTarget(this))
    {
        // cap possible value (with bonuses > max skill)
        int32 skill = attackerWeaponSkill;
        int32 maxskill = attackerMaxSkillValueForLevel;
        skill = (skill > maxskill) ? maxskill : skill;

        tmp = (10 + (victimDefenseSkill - skill)) * 100;
        tmp = tmp > 2500 ? 2500 : tmp;
        if (roll < (sum += tmp))
        {
            LOG_DEBUG(logging, "RollMeleeOutcomeAgainst: GLANCING <%d, %d)",
                sum - 2500, sum);
            return MELEE_HIT_GLANCING;
        }
    }

    // block chances
    // check if attack comes from behind, nobody can parry or block if
    // attacker
    // is behind
    if (!from_behind && !isCasting)
    {
        if (pVictim->GetTypeId() == TYPEID_PLAYER ||
            !(((Creature*)pVictim)->GetCreatureInfo()->flags_extra &
                CREATURE_FLAG_EXTRA_NO_BLOCK))
        {
            tmp = block_chance;
            if ((tmp > 0) // check if unit _can_ block
                && ((tmp -= skillBonus) > 0) && (roll < (sum += tmp)))
            {
                LOG_DEBUG(logging, "RollMeleeOutcomeAgainst: BLOCK <%d, %d)",
                    sum - tmp, sum);
                return MELEE_HIT_BLOCK;
            }
        }
    }

    // Critical chance
    tmp = crit_chance;

    if (tmp > 0 && roll < (sum += tmp))
    {
        LOG_DEBUG(
            logging, "RollMeleeOutcomeAgainst: CRIT <%d, %d)", sum - tmp, sum);
        return MELEE_HIT_CRIT;
    }

    if ((GetTypeId() != TYPEID_PLAYER && !((Creature*)this)->IsPet()) &&
        !(((Creature*)this)->GetCreatureInfo()->flags_extra &
            CREATURE_FLAG_EXTRA_NO_CRUSH))
    {
        // mobs can score crushing blows if they're 3 or more levels above
        // victim
        // or when their weapon skill is 15 or more above victim's defense
        // skill
        tmp = victimDefenseSkill;
        int32 tmpmax = victimMaxSkillValueForLevel;
        // having defense above your maximum (from items, talents etc.) has
        // no
        // effect
        tmp = tmp > tmpmax ? tmpmax : tmp;
        // tmp = mob's level * 5 - player's current defense skill
        tmp = attackerMaxSkillValueForLevel - tmp;
        if (tmp >= 15)
        {
            // add 2% chance per lacking skill point, min. is 15%
            tmp = tmp * 200 - 1500;
            if (roll < (sum += tmp))
            {
                LOG_DEBUG(logging, "RollMeleeOutcomeAgainst: CRUSHING <%d, %d)",
                    sum - tmp, sum);
                return MELEE_HIT_CRUSHING;
            }
        }
    }

    LOG_DEBUG(logging, "RollMeleeOutcomeAgainst: NORMAL");
    return MELEE_HIT_NORMAL;
}

uint32 Unit::CalculateDamage(WeaponAttackType attType, bool normalized)
{
    float min_damage, max_damage;

    if (normalized && GetTypeId() == TYPEID_PLAYER)
        ((Player*)this)
            ->CalculateMinMaxDamage(
                attType, normalized, min_damage, max_damage);
    else
    {
        switch (attType)
        {
        case RANGED_ATTACK:
            min_damage = GetFloatValue(UNIT_FIELD_MINRANGEDDAMAGE);
            max_damage = GetFloatValue(UNIT_FIELD_MAXRANGEDDAMAGE);
            break;
        case BASE_ATTACK:
            min_damage = GetFloatValue(UNIT_FIELD_MINDAMAGE);
            max_damage = GetFloatValue(UNIT_FIELD_MAXDAMAGE);
            break;
        case OFF_ATTACK:
            min_damage = GetFloatValue(UNIT_FIELD_MINOFFHANDDAMAGE);
            max_damage = GetFloatValue(UNIT_FIELD_MAXOFFHANDDAMAGE);
            break;
        // Just for good manner
        default:
            min_damage = 0.0f;
            max_damage = 0.0f;
            break;
        }
    }

    if (min_damage > max_damage)
    {
        std::swap(min_damage, max_damage);
    }

    if (max_damage == 0.0f)
        max_damage = 5.0f;

    return urand((uint32)min_damage, (uint32)max_damage);
}

// Returns scaling factor for spell downranking
float Unit::calculate_spell_downrank_factor(const SpellEntry* spell_info) const
{
    uint32 min_level = 0, max_level = 0;
    if (spell_info->maxLevel)
        max_level = spell_info->maxLevel;
    if (spell_info->baseLevel)
        min_level = spell_info->baseLevel;

    if (max_level == 0)
        return 1.0f;

    float downrank = (max_level + 6.0f) / getLevel();
    if (downrank > 1.0f)
        downrank = 1.0f;

    if (min_level < 20)
    {
        float level20_coeff = 1.0f - (20 - min_level) * 0.0375; // [0.25, 1.0]
        downrank = downrank * level20_coeff;
    }

    return downrank;
}

bool Unit::is_moving(bool only_directional) const
{
    uint32 dir_flags = MOVEFLAG_FORWARD | MOVEFLAG_BACKWARD |
                       MOVEFLAG_STRAFE_LEFT | MOVEFLAG_STRAFE_RIGHT;

    if (GetTypeId() == TYPEID_UNIT)
    {
        if (isCharmedOwnedByPlayerOrPlayer())
            return m_movementInfo.HasMovementFlag(dir_flags);
        return !IsStopped();
    }
    else
    {
        if (only_directional)
            return m_movementInfo.HasMovementFlag(dir_flags);
        return static_cast<const Player*>(this)->isMoving();
    }
}

G3D::Vector3 Unit::predict_dest(int milliseconds, bool estimate)
{
    auto rest_pos = GetTransport() ?
                        G3D::Vector3(m_movementInfo.transport.pos.x,
                            m_movementInfo.transport.pos.y,
                            m_movementInfo.transport.pos.z) :
                        G3D::Vector3(GetX(), GetY(), GetZ());

    if (!is_moving())
        return rest_pos;

    G3D::Vector3 pos;
    float angle = 0;
    float speed = IsWalking() ? GetSpeed(MOVE_WALK) : GetSpeed(MOVE_RUN);
    float secs = milliseconds / 1000.0f;

    if (isCharmedOwnedByPlayerOrPlayer())
    {
        bool fwd = m_movementInfo.HasMovementFlag(MOVEFLAG_FORWARD);
        bool bwd = m_movementInfo.HasMovementFlag(MOVEFLAG_BACKWARD);
        bool lft = m_movementInfo.HasMovementFlag(MOVEFLAG_STRAFE_LEFT);
        bool rgt = m_movementInfo.HasMovementFlag(MOVEFLAG_STRAFE_RIGHT);

        if (!fwd && !bwd && !lft && !rgt)
            return rest_pos;

        if (fwd)
        {
            angle = 0;
        }
        else if (bwd)
        {
            angle = M_PI_F;
            if (!IsWalking())
                speed = GetSpeed(MOVE_WALK);
        }

        if (!fwd && !bwd)
        {
            if (lft)
                angle = M_PI_F * 0.5f;
            else if (rgt)
                angle = M_PI_F * 1.5f;
        }
        else if (lft || rgt)
        {
            bool pos = (lft && fwd) || (rgt && bwd);
            if (pos)
                angle += 0.25f * M_PI_F;
            else
                angle -= 0.25f * M_PI_F;
        }
    }

    pos = GetPoint(angle, speed * secs, !estimate, false, true);

    return pos;
}

void Unit::SendMeleeAttackStart(Unit* pVictim)
{
    WorldPacket data(SMSG_ATTACKSTART, 8 + 8);
    data << GetObjectGuid();
    data << pVictim->GetObjectGuid();

    SendMessageToSet(&data, true);
}

void Unit::SendMeleeAttackStop(Unit* victim)
{
    if (!victim)
        return;

    WorldPacket data(SMSG_ATTACKSTOP, (4 + 16)); // we guess size
    data << GetPackGUID();
    data << victim->GetPackGUID(); // can be 0x00...
    data << uint32(0);             // can be 0x1
    SendMessageToSet(&data, true);
    LOG_DEBUG(logging, "%s %u stopped attacking %s %u",
        (GetTypeId() == TYPEID_PLAYER ? "player" : "creature"), GetGUIDLow(),
        (victim->GetTypeId() == TYPEID_PLAYER ? "player" : "creature"),
        victim->GetGUIDLow());

    /*if(victim->GetTypeId() == TYPEID_UNIT)
    ((Creature*)victim)->AI().EnterEvadeMode(this);*/
}

bool Unit::IsSpellBlocked(
    Unit* pCaster, SpellEntry const* spellEntry, WeaponAttackType attackType)
{
    if (!HasInArc(M_PI_F, pCaster) ||
        (IsNonMeleeSpellCasted(false, false, true) &&
            GetTypeId() == TYPEID_PLAYER))
        return false;

    if (spellEntry)
    {
        // Some spells cannot be blocked
        if (spellEntry->HasAttribute(SPELL_ATTR_IMPOSSIBLE_DODGE_PARRY_BLOCK))
            return false;
    }

    /*
    // Ignore combat result aura (parry/dodge check on prepare)
    AuraList const& ignore =
    GetAurasByType(SPELL_AURA_IGNORE_COMBAT_RESULT);
    for(AuraList::const_iterator i = ignore.begin(); i != ignore.end(); ++i)
    {
        if (!(*i)->isAffectedOnSpell(spellProto))
            continue;
        if ((*i)->GetModifier()->m_miscvalue == ???)
            return false;
    }
    */

    // Check creatures flags_extra for disable block
    if (GetTypeId() == TYPEID_UNIT)
    {
        if (((Creature*)this)->GetCreatureInfo()->flags_extra &
            CREATURE_FLAG_EXTRA_NO_BLOCK)
            return false;
    }

    float blockChance = GetUnitBlockChance();
    blockChance += (int32(pCaster->GetWeaponSkillValue(attackType)) -
                       int32(GetMaxSkillValueForLevel())) *
                   0.04f;

    return roll_chance_f(blockChance);
}

// Melee based spells can be miss, parry or dodge on this step
// Crit or block - determined on damage calculation phase! (and can be both
// in
// some time)
float Unit::MeleeSpellMissChance(Unit* pVictim, WeaponAttackType attType,
    int32 skillDiff, SpellEntry const* spell)
{
    // Calculate hit chance (more correct for chance mod)
    float hitChance = 0.0f;

    // PvP - PvE melee chances
    if (pVictim->GetTypeId() == TYPEID_PLAYER)
        hitChance = 95.0f + skillDiff * 0.04f;
    else if (skillDiff < -10)
        hitChance = 93.0f + (skillDiff + 10) * 0.4f; // 7% base chance to
                                                     // miss for big skill
                                                     // diff (%6 in 3.x)
    else
        hitChance = 95.0f + skillDiff * 0.1f;

    // Hit chance depends from victim auras
    if (attType == RANGED_ATTACK)
        hitChance += pVictim->GetTotalAuraModifier(
            SPELL_AURA_MOD_ATTACKER_RANGED_HIT_CHANCE);
    else
        hitChance += pVictim->GetTotalAuraModifier(
            SPELL_AURA_MOD_ATTACKER_MELEE_HIT_CHANCE);

    // Spellmod from SPELLMOD_RESIST_MISS_CHANCE
    if (Player* modOwner = GetSpellModOwner())
        modOwner->ApplySpellMod(
            spell->Id, SPELLMOD_RESIST_MISS_CHANCE, hitChance);

    // Miss = 100 - hit
    float missChance = 100.0f - hitChance;

    // Bonuses from attacker aura and ratings
    if (attType == RANGED_ATTACK)
        missChance -= m_modRangedHitChance;
    else
        missChance -= m_modMeleeHitChance;

    // Limit miss chance from 0 to 60%
    if (missChance < 0.0f)
        return 0.0f;
    if (missChance > 60.0f)
        return 60.0f;
    return missChance;
}

// Melee based spells hit result calculations
SpellMissInfo Unit::MeleeSpellHitResult(Unit* pVictim, SpellEntry const* spell)
{
    // Return evade for units in evade mode
    if (pVictim->GetTypeId() == TYPEID_UNIT &&
        static_cast<Creature*>(pVictim)->IsInEvadeMode())
        return SPELL_MISS_EVADE;

    WeaponAttackType attType = BASE_ATTACK;

    if (spell->DmgClass == SPELL_DAMAGE_CLASS_RANGED)
        attType = RANGED_ATTACK;

    // bonus from skills is 0.04% per skill Diff
    int32 attackerWeaponSkill =
        (spell->EquippedItemClass == ITEM_CLASS_WEAPON) ?
            int32(GetWeaponSkillValue(attType, pVictim)) :
            GetMaxSkillValueForLevel();
    int32 skillDiff =
        attackerWeaponSkill - int32(pVictim->GetMaxSkillValueForLevel(this));
    int32 fullSkillDiff =
        attackerWeaponSkill - int32(pVictim->GetDefenseSkillValue(this));

    uint32 roll = urand(0, 10000);

    uint32 missChance = uint32(
        MeleeSpellMissChance(pVictim, attType, fullSkillDiff, spell) * 100.0f);
    // Roll miss
    uint32 tmp = spell->HasAttribute(SPELL_ATTR_EX3_CANT_MISS) ? 0 : missChance;
    if (roll < tmp)
        return SPELL_MISS_MISS;

    // Chance resist mechanic (select max value from every mechanic spell
    // effect)
    int32 resist_mech = 0;

    // Mechanic resistance (in the case where the entire spell has a
    // mechanic)
    if (spell->Mechanic)
    {
        int32 resistChance = pVictim->GetTotalAuraModifierByMiscValue(
            SPELL_AURA_MOD_MECHANIC_RESISTANCE, spell->Mechanic);
        if (resistChance > 0)
            resist_mech = resistChance;
    }

    tmp += resist_mech;
    if (roll < tmp)
        return SPELL_MISS_RESIST;

    bool canDodge = true;
    bool canParry = true;

    // Same spells cannot be parry/dodge
    if (spell->HasAttribute(SPELL_ATTR_IMPOSSIBLE_DODGE_PARRY_BLOCK))
        return SPELL_MISS_NONE;

    // Ranged attack cannot be parry/dodge
    if (attType == RANGED_ATTACK)
        return SPELL_MISS_NONE;

    bool from_behind = !pVictim->HasInArc(M_PI_F, this);

    // Check for attack from behind
    if (from_behind)
    {
        // Can`t dodge from behind in PvP (but its possible in PvE)
        if (GetTypeId() == TYPEID_PLAYER &&
            pVictim->GetTypeId() == TYPEID_PLAYER)
            canDodge = false;
        // Can`t parry
        canParry = false;
    }
    // Check creatures flags_extra for disable parry
    if (pVictim->GetTypeId() == TYPEID_UNIT)
    {
        uint32 flagEx = ((Creature*)pVictim)->GetCreatureInfo()->flags_extra;
        if (flagEx & CREATURE_FLAG_EXTRA_NO_PARRY)
            canParry = false;
    }
    // Ignore combat result aura
    const Auras& ignore = GetAurasByType(SPELL_AURA_IGNORE_COMBAT_RESULT);
    for (const auto& elem : ignore)
    {
        if (!(elem)->isAffectedOnSpell(spell))
            continue;
        switch ((elem)->GetModifier()->m_miscvalue)
        {
        case MELEE_HIT_DODGE:
            canDodge = false;
            break;
        case MELEE_HIT_BLOCK:
            break; // Block check in hit step
        case MELEE_HIT_PARRY:
            canParry = false;
            break;
        default:
            LOG_DEBUG(logging,
                "Spell %u SPELL_AURA_IGNORE_COMBAT_RESULT have unhandled "
                "state "
                "%d",
                (elem)->GetId(), (elem)->GetModifier()->m_miscvalue);
            break;
        }
    }

    if (pVictim->IsNonMeleeSpellCasted(false, false, true))
    {
        canDodge = false;
        canParry = false;
    }

    if (canDodge)
    {
        // Roll dodge
        int32 dodgeChance =
            int32(pVictim->GetUnitDodgeChance() * 100.0f) - skillDiff * 4;
        // Reduce enemy dodge chance by SPELL_AURA_MOD_COMBAT_RESULT_CHANCE
        dodgeChance +=
            GetTotalAuraModifierByMiscValue(
                SPELL_AURA_MOD_COMBAT_RESULT_CHANCE, VICTIMSTATE_DODGE) *
            100;
        // Reduce dodge chance by attacker expertise rating
        if (GetTypeId() == TYPEID_PLAYER)
            dodgeChance -= int32(
                ((Player*)this)->GetExpertiseDodgeOrParryReduction(attType) *
                100.0f);
        if (dodgeChance < 0)
            dodgeChance = 0;

        tmp += dodgeChance;
        if (roll < tmp)
            return SPELL_MISS_DODGE;
    }

    if (canParry)
    {
        // Roll parry
        int32 parryChance =
            int32(pVictim->GetUnitParryChance() * 100.0f) - skillDiff * 4;
        // Reduce parry chance by attacker expertise rating
        if (GetTypeId() == TYPEID_PLAYER)
            parryChance -= int32(
                ((Player*)this)->GetExpertiseDodgeOrParryReduction(attType) *
                100.0f);

        // Can`t parry from behind
        if (parryChance < 0)
            parryChance = 0;

        tmp += parryChance;
        if (roll < tmp)
            return SPELL_MISS_PARRY;
    }

    return SPELL_MISS_NONE;
}

float Unit::SpellMissChanceCalc(
    const Unit* pVictim, SpellEntry const* spell) const
{
    float hitChance = 0;

    SpellSchoolMask schoolMask = GetSpellSchoolMask(spell);

    int32 lchance = pVictim->GetTypeId() == TYPEID_PLAYER ? 7 : 11;
    int32 leveldiff = int32(pVictim->GetLevelForTarget(this)) -
                      int32(GetLevelForTarget(pVictim));

    if (leveldiff < 3)
        hitChance = 96 - leveldiff;
    else
        hitChance = 94 - (leveldiff - 2) * lchance;

    if (Player* modOwner = GetSpellModOwner())
        modOwner->ApplySpellMod(
            spell->Id, SPELLMOD_RESIST_MISS_CHANCE, hitChance);
    // Increase from attacker SPELL_AURA_MOD_INCREASES_SPELL_PCT_TO_HIT
    // auras
    hitChance += GetTotalAuraModifierByMiscMask(
        SPELL_AURA_MOD_INCREASES_SPELL_PCT_TO_HIT, schoolMask);
    // Chance hit from victim SPELL_AURA_MOD_ATTACKER_SPELL_HIT_CHANCE auras
    hitChance += pVictim->GetTotalAuraModifierByMiscMask(
        SPELL_AURA_MOD_ATTACKER_SPELL_HIT_CHANCE, schoolMask);
    // Reduce spell hit chance for Area of effect spells from victim
    // SPELL_AURA_MOD_AOE_AVOIDANCE aura
    if (IsAreaOfEffectSpell(spell))
        hitChance -=
            pVictim->GetTotalAuraModifier(SPELL_AURA_MOD_AOE_AVOIDANCE);
    // Reduce spell hit chance for dispel mechanic spells from victim
    // SPELL_AURA_MOD_DISPEL_RESIST
    if (IsDispelSpell(spell))
        hitChance -=
            pVictim->GetTotalAuraModifier(SPELL_AURA_MOD_DISPEL_RESIST);

    // Chance resist debuff
    hitChance -= pVictim->GetTotalAuraModifierByMiscValue(
        SPELL_AURA_MOD_DEBUFF_RESISTANCE, int32(spell->Dispel));

    // Increase hit chance from attacker SPELL_AURA_MOD_SPELL_HIT_CHANCE and
    // attacker ratings
    hitChance += m_modSpellHitChance;

    // Decrease hit chance from victim rating bonus
    if (pVictim->GetTypeId() == TYPEID_PLAYER)
        hitChance -=
            ((Player*)pVictim)->GetRatingBonusValue(CR_HIT_TAKEN_SPELL);

    // Your spells hit chance must be in the range 1...99 %. You always have
    // at
    // least 1% chance to miss, or 1% chance to hit.
    if (hitChance < 1.0f)
        hitChance = 1.0f;
    else if (hitChance > 99.0f)
        hitChance = 99.0f;
    float missChance = 100.0f - hitChance;

    return missChance;
}

SpellMissInfo Unit::MagicSpellHitResult(Unit* pVictim, SpellEntry const* spell)
{
    // Can`t miss on dead target (on skinning for example)
    if (!pVictim->isAlive())
        return SPELL_MISS_NONE;

    // Attack misses if rand is below missvalue
    uint32 missvalue = spell->HasAttribute(SPELL_ATTR_EX3_CANT_MISS) ?
                           0 :
                           uint32(SpellMissChanceCalc(pVictim, spell) * 100);
    uint32 rand = urand(0, 9999);

    if (rand < missvalue)
        return SPELL_MISS_RESIST;

    // Binary Resistance
    if (pVictim->roll_binary_spell(this, spell))
        return SPELL_MISS_RESIST;

    return SPELL_MISS_NONE;
}

// Calculate spell hit result can be:
// Every spell can: Evade/Immune/Reflect/Sucesful hit
// For melee based spells:
//   Miss
//   Dodge
//   Parry
// For spells
//   Resist
SpellMissInfo Unit::SpellHitResult(
    Unit* pVictim, SpellEntry const* spell, bool CanReflect, bool was_reflected)
{
    // Spells on ourselves that weren't reflected always hit
    if (!was_reflected && pVictim == this)
        return SPELL_MISS_NONE;

    // Return evade for units in evade mode
    if (pVictim->GetTypeId() == TYPEID_UNIT &&
        static_cast<Creature*>(pVictim)->IsInEvadeMode())
        return SPELL_MISS_EVADE;

    // Increased chance to miss for 60+ spells
    if (spell->HasAttribute(SPELL_ATTR_CUSTOM_RESIST_OVER_60))
    {
        uint32 level = pVictim->getLevel();
        if (level > 60)
        {
            if (roll_chance_i(4 * (level - 60)))
                return SPELL_MISS_RESIST;
        }
    }

    // Positive spells can't miss. But some positive spells (dispel magic,
    // purge) can be used offensively
    // TODO: client not show miss log for this spells - so need find info
    // for
    // this in dbc and use it!
    if (IsPositiveSpell(spell->Id) &&
        !(IsDispelSpell(spell) && pVictim->IsHostileTo(this)))
        return SPELL_MISS_NONE;

    // Mechanic resistance (in the case where the entire spell has a
    // mechanic)
    if (spell->Mechanic)
    {
        int32 resistChance = pVictim->GetTotalAuraModifierByMiscValue(
            SPELL_AURA_MOD_MECHANIC_RESISTANCE, spell->Mechanic);
        if (resistChance > 0)
        {
            if (urand(1, 100) <= static_cast<uint32>(resistChance))
                return SPELL_MISS_RESIST;
        }
    }

    // Try victim reflect spell
    if (CanReflect)
    {
        int32 reflectchance =
            pVictim->GetTotalAuraModifier(SPELL_AURA_REFLECT_SPELLS);
        const Unit::Auras& reflectSpells =
            pVictim->GetAurasByType(SPELL_AURA_REFLECT_SPELLS_SCHOOL);
        for (const auto& reflectSpell : reflectSpells)
            if ((reflectSpell)->GetModifier()->m_miscvalue &
                GetSpellSchoolMask(spell))
                reflectchance += (reflectSpell)->GetModifier()->m_amount;
        if (reflectchance > 0 && roll_chance_i(reflectchance))
        {
            return SPELL_MISS_REFLECT;
        }
    }

    switch (spell->DmgClass)
    {
    case SPELL_DAMAGE_CLASS_NONE:
        return SPELL_MISS_NONE;
    case SPELL_DAMAGE_CLASS_MAGIC:
        return MagicSpellHitResult(pVictim, spell);
    case SPELL_DAMAGE_CLASS_MELEE:
    case SPELL_DAMAGE_CLASS_RANGED:
        return MeleeSpellHitResult(pVictim, spell);
    }
    return SPELL_MISS_NONE;
}

// returns: mask of effects that unit is still not immune to
uint32 Unit::SpellImmunityCheck(const SpellEntry* info, uint32 effectMask)
{
    if (effectMask == 0)
        return 0;

    if (info->HasAttribute(SPELL_ATTR_UNAFFECTED_BY_INVULNERABILITY) ||
        info->HasAttribute(SPELL_ATTR_PASSIVE))
        return effectMask;

    if (IsImmuneToSpell(info))
        return 0;

    // Full CC spells on the target should simply say Immune if she's flying on
    // a flying mount
    if (GetTypeId() == TYPEID_PLAYER &&
        m_movementInfo.HasMovementFlag(MOVEFLAG_FLYING2) &&
        HasAuraType(SPELL_AURA_MOUNTED) &&
        (info->HasApplyAuraName(SPELL_AURA_MOD_STUN) ||
            info->HasApplyAuraName(SPELL_AURA_MOD_CONFUSE) ||
            info->HasApplyAuraName(SPELL_AURA_MOD_CHARM) ||
            info->HasApplyAuraName(SPELL_AURA_MOD_FEAR) ||
            info->HasApplyAuraName(SPELL_AURA_MOD_ROOT)) &&
        !IsDamagingSpell(info))
    {
        return 0;
    }

    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if ((effectMask & (1 << i)) == 0)
            continue;
        if (IsDamagingEffect(info, SpellEffectIndex(i)) &&
            IsImmunedToDamage(GetSpellSchoolMask(info)))
            effectMask &= ~(1 << i);
        else if (IsImmuneToSpellEffect(info, SpellEffectIndex(i)))
            effectMask &= ~(1 << i);
    }

    return effectMask;
}

float Unit::MeleeMissChanceCalc(
    const Unit* pVictim, WeaponAttackType attType) const
{
    if (!pVictim)
        return 0.0f;

    // Base misschance 5%
    float missChance = 5.0f;

    // DualWield - white damage has additional 19% miss penalty
    if (haveOffhandWeapon() && attType != RANGED_ATTACK)
    {
        bool isNormal = false;
        for (uint32 i = CURRENT_FIRST_NON_MELEE_SPELL; i < CURRENT_MAX_SPELL;
             ++i)
        {
            if (m_currentSpells[i] &&
                (GetSpellSchoolMask(m_currentSpells[i]->m_spellInfo) &
                    SPELL_SCHOOL_MASK_NORMAL))
            {
                isNormal = true;
                break;
            }
        }
        if (!isNormal && !m_currentSpells[CURRENT_MELEE_SPELL])
            missChance += 19.0f;
    }

    int32 skillDiff = int32(GetWeaponSkillValue(attType, pVictim)) -
                      int32(pVictim->GetDefenseSkillValue(this));

    // PvP - PvE melee chances
    if (pVictim->GetTypeId() == TYPEID_PLAYER)
        missChance -= skillDiff * 0.04f;
    else if (skillDiff < -10)
        missChance -=
            (skillDiff + 10) * 0.4f -
            2.0f; // 7% base chance to miss for big skill diff (%6 in 3.x)
    else
        missChance -= skillDiff * 0.1f;

    // Hit chance bonus from attacker based on ratings and auras
    if (attType == RANGED_ATTACK)
        missChance -= m_modRangedHitChance;
    else
        missChance -= m_modMeleeHitChance;

    // Hit chance for victim based on ratings
    if (pVictim->GetTypeId() == TYPEID_PLAYER)
    {
        if (attType == RANGED_ATTACK)
            missChance +=
                ((Player*)pVictim)->GetRatingBonusValue(CR_HIT_TAKEN_RANGED);
        else
            missChance +=
                ((Player*)pVictim)->GetRatingBonusValue(CR_HIT_TAKEN_MELEE);
    }

    // Modify miss chance by victim auras
    if (attType == RANGED_ATTACK)
        missChance -= pVictim->GetTotalAuraModifier(
            SPELL_AURA_MOD_ATTACKER_RANGED_HIT_CHANCE);
    else
        missChance -= pVictim->GetTotalAuraModifier(
            SPELL_AURA_MOD_ATTACKER_MELEE_HIT_CHANCE);

    // Limit miss chance from 0 to 60%
    if (missChance < 0.0f)
        return 0.0f;
    if (missChance > 60.0f)
        return 60.0f;

    return missChance;
}

uint32 Unit::GetDefenseSkillValue(Unit const* target) const
{
    if (GetTypeId() == TYPEID_PLAYER)
    {
        // in PvP use full skill instead current skill value
        uint32 value = (target && target->GetTypeId() == TYPEID_PLAYER) ?
                           ((Player*)this)->GetMaxSkillValue(SKILL_DEFENSE) :
                           ((Player*)this)->GetSkillValue(SKILL_DEFENSE);
        value += uint32(((Player*)this)->GetRatingBonusValue(CR_DEFENSE_SKILL));
        return value;
    }
    else
        return GetUnitMeleeSkill(target);
}

float Unit::GetUnitDodgeChance() const
{
    if (hasUnitState(UNIT_STAT_CAN_NOT_REACT))
        return 0.0f;
    if (GetTypeId() == TYPEID_PLAYER)
        return GetFloatValue(PLAYER_DODGE_PERCENTAGE);
    else
    {
        if (((Creature const*)this)->IsTotem())
            return 0.0f;
        else
        {
            float dodge = 5.0f;
            dodge += GetTotalAuraModifier(SPELL_AURA_MOD_DODGE_PERCENT);
            return dodge > 0.0f ? dodge : 0.0f;
        }
    }
}

float Unit::GetUnitParryChance() const
{
    if (IsNonMeleeSpellCasted(false) || hasUnitState(UNIT_STAT_CAN_NOT_REACT))
        return 0.0f;

    float chance = 0.0f;

    if (GetTypeId() == TYPEID_PLAYER)
    {
        Player const* player = (Player const*)this;
        if (player->CanParry())
        {
            Item* tmpitem = player->GetWeaponForAttack(BASE_ATTACK, true, true);
            if (!tmpitem)
                tmpitem = player->GetWeaponForAttack(OFF_ATTACK, true, true);

            if (tmpitem)
                chance = GetFloatValue(PLAYER_PARRY_PERCENTAGE);
        }
    }
    else if (GetTypeId() == TYPEID_UNIT)
    {
        chance = 5.0f;
        chance += GetTotalAuraModifier(SPELL_AURA_MOD_PARRY_PERCENT);
    }

    return chance > 0.0f ? chance : 0.0f;
}

float Unit::GetUnitBlockChance() const
{
    if (IsNonMeleeSpellCasted(false) || hasUnitState(UNIT_STAT_CAN_NOT_REACT))
        return 0.0f;

    if (GetTypeId() == TYPEID_PLAYER)
    {
        Player const* player = (Player const*)this;
        if (player->CanBlock() && player->CanUseEquippedWeapon(OFF_ATTACK))
        {
            // XXX
            Item* item =
                player->storage().get(inventory::slot(inventory::personal_slot,
                    inventory::main_bag, inventory::off_hand_e));
            if (item && !item->IsBroken() && item->GetProto()->Block)
                return GetFloatValue(PLAYER_BLOCK_PERCENTAGE);
        }
        // is player but has no block ability or no not broken shield
        // equipped
        return 0.0f;
    }
    else
    {
        if (((Creature const*)this)->IsTotem())
            return 0.0f;
        else
        {
            float block = 5.0f;
            block += GetTotalAuraModifier(SPELL_AURA_MOD_BLOCK_PERCENT);
            return block > 0.0f ? block : 0.0f;
        }
    }
}

float Unit::GetUnitCriticalChance(
    WeaponAttackType attackType, const Unit* pVictim) const
{
    float crit;

    if (GetTypeId() == TYPEID_PLAYER)
    {
        switch (attackType)
        {
        case BASE_ATTACK:
            crit = GetFloatValue(PLAYER_CRIT_PERCENTAGE);
            break;
        case OFF_ATTACK:
            crit = GetFloatValue(PLAYER_OFFHAND_CRIT_PERCENTAGE);
            break;
        case RANGED_ATTACK:
            crit = GetFloatValue(PLAYER_RANGED_CRIT_PERCENTAGE);
            break;
        // Just for good manner
        default:
            crit = 0.0f;
            break;
        }
    }
    else
    {
        crit = 5.0f;
        crit += GetTotalAuraModifier(SPELL_AURA_MOD_CRIT_PERCENT);
    }

    // Don't apply mods for self-casts
    if (this == pVictim)
        return crit > 0.0f ? crit : 0.0f;

    // flat aura mods
    if (attackType == RANGED_ATTACK)
        crit += pVictim->GetTotalAuraModifier(
            SPELL_AURA_MOD_ATTACKER_RANGED_CRIT_CHANCE);
    else
        crit += pVictim->GetTotalAuraModifier(
            SPELL_AURA_MOD_ATTACKER_MELEE_CRIT_CHANCE);

    crit += pVictim->GetTotalAuraModifier(
        SPELL_AURA_MOD_ATTACKER_SPELL_AND_WEAPON_CRIT_CHANCE);

    // reduce crit chance from Rating for players
    if (pVictim->GetTypeId() == TYPEID_PLAYER)
    {
        if (attackType == RANGED_ATTACK)
            crit -=
                ((Player*)pVictim)->GetRatingBonusValue(CR_CRIT_TAKEN_RANGED);
        else
            crit -=
                ((Player*)pVictim)->GetRatingBonusValue(CR_CRIT_TAKEN_MELEE);
    }

    // Apply crit chance from defence skill
    crit += (int32(GetMaxSkillValueForLevel(pVictim)) -
                int32(pVictim->GetDefenseSkillValue(this))) *
            0.04f;

    if (crit < 0.0f)
        crit = 0.0f;
    return crit;
}

uint32 Unit::GetWeaponSkillValue(
    WeaponAttackType attType, Unit const* target) const
{
    uint32 value = 0;
    if (GetTypeId() == TYPEID_PLAYER)
    {
        Item* item = ((Player*)this)->GetWeaponForAttack(attType, true, true);

        // feral or unarmed skill only for base attack
        if (attType != BASE_ATTACK && !item)
            return 0;

        if (IsInFeralForm())
            return GetMaxSkillValueForLevel(); // always maximized
                                               // SKILL_FERAL_COMBAT in fact

        // weapon skill or (unarmed for base attack)
        uint32 skill =
            item ? item->GetSkill() : static_cast<uint32>(SKILL_UNARMED);

        // in PvP use full skill instead current skill value
        if (target && (target->GetTypeId() == TYPEID_PLAYER ||
                          static_cast<const Creature*>(target)->IsPlayerPet()))
            value = static_cast<const Player*>(this)->GetMaxSkillValue(skill);
        else
            value = static_cast<const Player*>(this)->GetSkillValue(skill);

        // Modify value from ratings
        value += uint32(((Player*)this)->GetRatingBonusValue(CR_WEAPON_SKILL));
        switch (attType)
        {
        case BASE_ATTACK:
            value += uint32(
                ((Player*)this)->GetRatingBonusValue(CR_WEAPON_SKILL_MAINHAND));
            break;
        case OFF_ATTACK:
            value += uint32(
                ((Player*)this)->GetRatingBonusValue(CR_WEAPON_SKILL_OFFHAND));
            break;
        case RANGED_ATTACK:
            value += uint32(
                ((Player*)this)->GetRatingBonusValue(CR_WEAPON_SKILL_RANGED));
            break;
        }
    }
    else
        value = GetUnitMeleeSkill(target);
    return value;
}

void Unit::_UpdateSpells(uint32 time)
{
    if (m_currentSpells[CURRENT_AUTOREPEAT_SPELL])
        _UpdateAutoRepeatSpell();
    else
        m_AutoRepeatFirstCast = true;

    // remove finished spells from current pointers
    for (auto& elem : m_currentSpells)
    {
        if (elem && elem->getState() == SPELL_STATE_FINISHED)
        {
            elem->SetReferencedFromCurrent(false);
            elem = nullptr; // remove pointer
        }
    }

    {
        aura_holder_guard guard(this);

        for (auto& elem : m_auraHolders)
        {
            AuraHolder* holder = elem.second;
            if (holder->IsDisabled())
                continue;

            holder->Update(time);

            // Holder can be disabled by AuraHolder::Update
            if (holder->IsDisabled())
                continue;

            // Remove aura if it just expired
            if (!(holder->IsPermanent() || holder->IsPassive()) &&
                holder->GetAuraDuration() == 0)
                RemoveAuraHolder(holder, AURA_REMOVE_BY_EXPIRE);
        }

        // Queued aura holders added when scope ends
    }

    // Cleanup auras that have been disabled
    CleanDisabledAuras();

    if (!m_gameObj.empty())
    {
        GameObjectList::iterator ite1, dnext1;
        for (ite1 = m_gameObj.begin(); ite1 != m_gameObj.end(); ite1 = dnext1)
        {
            dnext1 = ite1;
            //(*i)->Update( difftime );
            if (!(*ite1)->isSpawned())
            {
                (*ite1)->SetOwnerGuid(ObjectGuid());
                (*ite1)->SetRespawnTime(0);
                (*ite1)->Delete();
                dnext1 = m_gameObj.erase(ite1);
            }
            else
                ++dnext1;
        }
    }
}

void Unit::CleanDisabledAuras()
{
    for (auto itr = m_auraHolders.begin(); itr != m_auraHolders.end();)
    {
        if (itr->second->IsDisabled())
        {
            delete itr->second;
            itr = m_auraHolders.erase(itr);
        }
        else
        {
            ++itr;
        }
    }
}

void Unit::_UpdateAutoRepeatSpell()
{
    // check "realtime" interrupts
    if ((GetTypeId() == TYPEID_PLAYER && ((Player*)this)->isMoving()) ||
        IsNonMeleeSpellCasted(false, false, true))
    {
        // cancel wand shoot
        if (m_currentSpells[CURRENT_AUTOREPEAT_SPELL]->m_spellInfo->Category ==
            351)
            InterruptSpell(CURRENT_AUTOREPEAT_SPELL);
        // cancel pending steady shot
        static_cast<Player*>(this)->pending_steady_shot = false;
        m_AutoRepeatFirstCast = true;
        return;
    }

    // apply delay
    if (m_AutoRepeatFirstCast && getAttackTimer(RANGED_ATTACK) < 500)
        setAttackTimer(RANGED_ATTACK, 500);
    m_AutoRepeatFirstCast = false;

    // castroutine
    if (isAttackReady(RANGED_ATTACK))
    {
        // Check if able to cast
        if (m_currentSpells[CURRENT_AUTOREPEAT_SPELL]->CheckCast(true) !=
            SPELL_CAST_OK)
        {
            InterruptSpell(CURRENT_AUTOREPEAT_SPELL);
            return;
        }

        // we want to shoot
        auto info = m_currentSpells[CURRENT_AUTOREPEAT_SPELL]->m_spellInfo;
        auto spell = new Spell(this, info, true);

        // Shoot wand automatically switches target if you swap selection
        spell->m_targets = m_currentSpells[CURRENT_AUTOREPEAT_SPELL]->m_targets;
        if (GetTypeId() == TYPEID_PLAYER && info->Id == 5019 &&
            spell->m_targets.getUnitTargetGuid() !=
                static_cast<Player*>(this)->GetSelectionGuid())
        {
            if (auto unit = GetMap()->GetUnit(
                    static_cast<Player*>(this)->GetSelectionGuid()))
                spell->m_targets.setUnitTarget(unit);
            // Check cast for new target
            auto res = spell->CheckCast(true);
            // FIXME: Spell system doesn't reject this hostile cast on ourself
            if (res == SPELL_CAST_OK &&
                static_cast<Player*>(this)->GetSelectionGuid() ==
                    GetObjectGuid())
                res = SPELL_FAILED_BAD_TARGETS;
            // Range not checked on triggered spells, check manually
            if (res == SPELL_CAST_OK)
                res = spell->CheckRange(true);
            if (res != SPELL_CAST_OK)
            {
                spell->SendCastResult(res);
                InterruptSpell(CURRENT_AUTOREPEAT_SPELL);
                delete spell;
                return;
            }
        }

        spell->prepare(&spell->m_targets);

        // all went good, reset attack
        resetAttackTimer(RANGED_ATTACK);
    }
}

void Unit::SetCurrentCastedSpell(Spell* pSpell)
{
    assert(pSpell); // NULL may be never passed here, use InterruptSpell or
                    // InterruptNonMeleeSpells

    CurrentSpellTypes CSpellType = pSpell->GetCurrentContainer();

    // avoid breaking self
    if (pSpell == m_currentSpells[CSpellType])
        return;

    // break same type spell if it is not delayed
    InterruptSpell(CSpellType, false, pSpell);

    // special breakage effects:
    switch (CSpellType)
    {
    case CURRENT_GENERIC_SPELL:
    {
        // generic spells always break channeled not delayed spells
        InterruptSpell(CURRENT_CHANNELED_SPELL, false);

        // autorepeat breaking
        if (m_currentSpells[CURRENT_AUTOREPEAT_SPELL])
        {
            // break autorepeat if not Auto Shot
            if (m_currentSpells[CURRENT_AUTOREPEAT_SPELL]
                    ->m_spellInfo->Category == 351)
                InterruptSpell(CURRENT_AUTOREPEAT_SPELL);
            if (!pSpell->IsInstant() ||
                pSpell->m_spellInfo->StartRecoveryTime > 0)
                m_AutoRepeatFirstCast = true; // delay auto-shot on GCD cast
        }
    }
    break;

    case CURRENT_CHANNELED_SPELL:
    {
        // channel spells always break generic non-delayed and any channeled
        // spells
        InterruptSpell(CURRENT_GENERIC_SPELL, false);
        InterruptSpell(CURRENT_CHANNELED_SPELL, true,
            pSpell); // TODO: Does there really exist delayed channeled
                     // spells?

        // it also does break autorepeat if not Auto Shot
        if (m_currentSpells[CURRENT_AUTOREPEAT_SPELL] &&
            m_currentSpells[CURRENT_AUTOREPEAT_SPELL]->m_spellInfo->Category ==
                351)
            InterruptSpell(CURRENT_AUTOREPEAT_SPELL);
    }
    break;

    case CURRENT_AUTOREPEAT_SPELL:
    {
        // only Auto Shoot does not break anything
        if (pSpell->m_spellInfo->Category == 351)
        {
            // generic autorepeats break generic non-delayed and channeled
            // non-delayed spells
            InterruptSpell(CURRENT_GENERIC_SPELL, false);
            InterruptSpell(CURRENT_CHANNELED_SPELL, false);
        }
        // special action: set first cast flag
        m_AutoRepeatFirstCast = true;
    }
    break;

    default:
    {
        // other spell types don't break anything now
    }
    break;
    }

    // With instant spells we don't save them in a current spells slot,
    // instead
    // we execute them right away
    if (!pSpell->IsInstant())
    {
        // current spell (if it is still here) may be safely deleted now
        if (m_currentSpells[CSpellType])
            m_currentSpells[CSpellType]->SetReferencedFromCurrent(false);

        // set new current spell
        m_currentSpells[CSpellType] = pSpell;
        pSpell->SetReferencedFromCurrent(true);

        pSpell->m_selfContainer =
            &(m_currentSpells[pSpell->GetCurrentContainer()]);
    }
}

void Unit::InterruptSpell(CurrentSpellTypes spellType, bool withDelayed,
    Spell* replacedBy, bool send_autorepeat)
{
    assert(spellType < CURRENT_MAX_SPELL);

    Spell* spell = m_currentSpells[spellType];
    if (spell && (withDelayed || spell->getState() != SPELL_STATE_DELAYED))
    {
        m_currentSpells[spellType] = nullptr;

        // send autorepeat cancel message for autorepeat spells
        if (send_autorepeat && spellType == CURRENT_AUTOREPEAT_SPELL &&
            GetTypeId() == TYPEID_PLAYER)
            static_cast<Player*>(this)->SendAutoRepeatCancel();

        if (spell->getState() != SPELL_STATE_FINISHED)
            spell->cancel(replacedBy);

        spell->SetReferencedFromCurrent(false);
    }
}

void Unit::FinishSpell(CurrentSpellTypes spellType, bool ok /*= true*/)
{
    Spell* spell = m_currentSpells[spellType];
    if (!spell)
        return;

    if (spellType == CURRENT_CHANNELED_SPELL)
        spell->SendChannelUpdate(0);

    spell->finish(ok);
}

void Unit::InterruptSpellOn(Unit* target)
{
    for (int i = CURRENT_GENERIC_SPELL; i < CURRENT_MAX_SPELL; ++i)
    {
        Spell* spell = m_currentSpells[i];
        if (!spell)
            continue;

        if (IsAreaOfEffectSpell(spell->m_spellInfo) ||
            !(spell->getState() == SPELL_STATE_PREPARING ||
                (IsChanneledSpell(spell->m_spellInfo) &&
                    spell->getState() == SPELL_STATE_CASTING)))
            continue;

        if (spell->m_targets.getUnitTargetGuid() == target->GetObjectGuid())
            spell->cancel();
    }
}

bool Unit::IsNonMeleeSpellCasted(
    bool withDelayed, bool skipChanneled, bool skipAutorepeat) const
{
    // We don't do loop here to explicitly show that melee spell is
    // excluded.
    // Maybe later some special spells will be excluded too.

    // generic spells are casted when they are not finished and not delayed
    if (m_currentSpells[CURRENT_GENERIC_SPELL] &&
        (m_currentSpells[CURRENT_GENERIC_SPELL]->getState() !=
            SPELL_STATE_FINISHED) &&
        (withDelayed ||
            m_currentSpells[CURRENT_GENERIC_SPELL]->getState() !=
                SPELL_STATE_DELAYED))
        return true;

    // channeled spells may be delayed, but they are still considered casted
    else if (!skipChanneled && m_currentSpells[CURRENT_CHANNELED_SPELL] &&
             (m_currentSpells[CURRENT_CHANNELED_SPELL]->getState() !=
                 SPELL_STATE_FINISHED))
        return true;

    // autorepeat spells may be finished or delayed, but they are still
    // considered casted
    else if (!skipAutorepeat && m_currentSpells[CURRENT_AUTOREPEAT_SPELL])
        return true;

    return false;
}

bool Unit::IsCastedSpellPreventingMovementOrAttack() const
{
    if (auto gen = m_currentSpells[CURRENT_GENERIC_SPELL])
    {
        if (gen->m_spellInfo->CastingTimeIndex > 1 && // ignore instant spells
            gen->getState() != SPELL_STATE_FINISHED &&
            gen->getState() != SPELL_STATE_DELAYED &&
            gen->m_spellInfo->InterruptFlags &
                (SPELL_INTERRUPT_FLAG_MOVEMENT |
                    SPELL_INTERRUPT_FLAG_AUTOATTACK))
            return true;
    }
    if (auto channel = m_currentSpells[CURRENT_CHANNELED_SPELL])
    {
        if (channel->getState() != SPELL_STATE_FINISHED &&
            channel->m_spellInfo->ChannelInterruptFlags &
                (CHANNEL_FLAG_MOVEMENT | CHANNEL_FLAG_TURNING))
            return true;
    }
    if (auto repeat = m_currentSpells[CURRENT_AUTOREPEAT_SPELL])
    {
        if (repeat->m_spellInfo->InterruptFlags & SPELL_INTERRUPT_FLAG_MOVEMENT)
            return true;
    }
    return false;
}

void Unit::InterruptNonMeleeSpells(bool withDelayed, uint32 spell_id)
{
    // generic spells are interrupted if they are not finished or delayed
    if (m_currentSpells[CURRENT_GENERIC_SPELL] &&
        (!spell_id ||
            m_currentSpells[CURRENT_GENERIC_SPELL]->m_spellInfo->Id ==
                spell_id))
        InterruptSpell(CURRENT_GENERIC_SPELL, withDelayed);

    // autorepeat spells are interrupted if they are not finished or delayed
    if (m_currentSpells[CURRENT_AUTOREPEAT_SPELL] &&
        (!spell_id ||
            m_currentSpells[CURRENT_AUTOREPEAT_SPELL]->m_spellInfo->Id ==
                spell_id))
        InterruptSpell(CURRENT_AUTOREPEAT_SPELL, withDelayed);

    // channeled spells are interrupted if they are not finished, even if
    // they
    // are delayed
    if (m_currentSpells[CURRENT_CHANNELED_SPELL] &&
        (!spell_id ||
            m_currentSpells[CURRENT_CHANNELED_SPELL]->m_spellInfo->Id ==
                spell_id))
        InterruptSpell(CURRENT_CHANNELED_SPELL, true);
}

Spell* Unit::FindCurrentSpellBySpellId(uint32 spell_id) const
{
    for (auto& elem : m_currentSpells)
        if (elem && elem->m_spellInfo->Id == spell_id)
            return elem;
    return nullptr;
}

void Unit::SetInFront(Unit const* target)
{
    if (!hasUnitState(UNIT_STAT_CANNOT_ROTATE))
        SetOrientation(GetAngle(target));
}

void Unit::SetFacingTo(float ori)
{
    movement_gens.push(new movement::FaceMovementGenerator(ori));
}

void Unit::SetFacingToObject(WorldObject* pObject)
{
    // never face when already moving
    if (!IsStopped())
        return;

    // TODO: figure out under what conditions creature will move towards
    // object
    // instead of facing it where it currently is.
    SetFacingTo(GetAngle(pObject));
}

bool Unit::isInAccessablePlaceFor(Creature const* c) const
{
    if (IsInWater())
        return c->CanSwim();
    else
        return c->CanWalk() || c->CanFly();
}

GridMapLiquidStatus Unit::GetLiquidStatus(
    uint8 ReqType, GridMapLiquidData* out_data) const
{
    return GetLiquidStatus(GetX(), GetY(), GetZ(), ReqType, out_data);
}

// NOTE: only use if you're confident Unit is in same area as x,y,z
GridMapLiquidStatus Unit::GetLiquidStatus(
    float x, float y, float z, uint8 ReqType, GridMapLiquidData* out_data) const
{
    auto vmgr = VMAP::VMapFactory::createOrGetVMapManager();
    return GetTerrain()->getLiquidStatus(x, y, z, ReqType,
        vmgr->HasLiquidData(GetMapId(), GetAreaId()), out_data);
}

bool Unit::IsInWater() const
{
    auto status = GetLiquidStatus(MAP_ALL_LIQUIDS);
    return (status & (LIQUID_MAP_IN_WATER | LIQUID_MAP_UNDER_WATER)) != 0;
}

bool Unit::IsUnderWater() const
{
    auto status = GetLiquidStatus(MAP_ALL_LIQUIDS);
    return (status & LIQUID_MAP_UNDER_WATER) != 0;
}

void Unit::DeMorph()
{
    SetDisplayId(GetNativeDisplayId());
}

int32 Unit::GetTotalAuraModifier(AuraType auratype) const
{
    int32 modifier = 0;

    const Auras& auras = GetAurasByType(auratype);
    for (const auto& aura : auras)
        modifier += (aura)->GetModifier()->m_amount;

    return modifier;
}

float Unit::GetTotalAuraMultiplier(AuraType auratype) const
{
    float multiplier = 1.0f;

    const Auras& auras = GetAurasByType(auratype);
    for (const auto& aura : auras)
        multiplier *= (100.0f + (aura)->GetModifier()->m_amount) / 100.0f;

    return multiplier;
}

int32 Unit::GetMaxPositiveAuraModifier(AuraType auratype) const
{
    int32 modifier = 0;

    const Auras& auras = GetAurasByType(auratype);
    for (const auto& aura : auras)
        if ((aura)->GetModifier()->m_amount > modifier)
            modifier = (aura)->GetModifier()->m_amount;

    return modifier;
}

int32 Unit::GetMaxNegativeAuraModifier(AuraType auratype) const
{
    int32 modifier = 0;

    const Auras& auras = GetAurasByType(auratype);
    for (const auto& aura : auras)
        if ((aura)->GetModifier()->m_amount < modifier)
            modifier = (aura)->GetModifier()->m_amount;

    return modifier;
}

int32 Unit::GetTotalAuraModifierByMiscMask(
    AuraType auratype, uint32 misc_mask) const
{
    if (!misc_mask)
        return 0;

    int32 modifier = 0;

    const Auras& auras = GetAurasByType(auratype);
    for (const auto& aura : auras)
    {
        Modifier* mod = (aura)->GetModifier();
        if (mod->m_miscvalue & misc_mask)
            modifier += mod->m_amount;
    }
    return modifier;
}

float Unit::GetTotalAuraMultiplierByMiscMask(
    AuraType auratype, uint32 misc_mask) const
{
    if (!misc_mask)
        return 1.0f;

    float multiplier = 1.0f;

    const Auras& auras = GetAurasByType(auratype);
    for (const auto& aura : auras)
    {
        Modifier* mod = (aura)->GetModifier();
        if (mod->m_miscvalue & misc_mask)
            multiplier *= (100.0f + mod->m_amount) / 100.0f;
    }
    return multiplier;
}

int32 Unit::GetMaxPositiveAuraModifierByMiscMask(
    AuraType auratype, uint32 misc_mask) const
{
    if (!misc_mask)
        return 0;

    int32 modifier = 0;

    const Auras& auras = GetAurasByType(auratype);
    for (const auto& aura : auras)
    {
        Modifier* mod = (aura)->GetModifier();
        if (mod->m_miscvalue & misc_mask && mod->m_amount > modifier)
            modifier = mod->m_amount;
    }

    return modifier;
}

int32 Unit::GetMaxNegativeAuraModifierByMiscMask(
    AuraType auratype, uint32 misc_mask) const
{
    if (!misc_mask)
        return 0;

    int32 modifier = 0;

    const Auras& auras = GetAurasByType(auratype);
    for (const auto& aura : auras)
    {
        Modifier* mod = (aura)->GetModifier();
        if (mod->m_miscvalue & misc_mask && mod->m_amount < modifier)
            modifier = mod->m_amount;
    }

    return modifier;
}

int32 Unit::GetTotalAuraModifierByMiscValue(
    AuraType auratype, int32 misc_value) const
{
    int32 modifier = 0;

    const Auras& auras = GetAurasByType(auratype);
    for (const auto& aura : auras)
    {
        Modifier* mod = (aura)->GetModifier();
        if (mod->m_miscvalue == misc_value)
            modifier += mod->m_amount;
    }
    return modifier;
}

float Unit::GetTotalAuraMultiplierByMiscValue(
    AuraType auratype, int32 misc_value) const
{
    float multiplier = 1.0f;

    const Auras& auras = GetAurasByType(auratype);
    for (const auto& aura : auras)
    {
        Modifier* mod = (aura)->GetModifier();
        if (mod->m_miscvalue == misc_value)
            multiplier *= (100.0f + mod->m_amount) / 100.0f;
    }
    return multiplier;
}

int32 Unit::GetMaxPositiveAuraModifierByMiscValue(
    AuraType auratype, int32 misc_value) const
{
    int32 modifier = 0;

    const Auras& auras = GetAurasByType(auratype);
    for (const auto& aura : auras)
    {
        Modifier* mod = (aura)->GetModifier();
        if (mod->m_miscvalue == misc_value && mod->m_amount > modifier)
            modifier = mod->m_amount;
    }

    return modifier;
}

int32 Unit::GetMaxNegativeAuraModifierByMiscValue(
    AuraType auratype, int32 misc_value) const
{
    int32 modifier = 0;

    const Auras& auras = GetAurasByType(auratype);
    for (const auto& aura : auras)
    {
        Modifier* mod = (aura)->GetModifier();
        if (mod->m_miscvalue == misc_value && mod->m_amount < modifier)
            modifier = mod->m_amount;
    }

    return modifier;
}

void Unit::AddSingleTargetAura(AuraHolder* holder, ObjectGuid target)
{
    // returns GetId() again if no chain exists
    uint32 first_rank =
        sSpellMgr::Instance()->GetFirstSpellInChain(holder->GetId());

    ObjectGuid prev_target = m_singleTargetAuras[first_rank];

    if (prev_target == target)
        return;

    if (!prev_target.IsEmpty())
    {
        // Remove this aura (or any in its spell chain) from the previous
        // target
        if (Unit* tar = GetMap()->GetUnit(prev_target))
            tar->remove_auras_if([this, first_rank](AuraHolder* holder)
                {
                    return holder->GetCasterGuid() == GetObjectGuid() &&
                           first_rank ==
                               sSpellMgr::Instance()->GetFirstSpellInChain(
                                   holder->GetId());
                });
    }

    m_singleTargetAuras[first_rank] = target;
}

bool Unit::AddAuraHolder(AuraHolder* holder)
{
    const SpellEntry* aura_spell_info = holder->GetSpellProto();

    // Checks not related to stacking

    assert(holder->GetTarget() == this &&
           "Unit::AddAuraHolder: tried adding AuraHolder to wrong target");
#ifndef NDEBUG
    auto bounds = m_auraHolders.equal_range(holder->GetId());
    for (auto itr = bounds.first; itr != bounds.second; ++itr)
        assert(holder != itr->second &&
               "Unit::AddAuraHolder: AuraHolder added twice");
#endif

    // Dead casters can only have death only / death persistent spells
    if (!isAlive() && !IsDeathPersistentSpell(aura_spell_info) &&
        !IsDeathOnlySpell(aura_spell_info) &&
        !(GetTypeId() == TYPEID_PLAYER &&
            ((Player*)this)->GetSession()->PlayerLoading())) // Ignore if player
                                                             // is in login
                                                             // (TODO: Why?
                                                             // Mangos did this
                                                             // with a note that
                                                             // it will be
                                                             // cleaned up after
                                                             // login)
    {
        delete holder;
        return false;
    }

    // Stacking checks

    Unit* caster = holder->GetCaster();
    if (!caster)
        caster = this; // Caster is no longer valid, consider ourselves caster
                       // for bufff stacking

    std::pair<bool, std::set<AuraHolder*>> result =
        buff_stacks(aura_spell_info, caster, this, nullptr, holder)();
    if (!result.first)
    {
        // We can't add our buff; it did not pass stacking
        delete holder;
        return false;
    }

    if (aura_spell_info->StackAmount)
    {
        AuraHolder* add_to = nullptr;
        // Stacking spells
        if (aura_spell_info->HasAttribute(SPELL_ATTR_EX3_SEPARATE_STACKS))
        {
            // Append to our current holder if the rank is the same, otherwise
            // remove that rank
            if (!result.second.empty() &&
                (*result.second.begin())->GetId() == aura_spell_info->Id)
            {
                // Only our aura will be present in remove list
                add_to = *result.second.begin();
                result.second.erase(result.second.begin()); // Don't remove ours
            }
        }
        else
        {
            // Non-separate stacks, find a holder for this spell and add to
            // it
            add_to = get_aura(aura_spell_info->Id);
            result.second.clear(); // Max one conflicting (which'd be add_to)
        }

        if (add_to)
        {
            if (add_to->GetCaster() &&
                !aura_spell_info->HasAttribute(
                    SPELL_ATTR_CUSTOM_REFRESH_MODIFIERS))
            {
                // Add stacks to already existing holder
                add_to->ModStackAmount(holder->GetStackAmount());
                delete holder;
                return true;
            }
            else
            {
                // Original Caster has become invalid, add this holder with
                // ourselves as the new caster
                holder->SetPreferredAuraSlot(add_to->GetAuraSlot());
                holder->ModStackAmount(add_to->GetStackAmount(), true);
                RemoveAuraHolder(add_to);
            }
        }
    }

    // We passed stacking checks, time to apply ourselves

    // Save current health and mana before removing buffs
    holder->SetSavedHpMp(std::make_pair(GetHealth(), GetPower(POWER_MANA)));

    // Remove all spells that need to go for us to stay (as reported by class
    // buff_stacks)
    for (const auto& elem : result.second)
    {
        // This attribute makes it so that when overwriting a DoT with the same
        // DoT, the damage from the last DoT is added to the new (Mage's Ignite)
        if (aura_spell_info->HasAttribute(
                SPELL_ATTR_EX4_DOT_ADD_REMAINING_DAMAGE) &&
            aura_spell_info->Id == (elem)->GetId() &&
            aura_spell_info->HasApplyAuraName(SPELL_AURA_PERIODIC_DAMAGE))
        {
            SpellEffectIndex index = static_cast<SpellEffectIndex>(
                aura_spell_info->EffectApplyAuraName[0] ==
                        SPELL_AURA_PERIODIC_DAMAGE ?
                    0 :
                    aura_spell_info->EffectApplyAuraName[1] ==
                            SPELL_AURA_PERIODIC_DAMAGE ?
                    1 :
                    2);
            Aura* our = holder->GetAura(index);
            Aura* other = (elem)->GetAura(index);
            if (our && other && other->GetAuraDuration() > 0)
            {
                // Ticks do remain, transfer the damage the ticks would've
                // done.
                // FIXME: The transferred damage does not scale with previous
                // transfers, for example:
                // Aura1 -> Aura2 -> Aura3. Even if Aura1 had added damage to
                // Aura2, that is now lost in Aura3
                // This doesn't work currently
                int32 remaining_ticks =
                    other->GetAuraMaxTicks() - other->GetAuraTicks();
                int32 damage_transferred =
                    other->GetModifier()->m_amount * remaining_ticks;

                our->GetModifier()->m_amount +=
                    damage_transferred /
                    static_cast<int32>(our->GetAuraMaxTicks());
            }
        }

        (elem)->overwriting_aura(holder);
        RemoveAuraHolder(elem, AURA_REMOVE_BY_STACK);
    }

    if (guarding_holders_)
        m_queuedHolderAdds.emplace_back(holder);
    else
        FinalAddAuraHolder(holder, false);

    // NOTE: ApplyAuraModifiers can, by triggered spells, disable the current
    // holder; but that still means the adding was successful!
    return true;
}

void Unit::FinalAddAuraHolder(AuraHolder* holder, bool was_delayed)
{
    // If the addition was delayed we need to retest buff stacking, any
    // collision means we get dropped. This can only happen if multiple auras
    // are triggered during the deletion of an aura.
    if (was_delayed)
    {
        auto caster = holder->GetCaster();
        auto result = buff_stacks(holder->GetSpellProto(),
            caster ? caster : this, this, nullptr, holder)();
        if (!result.first || !result.second.empty())
        {
            delete holder;
            return;
        }
    }

    // Fully add the aura holder once it's passed all the checks, at this point
    // we are no longer responsible for cleanup
    holder->_AddAuraHolder();

    m_auraHolders.insert(AuraHolderMap::value_type(holder->GetId(), holder));

    if (holder->GetSpellProto()->AuraInterruptFlags)
    {
        interruptible_auras_.push_back(holder);
        interrupt_mask_ |= holder->GetSpellProto()->AuraInterruptFlags;
    }

    for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
        if (Aura* aur = holder->GetAura(SpellEffectIndex(i)))
            AddAuraToModList(aur);

    holder->ApplyAuraModifiers(true, true);

    // Aura can be deleted by triggered spells in ApplyAuraModifiers; if it has
    // we skip applying boosts
    if (holder->IsDisabled())
        return;

    holder->HandleSpellSpecificBoosts(true);

    // Clear out the saved hp/mp, it's only valid on application of the aura
    holder->SetSavedHpMp(std::make_pair(0, 0));

    // Register single-target spell at the caster
    if (holder->GetSpellProto()->HasAttribute(
            SPELL_ATTR_EX5_SINGLE_TARGET_SPELL))
        if (Unit* caster = holder->GetCaster())
            caster->AddSingleTargetAura(holder, GetObjectGuid());

    // If the state of the target changed in some way to make the aura invalid
    // to be applied (e.g. requires you to not be mounted, but the target is now
    // mounted) remove the aura holder after it finished applying fully
    if (!holder->valid_in_current_state(nullptr))
    {
        RemoveAuraHolder(holder);
        return;
    }

    // Multipart Auras
    if (auto multiparts =
            sSpellMgr::Instance()->GetMultipartAura(holder->GetId()))
    {
        if (auto caster = holder->GetCaster())
            for (auto& spell_id : *multiparts)
                AddAuraThroughNewHolder(
                    spell_id, caster, holder->GetAuraDuration());
    }
}

Unit::aura_holder_guard::aura_holder_guard(Unit* owner) : owner_{owner}
{
    ++owner_->guarding_holders_;
}

Unit::aura_holder_guard::~aura_holder_guard()
{
    if (owner_->guarding_holders_ > 0 && --owner_->guarding_holders_ != 0)
        return;

    auto copy = owner_->m_queuedHolderAdds;
    owner_->m_queuedHolderAdds.clear();

    for (auto& holder : copy)
        owner_->FinalAddAuraHolder(holder, true);
}

void Unit::RemoveAuraHolder(AuraHolder* holder, AuraRemoveMode mode)
{
    assert(!holder->IsDisabled() &&
           "Unit::RemoveAuraHolder called for already disabled holder");

#ifndef NDEBUG
    bool found = false;
    auto bounds = m_auraHolders.equal_range(holder->GetId());
    for (auto itr = bounds.first; itr != bounds.second; ++itr)
        if (itr->second == holder)
            found = true;
    assert(found && "Unit::RemoveAuraHolder: holder not present on the target");
#endif

    if (holder->GetSpellProto()->AuraInterruptFlags)
    {
        auto itr = std::find(
            interruptible_auras_.begin(), interruptible_auras_.end(), holder);
        if (itr != interruptible_auras_.end())
        {
            interruptible_auras_.erase(itr);
            update_interrupt_mask();
        }
    }

    // Disable auras and holder
    for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
        if (Aura* aura = holder->GetAura(SpellEffectIndex(i)))
            aura->Disable(mode, true); // invokes RemoveAuraFromModList
    holder->Disable();

    // Statue unsummoned at holder remove
    SpellEntry const* AurSpellInfo = holder->GetSpellProto();
    Totem* statue = nullptr;
    Unit* caster = holder->GetCaster();
    if (IsChanneledSpell(AurSpellInfo) && caster)
        if (caster->GetTypeId() == TYPEID_UNIT &&
            ((Creature*)caster)->IsTotem() &&
            ((Totem*)caster)->GetTotemType() == TOTEM_STATUE)
            statue = ((Totem*)caster);

    holder->SetRemoveMode(mode);

    holder->_RemoveAuraHolder();

    if (mode != AURA_REMOVE_BY_DELETE)
        holder->HandleSpellSpecificBoosts(false);

    if (statue)
        statue->UnSummon();

    if (mode != AURA_REMOVE_BY_EXPIRE && IsChanneledSpell(AurSpellInfo) &&
        !IsAreaOfEffectSpell(AurSpellInfo) &&
        !HasAreaAuraEffect(AurSpellInfo) && caster &&
        caster->GetObjectGuid() != GetObjectGuid())
    {
        caster->InterruptSpell(CURRENT_CHANNELED_SPELL);
    }

    // Multipart Auras
    if (auto multiparts =
            sSpellMgr::Instance()->GetMultipartAura(holder->GetId()))
    {
        for (auto& spell_id : *multiparts)
            remove_auras(spell_id, Unit::aura_no_op_true, mode);
    }
}

void Unit::AddAuraThroughNewHolder(uint32 spell_id, Unit* caster, int duration)
{
    const SpellEntry* info = sSpellStore.LookupEntry(spell_id);
    if (info)
    {
        auto is_aura_eff = [](uint8 eff)
        {
            return IsAreaAuraEffect(eff) || eff == SPELL_EFFECT_APPLY_AURA ||
                   eff == SPELL_EFFECT_PERSISTENT_AREA_AURA;
        };
        for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
            if (is_aura_eff(info->Effect[i]))
                goto has_aura_label;
        return;

    has_aura_label:
        AuraHolder* holder = CreateAuraHolder(info, this, caster);
        if (holder)
        {
            for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                uint8 eff = info->Effect[i];
                if (!is_aura_eff(eff))
                    continue;

                Aura* aura = CreateAura(
                    info, SpellEffectIndex(i), nullptr, holder, this, caster);
                holder->AddAura(aura, SpellEffectIndex(i));
            }
            if (duration > 0)
            {
                holder->SetAuraMaxDuration(duration);
                holder->SetAuraDuration(duration);
            }
            AddAuraHolder(holder);
        }
    }
}

void Unit::remove_auras_on_event(uint32 flags, uint32 ignore)
{
    if ((interrupt_mask_ & flags) == 0)
        return;

    std::vector<AuraHolder*> to_remove;
    for (auto itr = interruptible_auras_.begin();
         itr != interruptible_auras_.end();)
    {
        if ((*itr)->GetSpellProto()->AuraInterruptFlags & flags &&
            (ignore == 0 || ignore != (*itr)->GetId()))
        {
            auto holder = *itr;
            itr = interruptible_auras_.erase(itr);
            to_remove.push_back(holder);
        }
        else
        {
            ++itr;
        }
    }

    for (auto& holder : to_remove)
        RemoveAuraHolder(holder);
    if (!to_remove.empty())
        update_interrupt_mask();
}

void Unit::AddAuraToModList(Aura* aura)
{
    AuraType type = aura->GetModifier()->m_auraname;

    assert(type < TOTAL_AURAS &&
           "Unit::AddAuraToModList called with invalid aura name");

    auto itr =
        std::find(m_modAuras[type].begin(), m_modAuras[type].end(), aura);
    if (itr == m_modAuras[type].end())
        m_modAuras[type].push_back(aura);
}

void Unit::RemoveAuraFromModList(Aura* aura)
{
    AuraType type = aura->GetModifier()->m_auraname;

    assert(type < TOTAL_AURAS &&
           "Unit::AddAuraToModList called with invalid aura name");

    auto itr =
        std::find(m_modAuras[type].begin(), m_modAuras[type].end(), aura);
    if (itr != m_modAuras[type].end())
        m_modAuras[type].erase(itr);
}

void Unit::RemoveArenaAuras(bool onleave)
{
    // in join, remove positive buffs, on end, remove negative
    // used to remove positive visible auras in arenas
    remove_auras_if([onleave](AuraHolder* holder)
        {
            if (!holder->GetSpellProto()->HasAttribute(SPELL_ATTR_EX4_UNK21) &&
                // don't remove stances, shadowform, pally/hunter auras
                !holder->IsPassive() && // don't remove passive auras
                (!holder->GetSpellProto()->HasAttribute(
                     SPELL_ATTR_UNAFFECTED_BY_INVULNERABILITY) ||
                    !holder->GetSpellProto()->HasAttribute(SPELL_ATTR_UNK8)) &&
                // not unaffected by invulnerability auras or not having
                // that
                // unknown flag (that seemed the most probable)
                (holder->IsPositive() != onleave)) // remove positive buffs on
                                                   // enter, negative buffs on
                                                   // leave
            {
                return true;
            }
            return false;
        });
}

void Unit::DelaySpellAuraHolder(
    uint32 spellId, int32 delaytime, ObjectGuid casterGuid)
{
    loop_auras(
        [&](AuraHolder* holder)
        {
            if (casterGuid != holder->GetCasterGuid())
                return true; // continue

            if (holder->GetAuraDuration() < delaytime)
                holder->SetAuraDuration(0);
            else
                holder->SetAuraDuration(holder->GetAuraDuration() - delaytime);

            holder->UpdateAuraDuration();

            LOG_DEBUG(logging,
                "Spell %u partially interrupted on %s, new duration: %u ms",
                spellId, GetGuidStr().c_str(), holder->GetAuraDuration());
            return true; // continue
        },
        spellId);
}

void Unit::_RemoveAllAuraMods()
{
    loop_auras([](AuraHolder* holder)
        {
            holder->ApplyAuraModifiers(false);
            return true; // continue
        });
}

void Unit::_ApplyAllAuraMods()
{
    loop_auras([](AuraHolder* holder)
        {
            holder->ApplyAuraModifiers(true);
            return true; // continue
        });
}

bool Unit::has_aura(uint32 spell_id, AuraType type) const
{
    if (type != SPELL_AURA_NONE)
    {
        auto& al = GetAurasByType(type);
        for (auto aura : al)
            if (aura->GetId() == spell_id)
                return true;
        return false;
    }

    auto bounds = m_auraHolders.equal_range(spell_id);
    for (auto itr = bounds.first; itr != bounds.second; ++itr)
        if (!itr->second->IsDisabled())
            return true;
    return false;
}

bool Unit::HasAuraType(AuraType auraType) const
{
    return !GetAurasByType(auraType).empty();
}

void Unit::InformPetsAboutCC(const SpellEntry* spellInfo)
{
    if (spellInfo &&
        !(spellInfo->AuraInterruptFlags & AURA_INTERRUPT_FLAG_DAMAGE))
        return;

    const AttackerSet& set = getAttackers();
    std::vector<Creature*> pets; // Copy over our pets before we begin evading
                                 // them, as that'll remove them from the set
    for (const auto& elem : set)
    {
        if ((elem)->GetTypeId() == TYPEID_UNIT)
        {
            Creature* c = static_cast<Creature*>(elem);
            if (c->behavior() && !c->HasAuraType(SPELL_AURA_MOD_TAUNT))
            {
                // Do not stop if the pet just applied seduce (or seduce
                // will be
                // interrupted)
                if (spellInfo)
                {
                    if (!(spellInfo->Id == 6358 &&
                            c->GetCurrentSpell(CURRENT_CHANNELED_SPELL) &&
                            c->GetCurrentSpell(CURRENT_CHANNELED_SPELL)
                                    ->m_spellInfo->Id == 6358))
                        pets.push_back(c);
                }
                else
                {
                    pets.push_back(c);
                }
            }
        }
    }

    for (auto& pet : pets)
        (pet)->behavior()->evade();
}

bool Unit::HasBreakableByDamageAuraType(AuraType type) const
{
    const Auras& auras = GetAurasByType(type);
    for (const auto& aura : auras)
    {
        if (((aura)->GetSpellProto()->AuraInterruptFlags &
                AURA_INTERRUPT_FLAG_DAMAGE))
            return true;
    }
    return false;
}

bool Unit::HasBreakOnDamageCCAura() const
{
    return (HasBreakableByDamageAuraType(SPELL_AURA_MOD_CONFUSE) ||
            HasBreakableByDamageAuraType(SPELL_AURA_MOD_STUN));
}

uint32 Unit::GetAuraCount(uint32 spellId) const
{
    uint32 count = 0;
    for (auto itr = m_auraHolders.lower_bound(spellId);
         itr != m_auraHolders.upper_bound(spellId); ++itr)
    {
        if (itr->second->IsDisabled())
            continue;
        if (!itr->second->GetStackAmount())
            count++;
        else
            count += (uint32)itr->second->GetStackAmount();
    }
    return count;
}

bool Unit::HasAuraWithMechanic(uint32 mechanic_mask) const
{
    for (const auto& elem : m_auraHolders)
    {
        if (elem.second->IsDisabled())
            continue;
        if (GetAllSpellMechanicMask(elem.second->GetSpellProto()) &
            mechanic_mask)
            return true;
    }
    return false;
}

void Unit::AddDynObject(DynamicObject* dynObj)
{
    m_dynObjGUIDs.push_back(dynObj->GetObjectGuid());
}

void Unit::RemoveDynObject(uint32 spellid)
{
    if (m_dynObjGUIDs.empty())
        return;

    std::vector<DynamicObject*> del;

    for (auto i = m_dynObjGUIDs.begin(); i != m_dynObjGUIDs.end();)
    {
        DynamicObject* dynObj = GetMap()->GetDynamicObject(*i);
        if (!dynObj)
        {
            i = m_dynObjGUIDs.erase(i);
        }
        else if (spellid == 0 || dynObj->GetSpellId() == spellid)
        {
            del.push_back(dynObj);
            i = m_dynObjGUIDs.erase(i);
        }
        else
            ++i;
    }

    for (auto obj : del)
        obj->Delete();
}

void Unit::RemoveAllDynObjects()
{
    auto copy = m_dynObjGUIDs;

    m_dynObjGUIDs.clear();

    while (!copy.empty())
    {
        if (DynamicObject* dynObj = GetMap()->GetDynamicObject(*copy.begin()))
            dynObj->Delete();
        copy.erase(copy.begin());
    }
}

DynamicObject* Unit::GetDynObject(uint32 spellId, SpellEffectIndex effIndex)
{
    for (auto i = m_dynObjGUIDs.begin(); i != m_dynObjGUIDs.end();)
    {
        DynamicObject* dynObj = GetMap()->GetDynamicObject(*i);
        if (!dynObj)
        {
            i = m_dynObjGUIDs.erase(i);
            continue;
        }

        if (dynObj->GetSpellId() == spellId &&
            dynObj->GetEffIndex() == effIndex)
            return dynObj;
        ++i;
    }
    return nullptr;
}

DynamicObject* Unit::GetDynObject(uint32 spellId)
{
    for (auto i = m_dynObjGUIDs.begin(); i != m_dynObjGUIDs.end();)
    {
        DynamicObject* dynObj = GetMap()->GetDynamicObject(*i);
        if (!dynObj)
        {
            i = m_dynObjGUIDs.erase(i);
            continue;
        }

        if (dynObj->GetSpellId() == spellId)
            return dynObj;
        ++i;
    }
    return nullptr;
}

std::list<DynamicObject*> Unit::GetAllDynObjects(
    uint32 spellId, SpellEffectIndex effIndex)
{
    std::list<DynamicObject*> list;
    for (auto i = m_dynObjGUIDs.begin(); i != m_dynObjGUIDs.end();)
    {
        DynamicObject* dynObj = GetMap()->GetDynamicObject(*i);
        if (!dynObj)
        {
            i = m_dynObjGUIDs.erase(i);
            continue;
        }

        if (dynObj->GetSpellId() == spellId &&
            dynObj->GetEffIndex() == effIndex)
            list.push_back(dynObj);
        ++i;
    }
    return list;
}

GameObject* Unit::GetGameObject(uint32 spellId) const
{
    for (const auto& elem : m_gameObj)
        if ((elem)->GetSpellId() == spellId)
            return elem;

    return nullptr;
}

void Unit::AddGameObject(GameObject* gameObj)
{
    assert(gameObj && !gameObj->GetOwnerGuid());
    m_gameObj.push_back(gameObj);
    gameObj->SetOwnerGuid(GetObjectGuid());

    if (GetTypeId() == TYPEID_PLAYER && gameObj->GetSpellId())
    {
        SpellEntry const* createBySpell =
            sSpellStore.LookupEntry(gameObj->GetSpellId());
        // Need disable spell use for owner
        if (createBySpell &&
            createBySpell->HasAttribute(SPELL_ATTR_DISABLED_WHILE_ACTIVE))
            // note: item based cooldowns and cooldown spell mods with
            // charges
            // ignored (unknown existing cases)
            ((Player*)this)
                ->AddSpellAndCategoryCooldowns(createBySpell, 0, nullptr, true);
    }
}

void Unit::RemoveGameObject(GameObject* gameObj, bool del)
{
    assert(gameObj && gameObj->GetOwnerGuid() == GetObjectGuid());

    gameObj->SetOwnerGuid(ObjectGuid());

    // GO created by some spell
    if (uint32 spellid = gameObj->GetSpellId())
    {
        remove_auras(spellid);

        if (GetTypeId() == TYPEID_PLAYER)
        {
            SpellEntry const* createBySpell = sSpellStore.LookupEntry(spellid);
            // Need activate spell use for owner
            if (createBySpell &&
                createBySpell->HasAttribute(SPELL_ATTR_DISABLED_WHILE_ACTIVE))
                // note: item based cooldowns and cooldown spell mods with
                // charges ignored (unknown existing cases)
                ((Player*)this)->SendCooldownEvent(createBySpell);
        }
    }

    m_gameObj.remove(gameObj);

    if (del)
    {
        gameObj->SetRespawnTime(0);
        gameObj->Delete();
    }
}

void Unit::RemoveGameObject(uint32 spellid, bool del)
{
    if (m_gameObj.empty())
        return;
    GameObjectList::iterator i, next;
    for (i = m_gameObj.begin(); i != m_gameObj.end(); i = next)
    {
        next = i;
        if (spellid == 0 || (*i)->GetSpellId() == spellid)
        {
            (*i)->SetOwnerGuid(ObjectGuid());
            if (del)
            {
                (*i)->SetRespawnTime(0);
                (*i)->Delete();
            }

            next = m_gameObj.erase(i);
        }
        else
            ++next;
    }
}

void Unit::RemoveAllGameObjects()
{
    // remove references to unit
    for (auto i = m_gameObj.begin(); i != m_gameObj.end();)
    {
        (*i)->SetOwnerGuid(ObjectGuid());
        (*i)->SetRespawnTime(0);
        (*i)->Delete();
        i = m_gameObj.erase(i);
    }
}

void Unit::SendSpellNonMeleeDamageLog(SpellNonMeleeDamage* log)
{
    WorldPacket data(SMSG_SPELLNONMELEEDAMAGELOG,
        (16 + 4 + 4 + 1 + 4 + 4 + 1 + 1 + 4 + 4 + 1)); // we guess size
    data << log->target->GetPackGUID();
    data << log->attacker->GetPackGUID();
    data << uint32(log->SpellID);
    data << uint32(log->damage);     // damage amount
    data << uint8(log->schoolMask);  // damage school
    data << uint32(log->absorb);     // AbsorbedDamage
    data << uint32(log->resist);     // resist
    data << uint8(log->physicalLog); // if 1, then client show spell name
    // (example: %s's ranged shot hit %s for %u
    // school or %s suffers %u school damage
    // from %s's spell_name
    data << uint8(log->unused);   // unused
    data << uint32(log->blocked); // blocked
    data << uint32(log->HitInfo);
    data << uint8(0); // flag to use extend data
    SendMessageToSet(&data, true);
}

void Unit::SendSpellNonMeleeDamageLog(Unit* target, uint32 SpellID,
    uint32 Damage, SpellSchoolMask damageSchoolMask, uint32 AbsorbedDamage,
    uint32 Resist, bool PhysicalDamage, uint32 Blocked, bool CriticalHit)
{
    SpellNonMeleeDamage log(this, target, SpellID, damageSchoolMask);
    log.damage = Damage - AbsorbedDamage - Resist - Blocked;
    log.absorb = AbsorbedDamage;
    log.resist = Resist;
    log.physicalLog = PhysicalDamage;
    log.blocked = Blocked;
    log.HitInfo =
        SPELL_HIT_TYPE_UNK1 | SPELL_HIT_TYPE_UNK3 | SPELL_HIT_TYPE_UNK6;
    if (CriticalHit)
        log.HitInfo |= SPELL_HIT_TYPE_CRIT;
    SendSpellNonMeleeDamageLog(&log);
}

void Unit::SendPeriodicAuraLog(SpellPeriodicAuraLogInfo* pInfo)
{
    Aura* aura = pInfo->aura;
    Modifier* mod = aura->GetModifier();

    WorldPacket data(SMSG_PERIODICAURALOG, 30);
    if (mod->m_auraname != SPELL_AURA_PERIODIC_HEALTH_FUNNEL)
    {
        data << aura->GetTarget()->GetPackGUID();
        data << aura->GetCasterGuid().WriteAsPacked();
    }
    else
    {
        data << aura->GetCasterGuid().WriteAsPacked();
        data << aura->GetTarget()->GetPackGUID();
    }
    data << uint32(aura->GetId()); // spellId
    data << uint32(1);             // count

    // HACK: some aura types that should be reported as periodic damage are not
    // handled in the client, this sends them as a type that is (looks correct
    // for players, bit ugly code-wise).
    uint32 aura_type = mod->m_auraname;
    if (aura_type == SPELL_AURA_PERIODIC_LEECH ||
        aura_type == SPELL_AURA_PERIODIC_HEALTH_FUNNEL)
        aura_type = SPELL_AURA_PERIODIC_DAMAGE;

    data << uint32(aura_type); // auraId
    switch (aura_type)
    {
    case SPELL_AURA_PERIODIC_DAMAGE:
    case SPELL_AURA_PERIODIC_DAMAGE_PERCENT:
        data << uint32(pInfo->damage); // damage
        data << uint32(GetSpellSchoolMask(aura->GetSpellProto()));
        data << uint32(pInfo->absorb); // absorb
        data << uint32(pInfo->resist); // resist
        break;
    case SPELL_AURA_PERIODIC_HEAL:
    case SPELL_AURA_OBS_MOD_HEALTH:
        data << uint32(pInfo->damage); // damage
        break;
    case SPELL_AURA_OBS_MOD_MANA:
    case SPELL_AURA_PERIODIC_ENERGIZE:
        data << uint32(mod->m_miscvalue); // power type
        data << uint32(pInfo->damage);    // damage
        break;
    case SPELL_AURA_PERIODIC_MANA_LEECH:
        data << uint32(mod->m_miscvalue); // power type
        data << uint32(pInfo->damage);    // amount
        data << float(pInfo->multiplier); // gain multiplier
        break;
    default:
        logging.error("Unit::SendPeriodicAuraLog: unknown aura %u",
            uint32(mod->m_auraname));
        return;
    }

    aura->GetTarget()->SendMessageToSet(&data, true);
}

void Unit::ProcDamageAndSpell(Unit* pVictim, uint32 procAttacker,
    uint32 procVictim, uint32 procExtra, proc_amount amount,
    WeaponAttackType attType, SpellEntry const* procSpell,
    ExtraAttackType extraAttackType, uint32 extraAttackId)
{
    // Not much to do if no flags are set.
    if (procAttacker)
        ProcDamageAndSpellFor(false, pVictim, procAttacker, procExtra, attType,
            procSpell, amount, extraAttackType, extraAttackId);
    // Now go on with a victim's events'n'auras
    // Not much to do if no flags are set or there is no victim
    if (pVictim && pVictim->isAlive() && procVictim)
        pVictim->ProcDamageAndSpellFor(true, this, procVictim, procExtra,
            attType, procSpell, amount, extraAttackType, extraAttackId);
}

void Unit::ProcSpellsOnCast(Spell* spell, Unit* /*pVictim*/,
    uint32 procAttacker, uint32 /*procVictim*/, uint32 procExtra,
    proc_amount /*amount*/, WeaponAttackType /*attType*/,
    SpellEntry const* procSpell, ExtraAttackType /*extraAttackType*/,
    uint32 /*extraAttackId*/, int32 cast_time)
{
    // FIXME: This function should be removed in favor of having auras that
    // proc
    // on being used marked somehow (custom attr maybe), and then only
    // removing
    // them if a spell actually makes use of their modifier.

    if (procAttacker)
    {
        for (auto& elem : m_auraHolders)
        {
            if (elem.second->IsDisabled())
                continue;

            // ProcSpellsOnCast happens after auras of instant spells have
            // been
            // applied, so ignore aura with same id as procSpell
            if (procSpell->Id == elem.second->GetId())
                continue;

            SpellEntry const* entry = elem.second->GetSpellProto();
            if (!IsSpellProcOnCast(entry))
                continue;

            // Hack fix for Backlash and Nightfall. It makes it so the used
            // cast
            // time must be 0 for these spells, so they're not consumed when
            // they weren't used.
            if (entry->HasApplyAuraName(SPELL_AURA_ADD_PCT_MODIFIER))
            {
                int index = entry->EffectApplyAuraName[0] ==
                                    SPELL_AURA_ADD_PCT_MODIFIER ?
                                0 :
                                entry->EffectApplyAuraName[1] ==
                                        SPELL_AURA_ADD_PCT_MODIFIER ?
                                1 :
                                2;
                if (entry->EffectMiscValue[index] == SPELLMOD_CASTING_TIME &&
                    entry->EffectBasePoints[index] == -101)
                {
                    if (cast_time > 0)
                        continue;
                }
            }

            for (uint8 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                Aura* aur = elem.second->GetAura(SpellEffectIndex(i));
                if (aur &&
                    aur->CanProcFrom(
                        procSpell, PROC_EX_NONE, procExtra, true, true))
                {
                    if (elem.second->GetId() == 16166)
                        spell->ElementalMasteryUsed = true;

                    if (elem.second->DropAuraCharge())
                        RemoveAuraHolder(elem.second); // No charges left, need
                                                       // to remove holder

                    break; // Do not continue processing, we already dropped
                           // one
                           // charge
                }
            }
        }
    }
}

void Unit::SendSpellMiss(Unit* target, uint32 spellID, SpellMissInfo missInfo)
{
    WorldPacket data(SMSG_SPELLLOGMISS, (4 + 8 + 1 + 4 + 8 + 1));
    data << uint32(spellID);
    data << GetObjectGuid();
    data << uint8(0);  // can be 0 or 1
    data << uint32(1); // target count
    // for(i = 0; i < target count; ++i)
    data << target->GetObjectGuid(); // target GUID
    data << uint8(missInfo);
    // end loop
    SendMessageToSet(&data, true);
}

void Unit::SendAttackStateUpdate(CalcDamageInfo* damageInfo)
{
    WorldPacket data(SMSG_ATTACKERSTATEUPDATE, (16 + 84)); // we guess size
    data << (uint32)damageInfo->HitInfo;
    data << GetPackGUID();
    data << damageInfo->target->GetPackGUID();
    data << uint32(damageInfo->damage); // Full damage

    data << uint8(1); // Sub damage count
    //===  Sub damage description
    data << uint32(damageInfo->damageSchoolMask); // School of sub damage
    data << float(damageInfo->damage);            // sub damage
    data << uint32(damageInfo->damage);           // Sub Damage
    data << uint32(damageInfo->absorb);           // Absorb
    data << uint32(damageInfo->resist);           // Resist
    //=================================================
    data << uint32(damageInfo->TargetState);
    data << uint32(0); // unknown, usually seen with -1, 0 and 1000
    data << uint32(
        0); // spell id, seen with heroic strike and disarm as examples.
            // HITINFO_NOACTION normally set if spell
    data << uint32(damageInfo->blocked_amount);
    SendMessageToSet(&data, true); /**/
}

void Unit::SendAttackStateUpdate(uint32 HitInfo, Unit* target,
    uint8 /*SwingType*/, SpellSchoolMask damageSchoolMask, uint32 Damage,
    uint32 AbsorbDamage, uint32 Resist, VictimState TargetState,
    uint32 BlockedAmount)
{
    CalcDamageInfo dmgInfo;
    dmgInfo.HitInfo = HitInfo;
    dmgInfo.attacker = this;
    dmgInfo.target = target;
    dmgInfo.damage = Damage - AbsorbDamage - Resist - BlockedAmount;
    dmgInfo.damageSchoolMask = damageSchoolMask;
    dmgInfo.absorb = AbsorbDamage;
    dmgInfo.resist = Resist;
    dmgInfo.TargetState = TargetState;
    dmgInfo.blocked_amount = BlockedAmount;
    SendAttackStateUpdate(&dmgInfo);
}

void Unit::setPowerType(Powers new_powertype)
{
    SetByteValue(UNIT_FIELD_BYTES_0, 3, new_powertype);

    if (GetTypeId() == TYPEID_PLAYER)
    {
        if (((Player*)this)->GetGroup())
            ((Player*)this)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_POWER_TYPE);
    }
    else if (((Creature*)this)->IsPet())
    {
        Pet* pet = ((Pet*)this);
        if (pet->isControlled())
        {
            Unit* owner = GetOwner();
            if (owner && (owner->GetTypeId() == TYPEID_PLAYER) &&
                ((Player*)owner)->GetGroup())
                ((Player*)owner)
                    ->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_PET_POWER_TYPE);
        }
    }

    switch (new_powertype)
    {
    default:
    case POWER_MANA:
        break;
    case POWER_RAGE:
        SetMaxPower(POWER_RAGE, GetCreatePowers(POWER_RAGE));
        SetPower(POWER_RAGE, 0);
        break;
    case POWER_FOCUS:
        SetMaxPower(POWER_FOCUS, GetCreatePowers(POWER_FOCUS));
        SetPower(POWER_FOCUS, GetCreatePowers(POWER_FOCUS));
        break;
    case POWER_ENERGY:
        SetMaxPower(POWER_ENERGY, GetCreatePowers(POWER_ENERGY));
        SetPower(POWER_ENERGY, 0);
        break;
    case POWER_HAPPINESS:
        SetMaxPower(POWER_HAPPINESS, GetCreatePowers(POWER_HAPPINESS));
        SetPower(POWER_HAPPINESS, GetCreatePowers(POWER_HAPPINESS));
        break;
    }
}

FactionTemplateEntry const* Unit::getFactionTemplateEntry() const
{
    FactionTemplateEntry const* entry =
        sFactionTemplateStore.LookupEntry(getFaction());
    if (!entry)
    {
        static ObjectGuid guid; // prevent repeating spam same faction problem

        if (GetObjectGuid() != guid)
        {
            logging.error("%s have invalid faction (faction template id) #%u",
                GetGuidStr().c_str(), getFaction());
            guid = GetObjectGuid();
        }
    }
    return entry;
}

// function based on function Unit::UnitReaction from 13850 client
ReputationRank Unit::GetReactionTo(Unit* target)
{
    // always friendly to self
    if (this == target)
        return REP_FRIENDLY;

    // always friendly to charmer or owner
    if (GetCharmerOrOwnerOrSelf() == target->GetCharmerOrOwnerOrSelf())
        return REP_FRIENDLY;

    if (HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PVP_ATTACKABLE))
    {
        if (target->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PVP_ATTACKABLE))
        {
            Player* selfPlayerOwner = GetAffectingPlayer();
            Player* targetPlayerOwner = target->GetAffectingPlayer();

            if (selfPlayerOwner && targetPlayerOwner)
            {
                // always friendly to other unit controlled by player, or to
                // the
                // player himself
                if (selfPlayerOwner == targetPlayerOwner)
                    return REP_FRIENDLY;

                // duel - always hostile to opponent
                if (selfPlayerOwner->duel &&
                    selfPlayerOwner->duel->opponent == targetPlayerOwner &&
                    selfPlayerOwner->duel->startTime != 0)
                    return REP_HOSTILE;

                // same group - checks dependant only on our faction - skip
                // FFA_PVP for example
                if (selfPlayerOwner->IsInRaidWith(targetPlayerOwner))
                    return REP_FRIENDLY; // return true to allow config
                                         // option
                // AllowTwoSide.Interaction.Group to
                // work
                // however client seems to allow mixed group parties,
                // because in
                // 13850 client it works like:
                // return GetFactionReactionTo(getFactionTemplateEntry(),
                // target);
            }

            // check FFA_PVP
            if (GetByteValue(UNIT_FIELD_BYTES_2, 1) & UNIT_BYTE2_FLAG_FFA_PVP &&
                target->GetByteValue(UNIT_FIELD_BYTES_2, 1) &
                    UNIT_BYTE2_FLAG_FFA_PVP)
                return REP_HOSTILE;

            if (selfPlayerOwner)
            {
                if (FactionTemplateEntry const* targetFactionTemplateEntry =
                        target->getFactionTemplateEntry())
                {
                    if (ReputationRank const* repRank =
                            selfPlayerOwner->GetReputationMgr()
                                .GetForcedRankIfAny(targetFactionTemplateEntry))
                        return *repRank;
                    if (!selfPlayerOwner->HasFlag(
                            UNIT_FIELD_FLAGS_2, UNIT_FLAG2_IGNORE_REPUTATION))
                    {
                        if (FactionEntry const* targetFactionEntry =
                                sFactionStore.LookupEntry(
                                    targetFactionTemplateEntry->faction))
                        {
                            if (targetFactionEntry->CanHaveReputation())
                            {
                                // check contested flags
                                if (targetFactionTemplateEntry->factionFlags &
                                        FACTION_TEMPLATE_FLAG_CONTESTED_GUARD &&
                                    selfPlayerOwner->HasFlag(PLAYER_FLAGS,
                                        PLAYER_FLAGS_CONTESTED_PVP))
                                    return REP_HOSTILE;

                                // if faction has reputation, hostile state
                                // depends only from AtWar state
                                if (selfPlayerOwner->GetReputationMgr().IsAtWar(
                                        targetFactionEntry))
                                    return REP_HOSTILE;
                                return REP_FRIENDLY;
                            }
                        }
                    }
                }
            }
        }
    }
    // do checks dependant only on our faction
    return GetFactionReactionTo(getFactionTemplateEntry(), target);
}

ReputationRank Unit::GetFactionReactionTo(
    const FactionTemplateEntry* factionTemplateEntry, Unit* target)
{
    // always neutral when no template entry found
    if (!factionTemplateEntry)
        return REP_NEUTRAL;

    FactionTemplateEntry const* targetFactionTemplateEntry =
        target->getFactionTemplateEntry();
    if (!targetFactionTemplateEntry)
        return REP_NEUTRAL;

    if (Player const* targetPlayerOwner = target->GetAffectingPlayer())
    {
        // check contested flags
        if (factionTemplateEntry->factionFlags &
                FACTION_TEMPLATE_FLAG_CONTESTED_GUARD &&
            targetPlayerOwner->HasFlag(
                PLAYER_FLAGS, PLAYER_FLAGS_CONTESTED_PVP))
            return REP_HOSTILE;
        if (ReputationRank const* repRank =
                targetPlayerOwner->GetReputationMgr().GetForcedRankIfAny(
                    factionTemplateEntry))
            return *repRank;
        if (!target->HasFlag(UNIT_FIELD_FLAGS_2, UNIT_FLAG2_IGNORE_REPUTATION))
        {
            if (FactionEntry const* factionEntry =
                    sFactionStore.LookupEntry(factionTemplateEntry->faction))
            {
                if (factionEntry->CanHaveReputation())
                {
                    // CvP case - check reputation, don't allow state higher
                    // than neutral when at war
                    ReputationRank repRank =
                        targetPlayerOwner->GetReputationMgr().GetRank(
                            factionEntry);
                    if (targetPlayerOwner->GetReputationMgr().IsAtWar(
                            factionEntry))
                        repRank = std::min(REP_NEUTRAL, repRank);
                    return repRank;
                }
            }
        }
    }

    // common faction based check
    if (factionTemplateEntry->IsHostileTo(*targetFactionTemplateEntry))
        return REP_HOSTILE;
    if (factionTemplateEntry->IsFriendlyTo(*targetFactionTemplateEntry))
        return REP_FRIENDLY;
    if (targetFactionTemplateEntry->IsFriendlyTo(*factionTemplateEntry))
        return REP_FRIENDLY;
    if (factionTemplateEntry->factionFlags &
        FACTION_TEMPLATE_FLAG_HOSTILE_BY_DEFAULT)
        return REP_HOSTILE;
    // neutral by default
    return REP_NEUTRAL;
}

bool Unit::IsHostileTo(Unit const* unit) const
{
    // always non-hostile to self
    if (unit == this)
        return false;
    // always non-hostile to your owner (assuming it's not charmed, i.e.
    // "stolen", by something)
    if (unit->GetOwnerGuid() == GetObjectGuid() &&
        unit->GetCharmerGuid().IsEmpty())
        return false;

    // always non-hostile to GM in GM mode
    if (unit->GetTypeId() == TYPEID_PLAYER &&
        ((Player const*)unit)->isGameMaster())
        return false;

    // always hostile to enemy
    if (getVictim() == unit)
        return true;

    // test pet/charm masters instead pers/charmeds
    Unit const* testerOwner = GetCharmerOrOwner();
    Unit const* targetOwner = unit->GetCharmerOrOwner();

    // always hostile to owner's enemy
    if (testerOwner &&
        (testerOwner->getVictim() == unit || unit->getVictim() == testerOwner))
        return true;

    // always hostile to enemy owner
    if (targetOwner &&
        (getVictim() == targetOwner || targetOwner->getVictim() == this))
        return true;

    // always hostile to owner of owner's enemy
    if (testerOwner && targetOwner &&
        (testerOwner->getVictim() == targetOwner ||
            targetOwner->getVictim() == testerOwner))
        return true;

    Unit const* tester = testerOwner ? testerOwner : this;
    Unit const* target = targetOwner ? targetOwner : unit;

    // always non-hostile to target with common owner, or to owner/pet
    if (tester == target)
        return false;

    // special cases (Duel, etc)
    if (tester->GetTypeId() == TYPEID_PLAYER &&
        target->GetTypeId() == TYPEID_PLAYER)
    {
        Player const* pTester = (Player const*)tester;
        Player const* pTarget = (Player const*)target;

        // Duel
        if (pTester->IsInDuelWith(pTarget))
            return true;

        // Group
        if (pTester->GetGroup() && pTester->GetGroup() == pTarget->GetGroup())
            return false;

        // Sanctuary
        if (pTarget->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_SANCTUARY) &&
            pTester->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_SANCTUARY))
            return false;

        // PvP FFA state
        if (pTester->IsFFAPvP() && pTarget->IsFFAPvP())
            return true;

        //= PvP states
        // Green/Blue (can't attack)
        if (pTester->GetTeam() == pTarget->GetTeam())
            return false;

        // Red (can attack) if true, Blue/Yellow (can't attack) in another
        // case
        return pTester->IsPvP() && pTarget->IsPvP();
    }

    // faction base cases
    FactionTemplateEntry const* tester_faction = getFactionTemplateEntry();
    FactionTemplateEntry const* target_faction =
        unit->getFactionTemplateEntry();
    if (!tester_faction || !target_faction)
        return false;

    if (target->isAttackingPlayer() && tester->IsContestedGuard())
        return true;

    // PvC forced reaction and reputation case
    if (tester->GetTypeId() == TYPEID_PLAYER &&
        tester->getFaction() == getFaction())
    {
        if (target_faction->faction)
        {
            // forced reaction
            if (ReputationRank const* force =
                    ((Player*)tester)
                        ->GetReputationMgr()
                        .GetForcedRankIfAny(target_faction))
                return *force <= REP_HOSTILE;

            // if faction have reputation then hostile state for tester at
            // 100%
            // dependent from at_war state
            if (FactionEntry const* raw_target_faction =
                    sFactionStore.LookupEntry(target_faction->faction))
                if (FactionState const* factionState =
                        ((Player*)tester)
                            ->GetReputationMgr()
                            .GetState(raw_target_faction))
                    return (factionState->Flags & FACTION_FLAG_AT_WAR);
        }
    }
    // CvP forced reaction and reputation case
    else if (target->GetTypeId() == TYPEID_PLAYER &&
             target->getFaction() == unit->getFaction())
    {
        if (tester_faction->faction)
        {
            // forced reaction
            if (ReputationRank const* force =
                    ((Player*)target)
                        ->GetReputationMgr()
                        .GetForcedRankIfAny(tester_faction))
                return *force <= REP_HOSTILE;

            // apply reputation state
            FactionEntry const* raw_tester_faction =
                sFactionStore.LookupEntry(tester_faction->faction);
            if (raw_tester_faction && raw_tester_faction->reputationListID >= 0)
                return ((Player const*)target)
                           ->GetReputationMgr()
                           .GetRank(raw_tester_faction) <= REP_HOSTILE;
        }
    }

    // common faction based case (CvC,PvC,CvP)
    return tester_faction->IsHostileTo(*target_faction);
}

bool Unit::IsFriendlyTo(Unit const* unit) const
{
    // always friendly to self
    if (unit == this)
        return true;

    // always friendly to GM in GM mode
    if (unit->GetTypeId() == TYPEID_PLAYER &&
        ((Player const*)unit)->isGameMaster())
        return true;

    // always non-friendly to enemy
    if (getVictim() == unit || unit->getVictim() == this)
        return false;

    // test pets or charmed mobs using their owners (or if they're owned by
    // a
    // totem; the owner of the totem)
    Unit const* testerOwner = GetCharmerOrOwnerOrTotemOwner();
    Unit const* targetOwner = unit->GetCharmerOrOwnerOrTotemOwner();

    // always non-friendly to owner's enemy
    if (testerOwner &&
        (testerOwner->getVictim() == unit || unit->getVictim() == testerOwner))
        return false;

    // always non-friendly to enemy owner
    if (targetOwner &&
        (getVictim() == targetOwner || targetOwner->getVictim() == this))
        return false;

    // always non-friendly to owner of owner's enemy
    if (testerOwner && targetOwner &&
        (testerOwner->getVictim() == targetOwner ||
            targetOwner->getVictim() == testerOwner))
        return false;

    Unit const* tester = testerOwner ? testerOwner : this;
    Unit const* target = targetOwner ? targetOwner : unit;

    // always friendly to target with common owner, or to owner/pet
    if (tester == target)
        return true;

    // special cases (Duel)
    if (tester->GetTypeId() == TYPEID_PLAYER &&
        target->GetTypeId() == TYPEID_PLAYER)
    {
        Player const* pTester = (Player const*)tester;
        Player const* pTarget = (Player const*)target;

        // Duel
        if (pTester->IsInDuelWith(pTarget))
            return false;

        // Group
        if (pTester->GetGroup() && pTester->GetGroup() == pTarget->GetGroup())
            return true;

        // PvP FFA state
        if (pTester->IsFFAPvP() && pTarget->IsFFAPvP())
            return false;

        //= PvP states
        // Green/Blue (non-attackable)
        if (pTester->GetTeam() == pTarget->GetTeam())
            return true;

        // Blue (friendly/non-attackable) if not PVP, or Yellow/Red in
        // another
        // case (attackable)
        return !pTarget->IsPvP();
    }

    // faction base cases
    FactionTemplateEntry const* tester_faction = getFactionTemplateEntry();
    FactionTemplateEntry const* target_faction =
        unit->getFactionTemplateEntry();
    if (!tester_faction || !target_faction)
        return false;

    if (target->isAttackingPlayer() && tester->IsContestedGuard())
        return false;

    // PvC forced reaction and reputation case
    if (tester->GetTypeId() == TYPEID_PLAYER &&
        tester->getFaction() == getFaction())
    {
        if (target_faction->faction)
        {
            // forced reaction
            if (ReputationRank const* force =
                    ((Player*)tester)
                        ->GetReputationMgr()
                        .GetForcedRankIfAny(target_faction))
                return *force >= REP_FRIENDLY;

            // if faction have reputation then friendly state for tester at
            // 100%
            // dependent from at_war state
            if (FactionEntry const* raw_target_faction =
                    sFactionStore.LookupEntry(target_faction->faction))
                if (FactionState const* factionState =
                        ((Player*)tester)
                            ->GetReputationMgr()
                            .GetState(raw_target_faction))
                    return !(factionState->Flags & FACTION_FLAG_AT_WAR);
        }
    }
    // CvP forced reaction and reputation case
    else if (target->GetTypeId() == TYPEID_PLAYER &&
             target->getFaction() == unit->getFaction())
    {
        if (tester_faction->faction)
        {
            // forced reaction
            if (ReputationRank const* force =
                    ((Player*)target)
                        ->GetReputationMgr()
                        .GetForcedRankIfAny(tester_faction))
                return *force >= REP_FRIENDLY;

            // apply reputation state
            if (FactionEntry const* raw_tester_faction =
                    sFactionStore.LookupEntry(tester_faction->faction))
                if (raw_tester_faction->reputationListID >= 0)
                    return ((Player const*)target)
                               ->GetReputationMgr()
                               .GetRank(raw_tester_faction) >= REP_FRIENDLY;
        }
    }

    // common faction based case (CvC,PvC,CvP)
    return tester_faction->IsFriendlyTo(*target_faction);
}

bool Unit::IsHostileToPlayers() const
{
    FactionTemplateEntry const* my_faction = getFactionTemplateEntry();
    if (!my_faction || !my_faction->faction)
        return false;

    FactionEntry const* raw_faction =
        sFactionStore.LookupEntry(my_faction->faction);
    if (raw_faction && raw_faction->reputationListID >= 0)
        return false;

    return my_faction->IsHostileToPlayers();
}

bool Unit::IsNeutralToAll() const
{
    FactionTemplateEntry const* my_faction = getFactionTemplateEntry();
    if (!my_faction || !my_faction->faction)
        return true;

    FactionEntry const* raw_faction =
        sFactionStore.LookupEntry(my_faction->faction);
    if (raw_faction && raw_faction->reputationListID >= 0)
        return false;

    return my_faction->IsNeutralToAll();
}

bool Unit::IsInPartyWith(Unit* unit)
{
    if (this == unit)
        return true;

    Unit* u1 = GetCharmerOrOwnerOrSelf();
    Unit* u2 = unit->GetCharmerOrOwnerOrSelf();
    if (u1 == u2)
        return true;

    if (u1->GetTypeId() == TYPEID_PLAYER && u2->GetTypeId() == TYPEID_PLAYER)
        return ((Player*)u1)->IsInSameGroupWith((Player*)u2);
    else if ((u2->GetTypeId() == TYPEID_PLAYER &&
                 u1->GetTypeId() == TYPEID_UNIT &&
                 ((Creature*)u1)->GetCreatureInfo()->type_flags &
                     CREATURE_TYPEFLAGS_PARTY_MEMBER) ||
             (u1->GetTypeId() == TYPEID_PLAYER &&
                 u2->GetTypeId() == TYPEID_UNIT &&
                 ((Creature*)u2)->GetCreatureInfo()->type_flags &
                     CREATURE_TYPEFLAGS_PARTY_MEMBER))
        return true;
    else
        return false;
}

bool Unit::IsInRaidWith(Unit* unit)
{
    if (this == unit)
        return true;

    Unit* u1 = GetCharmerOrOwnerOrSelf();
    Unit* u2 = unit->GetCharmerOrOwnerOrSelf();
    if (u1 == u2)
        return true;

    if (u1->GetTypeId() == TYPEID_PLAYER && u2->GetTypeId() == TYPEID_PLAYER)
        return ((Player*)u1)->IsInSameRaidWith((Player*)u2);
    else if ((u2->GetTypeId() == TYPEID_PLAYER &&
                 u1->GetTypeId() == TYPEID_UNIT &&
                 ((Creature*)u1)->GetCreatureInfo()->type_flags &
                     CREATURE_TYPEFLAGS_PARTY_MEMBER) ||
             (u1->GetTypeId() == TYPEID_PLAYER &&
                 u2->GetTypeId() == TYPEID_UNIT &&
                 ((Creature*)u2)->GetCreatureInfo()->type_flags &
                     CREATURE_TYPEFLAGS_PARTY_MEMBER))
        return true;
    else
        return false;
}

void Unit::GetPartyMembers(std::list<Unit*>& TagUnitMap)
{
    Unit* owner = GetCharmerOrOwnerOrSelf();
    Group* group = nullptr;
    if (owner->GetTypeId() == TYPEID_PLAYER)
        group = ((Player*)owner)->GetGroup();

    if (group)
    {
        uint8 subgroup = ((Player*)owner)->GetSubGroup();

        for (auto member : group->members(true))
        {
            // IsHostileTo check duel and controlled by enemy
            if (member && member->GetSubGroup() == subgroup &&
                !IsHostileTo(member))
            {
                if (member->isAlive() && IsInMap(member))
                    TagUnitMap.push_back(member);

                if (Pet* pet = member->GetPet())
                    if (pet->isAlive() && IsInMap(member))
                        TagUnitMap.push_back(pet);
            }
        }
    }
    else
    {
        if (owner->isAlive() && (owner == this || IsInMap(owner)))
            TagUnitMap.push_back(owner);
        if (Pet* pet = owner->GetPet()) // if (Guardian* pet =
                                        // owner->GetGuardianPet())
            if (pet->isAlive() && (pet == this || IsInMap(pet)))
                TagUnitMap.push_back(pet);
    }
}

bool Unit::Attack(Unit* victim, bool meleeAttack)
{
    if (!victim || victim == this)
        return false;

    // dead units can neither attack nor be attacked
    if (!isAlive() || !victim->IsInWorld() || !victim->isAlive())
        return false;

    // Players or player owned pets cannot attack people in sanctuary
    if (victim->GetTypeId() == TYPEID_PLAYER &&
        victim->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_SANCTUARY) &&
        player_or_pet())
        return false;

    // player cannot attack in mount state
    if (GetTypeId() == TYPEID_PLAYER && IsMounted())
        return false;

    // Creatures cannot attack while running away in fear
    if (GetTypeId() == TYPEID_UNIT &&
        movement_gens.top_id() == movement::gen::run_in_fear)
        return false;

    // Creatures cannot attack while in evade mode
    if (GetTypeId() == TYPEID_UNIT &&
        static_cast<Creature*>(this)->IsInEvadeMode())
        return false;

    if (GetTypeId() == TYPEID_UNIT && victim->GetTypeId() == TYPEID_UNIT &&
        static_cast<Creature*>(victim)->GetCreatureInfo()->flags_extra &
            CREATURE_FLAG_IGNORED_BY_NPCS &&
        victim->GetCharmerGuid().IsEmpty())
        return false;

    // Cannot attack if victim is in feign death (and we're not in his
    // hostile
    // ref manager
    // as ignored already; as that would mean we resisted his feign death)
    if (!player_or_pet() && victim->HasAuraType(SPELL_AURA_FEIGN_DEATH))
    {
        bool resisted_fd = false;
        HostileReference* ref = victim->getHostileRefManager().getFirst();
        while (ref)
        {
            if (ref->getSource()->getOwner() == this)
            {
                if (!ref->isIgnored())
                    resisted_fd = true;
                break;
            }
            ref = ref->next();
        }
        if (!resisted_fd)
            return false;
    }

    // nobody can attack GM in GM-mode
    if (victim->GetTypeId() == TYPEID_PLAYER)
    {
        if (((Player*)victim)->isGameMaster())
            return false;
    }
    else
    {
        if (((Creature*)victim)->IsInEvadeMode())
            return false;
    }

    // in fighting already
    if (m_attacking)
    {
        if (m_attacking == victim)
        {
            // switch to melee attack from ranged/magic
            if (meleeAttack && !hasUnitState(UNIT_STAT_MELEE_ATTACKING))
            {
                addUnitState(UNIT_STAT_MELEE_ATTACKING);
                SendMeleeAttackStart(victim);
                return true;
            }
            return false;
        }

        // remove old target data
        AttackStop(true);
    }

    // Set our target
    if (GetTypeId() == TYPEID_UNIT)
    {
        if (!static_cast<Creature*>(this)->IsTotem() &&
            !static_cast<Creature*>(this)->IsAffectedByThreatIgnoringCC() &&
            (!static_cast<Creature*>(this)->AI() ||
                !static_cast<Creature*>(this)->AI()->IsPacified()))
            SetTargetGuid(victim->GetObjectGuid());
    }
    else
        SetTargetGuid(victim->GetObjectGuid());

    if (meleeAttack)
    {
        if (getAttackTimer(BASE_ATTACK) < 500)
            setAttackTimer(BASE_ATTACK, 500);
        if (getAttackTimer(OFF_ATTACK) < 500 + ATTACK_DISPLAY_DELAY)
            setAttackTimer(OFF_ATTACK, 500 + ATTACK_DISPLAY_DELAY);
        addUnitState(UNIT_STAT_MELEE_ATTACKING);
        SendMeleeAttackStart(victim);
    }

    m_attacking = victim;
    m_attackingGuid = victim->GetObjectGuid();
    m_attacking->_addAttacker(this);

    if (GetTypeId() == TYPEID_UNIT)
    {
        // Clear emote state
        SetUInt32Value(UNIT_NPC_EMOTESTATE, 0);

        static_cast<Creature*>(this)->SendAIReaction(AI_REACTION_HOSTILE);
        static_cast<Creature*>(this)->SetAggroPulseTimer(
            1000); // Do first pulse in 1000 ms

        // let pets and guardians know owner entered combat
        static_cast<Creature*>(this)->CallForAllControlledUnits(
            [victim](Unit* pet)
            {
                if (pet->GetTypeId() == TYPEID_UNIT &&
                    static_cast<Creature*>(pet)->AI())
                    static_cast<Creature*>(pet)->AI()->AttackedBy(victim);
                else if (pet->GetTypeId() == TYPEID_PLAYER &&
                         static_cast<Player*>(pet)->AI())
                    static_cast<Player*>(pet)->AI()->AttackedBy(victim);
            },
            CONTROLLED_PET | CONTROLLED_GUARDIANS | CONTROLLED_CHARM);
    }

    return true;
}

void Unit::AttackedBy(Unit* attacker)
{
    // trigger AI reaction
    if (GetTypeId() == TYPEID_UNIT && ((Creature*)this)->AI())
        ((Creature*)this)->AI()->AttackedBy(attacker);

    // do not pet reaction for self inflicted damage (like environmental)
    if (attacker == this)
        return;

    // trigger pet AI reaction
    if (Pet* pet = GetPet())
    {
        if (pet->CanStartAttacking(attacker))
            pet->AttackedBy(attacker);
    }

    // trigger guardian AI reaction
    for (auto guid : GetGuardians())
        if (Pet* guardian = GetMap()->GetPet(guid))
        {
            if (guardian->CanStartAttacking(attacker))
                guardian->AttackedBy(attacker);
        }
}

bool Unit::AttackStop(
    bool /*targetSwitch*/, bool client_initiated, bool pet_evade, bool on_death)
{
    if (!m_attacking)
        return false;

    Unit* victim = m_attacking;

    m_attacking->_removeAttacker(this);
    m_attacking = nullptr;
    m_attackingGuid.Clear();

    // Clear our target
    SetTargetGuid(ObjectGuid());

    clearUnitState(UNIT_STAT_MELEE_ATTACKING);

    InterruptSpell(CURRENT_MELEE_SPELL);

    SendMeleeAttackStop(victim);

    if (GetTypeId() == TYPEID_PLAYER && !client_initiated)
        static_cast<Player*>(this)
            ->SendAttackSwingCancelAttack(); // tell the client to stop
    // reactivating melee and ranged
    // attacks

    if (!pet_evade && GetTypeId() == TYPEID_UNIT &&
        static_cast<Creature*>(this)->behavior())
        static_cast<Pet*>(this)->behavior()->evade(on_death);

    return true;
}

void Unit::CombatStop(bool includingCast, bool keep_combat, bool on_death)
{
    if (includingCast && IsNonMeleeSpellCasted(false))
        InterruptNonMeleeSpells(false);

    bool had_victim = AttackStop();
    RemoveAllAttackers(on_death);

    // If attack stop had no victim we need to manually send the stop swing
    // opcode (in case the player is auto-shooting atm)
    if (!had_victim && GetTypeId() == TYPEID_PLAYER)
        static_cast<Player*>(this)
            ->SendAttackSwingCancelAttack(); // tell the client to stop
    // reactivating melee and ranged
    // attacks

    if (GetTypeId() == TYPEID_UNIT)
    {
        if (((Creature*)this)->GetTemporaryFactionFlags() &
            TEMPFACTION_RESTORE_COMBAT_STOP)
            ((Creature*)this)->ClearTemporaryFaction();

        // Stop evading if we were
        if (static_cast<Creature*>(this)->evading())
            static_cast<Creature*>(this)->stop_evade();
    }

    if (!keep_combat)
        ClearInCombat();
}

struct CombatStopWithPetsHelper
{
    explicit CombatStopWithPetsHelper(bool _includingCast)
      : includingCast(_includingCast)
    {
    }
    void operator()(Unit* unit) const { unit->CombatStop(includingCast); }
    bool includingCast;
};

void Unit::CombatStopWithPets(bool includingCast)
{
    CombatStop(includingCast);
    CallForAllControlledUnits(CombatStopWithPetsHelper(includingCast),
        CONTROLLED_PET | CONTROLLED_GUARDIANS | CONTROLLED_CHARM);
}

struct IsAttackingPlayerHelper
{
    explicit IsAttackingPlayerHelper() {}
    bool operator()(Unit const* unit) const
    {
        return unit->isAttackingPlayer();
    }
};

bool Unit::isAttackingPlayer() const
{
    if (hasUnitState(UNIT_STAT_ATTACK_PLAYER))
        return true;

    return CheckAllControlledUnits(
        IsAttackingPlayerHelper(), CONTROLLED_PET | CONTROLLED_TOTEMS |
                                       CONTROLLED_GUARDIANS | CONTROLLED_CHARM);
}

void Unit::RemoveAllAttackers(bool on_death)
{
    while (!m_attackers.empty())
    {
        auto iter = m_attackers.begin();
        if (!(*iter)->AttackStop(false, false, false, on_death))
        {
            logging.error(
                "WORLD: Unit has an attacker that isn't attacking it!");
            m_attackers.erase(iter);
        }
    }
}

bool Unit::HasAuraStateForCaster(AuraState flag, ObjectGuid casterGuid) const
{
    if (!HasAuraState(flag))
        return false;

    // single per-caster aura state
    if (flag == AURA_STATE_CONFLAGRATE)
    {
        const Auras& dotList = GetAurasByType(SPELL_AURA_PERIODIC_DAMAGE);
        for (const auto& elem : dotList)
        {
            if ((elem)->GetCasterGuid() == casterGuid &&
                //  Immolate
                (elem)->GetSpellProto()->IsFitToFamily(
                    SPELLFAMILY_WARLOCK, UI64LIT(0x0000000000000004)))
            {
                return true;
            }
        }

        return false;
    }

    return true;
}

void Unit::ModifyAuraState(AuraState flag, bool apply)
{
    if (apply)
    {
        if (!HasFlag(UNIT_FIELD_AURASTATE, 1 << (flag - 1)))
        {
            SetFlag(UNIT_FIELD_AURASTATE, 1 << (flag - 1));
            if (GetTypeId() == TYPEID_PLAYER)
            {
                const PlayerSpellMap& sp_list = ((Player*)this)->GetSpellMap();
                for (const auto& elem : sp_list)
                {
                    if (elem.second.state == PLAYERSPELL_REMOVED)
                        continue;
                    SpellEntry const* spellInfo =
                        sSpellStore.LookupEntry(elem.first);
                    if (!spellInfo || !IsPassiveSpell(spellInfo))
                        continue;
                    if (AuraState(spellInfo->CasterAuraState) == flag)
                        CastSpell(this, elem.first, true, nullptr);
                }
            }
        }
    }
    else
    {
        if (HasFlag(UNIT_FIELD_AURASTATE, 1 << (flag - 1)))
        {
            RemoveFlag(UNIT_FIELD_AURASTATE, 1 << (flag - 1));

            remove_auras_if([flag](AuraHolder* holder)
                {
                    return holder->GetSpellProto()->CasterAuraState == flag &&
                           !(holder->GetSpellProto()->SpellIconID == 2006 &&
                               holder->GetSpellProto()->SpellFamilyFlags &
                                   0x100000);
                }); // Rampage not removed at state change
        }
    }
}

Unit* Unit::GetOwner() const
{
    if (ObjectGuid ownerid = GetOwnerGuid())
        return ObjectAccessor::GetUnit(*this, ownerid);
    return nullptr;
}

Unit* Unit::GetCharmer() const
{
    if (ObjectGuid charmerid = GetCharmerGuid())
        return ObjectAccessor::GetUnit(*this, charmerid);
    return nullptr;
}

bool Unit::IsCharmerOrOwnerPlayerOrPlayerItself() const
{
    if (GetTypeId() == TYPEID_PLAYER)
        return true;

    return GetCharmerOrOwnerGuid().IsPlayer();
}

Player* Unit::GetCharmerOrOwnerPlayerOrPlayerItself()
{
    ObjectGuid guid = GetCharmerOrOwnerGuid();
    if (guid.IsPlayer())
        return ObjectAccessor::FindPlayer(guid);

    // If we're a creature, owned by a totem, that's owned by a player
    if (guid.IsCreature())
    {
        if (Creature* c = GetMap()->GetCreature(guid))
        {
            if (c->IsTotem() && c->GetOwnerGuid().IsPlayer())
                return ObjectAccessor::FindPlayer(c->GetOwnerGuid());
        }
    }

    return GetTypeId() == TYPEID_PLAYER ? (Player*)this : nullptr;
}

Player const* Unit::GetCharmerOrOwnerPlayerOrPlayerItself() const
{
    ObjectGuid guid = GetCharmerOrOwnerGuid();
    if (guid.IsPlayer())
        return ObjectAccessor::FindPlayer(guid);

    // If we're a creature, owned by a totem, that's owned by a player
    if (guid.IsCreature())
    {
        if (Creature* c = GetMap()->GetCreature(guid))
        {
            if (c->IsTotem() && c->GetOwnerGuid().IsPlayer())
                return ObjectAccessor::FindPlayer(c->GetOwnerGuid());
        }
    }

    return GetTypeId() == TYPEID_PLAYER ? (Player const*)this : nullptr;
}

// Returns true if unit is a player, or if owned or controlled by a player
bool Unit::player_controlled() const
{
    // TODO: Might want to cach if we're charmed/owned by a player
    return GetCharmerOrOwnerPlayerOrPlayerItself() != nullptr;
}

Player* Unit::GetAffectingPlayer() const
{
    if (!GetCharmerOrOwnerGuid())
        return GetTypeId() == TYPEID_PLAYER ? (Player*)this : nullptr;

    if (Unit* owner = GetCharmerOrOwner())
        return owner->GetCharmerOrOwnerPlayerOrPlayerItself();
    return nullptr;
}

Pet* Unit::GetPet() const
{
    if (ObjectGuid pet_guid = GetPetGuid())
    {
        if (Pet* pet = GetMap()->GetPet(pet_guid))
            return pet;
        // NOTE: pet can be non-existant if it's not yet added to the same
        // map
        //       as the owning player
    }

    return nullptr;
}

Pet* Unit::_GetPet(ObjectGuid guid) const
{
    return GetMap()->GetPet(guid);
}

Unit* Unit::GetCharm() const
{
    if (ObjectGuid charm_guid = GetCharmGuid())
    {
        if (Unit* pet = ObjectAccessor::GetUnit(*this, charm_guid))
            return pet;

        logging.error("Unit::GetCharm: Charmed %s not exist.",
            charm_guid.GetString().c_str());
        const_cast<Unit*>(this)->SetCharm(nullptr);
    }

    return nullptr;
}

void Unit::Uncharm()
{
    if (Unit* charm = GetCharm())
    {
        charm->remove_auras(SPELL_AURA_MOD_CHARM);
        charm->remove_auras(SPELL_AURA_MOD_POSSESS);
        charm->remove_auras(SPELL_AURA_MOD_POSSESS_PET);
    }
}

float Unit::GetCombatDistance(const Unit* target) const
{
    float radius = target->GetFloatValue(UNIT_FIELD_COMBATREACH) +
                   GetFloatValue(UNIT_FIELD_COMBATREACH);
    float dx = GetX() - target->GetX();
    float dy = GetY() - target->GetY();
    float dz = GetZ() - target->GetZ();
    float dist = sqrt((dx * dx) + (dy * dy) + (dz * dz)) - radius;
    return (dist > 0 ? dist : 0);
}

bool Unit::ShouldIgnoreTargetBecauseOfPvpFlag(Unit* target) const
{
    const Player* me = GetCharmerOrOwnerPlayerOrPlayerItself();
    if (!me)
        return false;

    const Player* tar = target->GetCharmerOrOwnerPlayerOrPlayerItself();
    if (!tar)
        return false;

    if (me->duel && me->duel->startTime > 0 && me->duel->startTimer == 0 &&
        me->duel->opponent == tar)
        return false;

    return !me->IsPvP();
}

void Unit::SetPet(Pet* pet)
{
    SetPetGuid(pet ? pet->GetObjectGuid() : ObjectGuid());
}

void Unit::SetCharm(Unit* pet)
{
    // health needs to be resent on charm: to go from % to exact
    if (IsInWorld())
    {
        if (pet)
            pet->resend_health();
        else if (Unit* curr_charm = GetMap()->GetUnit(GetCharmGuid()))
            curr_charm->resend_health();
    }

    SetCharmGuid(pet ? pet->GetObjectGuid() : ObjectGuid());
}

void Unit::InterruptCharms()
{
    // Removing the aura would potentially change charm_targets_,
    // so we build a vector of targets first
    std::vector<Unit*> temp;

    for (auto guid : charm_targets_)
    {
        if (Unit* target = GetMap()->GetUnit(guid))
            temp.push_back(target);
    }

    charm_targets_.clear();

    for (auto u : temp)
    {
        u->remove_auras(SPELL_AURA_MOD_CHARM);
        u->remove_auras(SPELL_AURA_AOE_CHARM);
    }
}

void Unit::AddGuardian(Pet* pet)
{
    m_guardianPets.insert(pet->GetObjectGuid());
}

void Unit::RemoveGuardian(Pet* pet)
{
    m_guardianPets.erase(pet->GetObjectGuid());
}

void Unit::RemoveGuardians()
{
    while (!m_guardianPets.empty())
    {
        ObjectGuid guid = *m_guardianPets.begin();

        if (Pet* pet = GetMap()->GetPet(guid))
            pet->Unsummon(PET_SAVE_AS_DELETED,
                this); // can remove pet guid from m_guardianPets

        m_guardianPets.erase(guid);
    }
}

Pet* Unit::FindGuardianWithEntry(uint32 entry)
{
    for (const auto& elem : m_guardianPets)
        if (Pet* pet = GetMap()->GetPet(elem))
            if (pet->GetEntry() == entry)
                return pet;

    return nullptr;
}

Unit* Unit::_GetTotem(TotemSlot slot) const
{
    return GetTotem(slot);
}

Totem* Unit::GetTotem(TotemSlot slot) const
{
    if (slot >= MAX_TOTEM_SLOT || !IsInWorld() || !m_TotemSlot[slot])
        return nullptr;

    Creature* totem = GetMap()->GetCreature(m_TotemSlot[slot]);
    return totem && totem->IsTotem() ? (Totem*)totem : nullptr;
}

bool Unit::IsAllTotemSlotsUsed() const
{
    for (auto& elem : m_TotemSlot)
        if (!elem)
            return false;
    return true;
}

void Unit::_AddTotem(TotemSlot slot, Totem* totem)
{
    m_TotemSlot[slot] = totem->GetObjectGuid();
}

void Unit::_RemoveTotem(Totem* totem)
{
    for (auto& elem : m_TotemSlot)
    {
        if (elem == totem->GetObjectGuid())
        {
            elem.Clear();
            break;
        }
    }
}

void Unit::UnsummonAllTotems()
{
    for (int i = 0; i < MAX_TOTEM_SLOT; ++i)
        if (Totem* totem = GetTotem(TotemSlot(i)))
            totem->UnSummon();
}

int32 Unit::DealHeal(Unit* pVictim, uint32 addhealth,
    SpellEntry const* spellProto, bool critical)
{
    int32 gain = pVictim->ModifyHealth(int32(addhealth));

    Unit* unit = this;

    if (GetTypeId() == TYPEID_UNIT && ((Creature*)this)->IsTotem() &&
        ((Totem*)this)->GetTotemType() != TOTEM_STATUE)
    {
        if (auto owner = GetOwner())
            unit = owner;
    }

    unit->SendHealSpellLog(pVictim, spellProto->Id, addhealth, critical);

    if (unit->GetTypeId() == TYPEID_PLAYER)
    {
        if (BattleGround* bg = ((Player*)unit)->GetBattleGround())
            bg->UpdatePlayerScore((Player*)unit, SCORE_HEALING_DONE, gain);
    }

    return gain;
}

Unit* Unit::SelectMagnetTarget(Unit* victim, Spell* spell, SpellEffectIndex eff)
{
    if (!victim)
        return nullptr;

    if (spell &&
        spell->m_spellInfo->HasAttribute(SPELL_ATTR_EX_CANT_BE_REDIRECTED))
        return victim;

    // Magic case (Note that if a spell is reflectable it's also groundable
    if (spell && (spell->m_spellInfo->DmgClass == SPELL_DAMAGE_CLASS_NONE ||
                     spell->m_spellInfo->DmgClass == SPELL_DAMAGE_CLASS_MAGIC))
    {
        if (!spell->IsReflectable())
            return victim;

        const Auras& magnetAuras =
            victim->GetAurasByType(SPELL_AURA_SPELL_MAGNET);
        for (const auto& magnetAura : magnetAuras)
        {
            if (Unit* magnet = (magnetAura)->GetCaster())
            {
                if (magnet->isAlive() && spell->CheckTarget(magnet, eff))
                {
                    if (AuraHolder* holder = (magnetAura)->GetHolder())
                        if (holder->DropAuraCharge())
                            victim->RemoveAuraHolder(
                                holder); // NOTE: Removal okay because we
                                         // return
                                         // and don't use itr again
                    spell->SetGrounded();
                    return magnet;
                }
            }
        }
    }
    // Melee && ranged case
    else
    {
        const Auras& hitTriggerAuras =
            victim->GetAurasByType(SPELL_AURA_ADD_CASTER_HIT_TRIGGER);
        for (const auto& hitTriggerAura : hitTriggerAuras)
        {
            if (Unit* magnet = (hitTriggerAura)->GetCaster())
            {
                if (magnet->isAlive() && magnet->IsWithinWmoLOSInMap(this) &&
                    (!spell || spell->CheckTarget(magnet, eff)))
                {
                    if (roll_chance_i(
                            (hitTriggerAura)->GetModifier()->m_amount))
                    {
                        if (AuraHolder* holder = (hitTriggerAura)->GetHolder())
                            if (holder->DropAuraCharge())
                                victim->RemoveAuraHolder(
                                    holder); // NOTE: Removal okay because
                                             // we
                                             // return and don't use i again
                        return magnet;
                    }
                }
            }
        }
    }

    return victim;
}

void Unit::SendHealSpellLog(
    Unit* pVictim, uint32 SpellID, uint32 Damage, bool critical)
{
    // we guess size
    WorldPacket data(SMSG_SPELLHEALLOG, (8 + 8 + 4 + 4 + 1));
    data << pVictim->GetPackGUID();
    data << GetPackGUID();
    data << uint32(SpellID);
    data << uint32(Damage);
    data << uint8(critical ? 1 : 0);
    data << uint8(0); // unused in client?
    SendMessageToSet(&data, true);
}

void Unit::SendEnergizeSpellLog(
    Unit* pVictim, uint32 SpellID, uint32 Damage, Powers powertype)
{
    WorldPacket data(SMSG_SPELLENERGIZELOG, (8 + 8 + 4 + 4 + 4 + 1));
    data << pVictim->GetPackGUID();
    data << GetPackGUID();
    data << uint32(SpellID);
    data << uint32(powertype);
    data << uint32(Damage);
    SendMessageToSet(&data, true);
}

void Unit::EnergizeBySpell(
    Unit* pVictim, uint32 SpellID, uint32 Damage, Powers powertype)
{
    SendEnergizeSpellLog(pVictim, SpellID, Damage, powertype);
    // needs to be called after sending spell log
    pVictim->ModifyPower(powertype, Damage);
}

int32 Unit::SpellBonusWithCoeffs(SpellEntry const* spellProto, int32 total,
    int32 benefit, int32 ap_benefit, DamageEffectType damagetype, bool donePart,
    Unit* pCaster)
{
    float coeff;

    // Not apply this to creature casted spells
    if (pCaster && pCaster->GetTypeId() == TYPEID_UNIT &&
        !((Creature*)pCaster)->IsPet())
        coeff = 1.0f;
    // Check for table values
    else if (SpellBonusEntry const* bonus =
                 sSpellMgr::Instance()->GetSpellBonusData(spellProto->Id))
    {
        coeff = damagetype == DOT ? bonus->dot_damage : bonus->direct_damage;

        // apply ap bonus at done part calculation only (it flat total mod
        // so
        // common with taken)
        if (donePart && (bonus->ap_bonus || bonus->ap_dot_bonus))
        {
            float ap_bonus =
                damagetype == DOT ? bonus->ap_dot_bonus : bonus->ap_bonus;

            total += int32(ap_bonus * (GetTotalAttackPowerValue(
                                           IsSpellRequiresRangedAP(spellProto) ?
                                               RANGED_ATTACK :
                                               BASE_ATTACK) +
                                          ap_benefit));
        }
    }
    // DmgClass = 0 casted by Players should not scale with spelldamage,
    // exception can be set in spell_bonus_data
    else if (GetTypeId() == TYPEID_PLAYER && donePart &&
             spellProto->DmgClass == 0)
        return total;
    // Default calculation
    else if (benefit)
        coeff = CalculateDefaultCoefficient(spellProto, damagetype);

    if (benefit && coeff > 0.0f)
    {
        // Spellmod SpellDamage
        if (Player* modOwner = GetSpellModOwner())
        {
            coeff *= 100.0f;
            modOwner->ApplySpellMod(
                spellProto->Id, SPELLMOD_SPELL_BONUS_DAMAGE, coeff);
            coeff /= 100.0f;
        }

        // Spell downranking
        float downrank = calculate_spell_downrank_factor(spellProto);

        total += int32(benefit * coeff * downrank);
    }

    return total;
}

/**
 * Calculates caster part of spell damage bonuses,
 * also includes different bonuses dependent from target auras
 */
uint32 Unit::SpellDamageBonusDone(Unit* pVictim, SpellEntry const* spellProto,
    uint32 pdamage, DamageEffectType damagetype, uint32 stack)
{
    if (!spellProto || !pVictim || damagetype == DIRECT_DAMAGE ||
        (spellProto &&
            spellProto->HasAttribute(SPELL_ATTR_EX3_NO_DAMAGE_BONUS)))
        return pdamage;

    // For totems get damage bonus from owner (statue isn't totem in fact)
    if (GetTypeId() == TYPEID_UNIT && ((Creature*)this)->IsTotem() &&
        ((Totem*)this)->GetTotemType() != TOTEM_STATUE)
    {
        if (Unit* owner = GetOwner())
            return owner->SpellDamageBonusDone(
                pVictim, spellProto, pdamage, damagetype);
    }

    float DoneTotalMod = 1.0f;
    int32 DoneTotal = 0;

    // Creature damage
    if (GetTypeId() == TYPEID_UNIT && !((Creature*)this)->IsPet())
        DoneTotalMod *=
            ((Creature*)this)
                ->GetSpellDamageMod(((Creature*)this)->GetCreatureInfo()->rank);

    const Auras& modDmgPct = GetAurasByType(SPELL_AURA_MOD_DAMAGE_PERCENT_DONE);
    for (const auto& elem : modDmgPct)
    {
        if (((elem)->GetModifier()->m_miscvalue &
                GetSpellSchoolMask(spellProto)) &&
            (elem)->GetSpellProto()->EquippedItemClass == -1 &&
            // -1 == any item class (not wand then)
            (elem)->GetSpellProto()->EquippedItemInventoryTypeMask == 0)
        // 0 == any inventory type (not wand then)
        {
            DoneTotalMod *= ((elem)->GetModifier()->m_amount + 100.0f) / 100.0f;
        }
    }

    uint32 creatureTypeMask = pVictim->GetCreatureTypeMask();
    // Add flat bonus from spell damage versus
    DoneTotal += GetTotalAuraModifierByMiscMask(
        SPELL_AURA_MOD_FLAT_SPELL_DAMAGE_VERSUS, creatureTypeMask);
    const Auras& dmgDoneVs = GetAurasByType(SPELL_AURA_MOD_DAMAGE_DONE_VERSUS);
    for (const auto& dmgDoneV : dmgDoneVs)
        if (creatureTypeMask & uint32((dmgDoneV)->GetModifier()->m_miscvalue))
            DoneTotalMod *=
                ((dmgDoneV)->GetModifier()->m_amount + 100.0f) / 100.0f;

    // Done fixed damage bonus auras
    int32 DoneAdvertisedBenefit = SpellBaseDamageBonusDone(
        GetSpellSchoolMask(spellProto), spellProto->Id);

    // done scripted mod (take it from owner)
    Unit* owner = GetOwner();
    if (!owner)
        owner = this;
    const Auras& overrideCS =
        owner->GetAurasByType(SPELL_AURA_OVERRIDE_CLASS_SCRIPTS);
    for (const auto& elem : overrideCS)
    {
        if (!(elem)->isAffectedOnSpell(spellProto))
            continue;
        switch ((elem)->GetModifier()->m_miscvalue)
        {
        // Molten Fury
        case 4920:
        case 4919:
        {
            if (pVictim->HasAuraState(AURA_STATE_HEALTHLESS_35_PERCENT))
                DoneTotalMod *=
                    (100.0f + (elem)->GetModifier()->m_amount) / 100.0f;
            break;
        }
        // Soul Siphon
        case 4992:
        case 4993:
        {
            // effect 1 m_amount
            int32 max = (elem)->GetModifier()->m_amount;
            // effect 0 m_amount
            int32 step = CalculateSpellDamage(
                this, (elem)->GetSpellProto(), EFFECT_INDEX_0);
            // count affliction effects and calc additional damage in
            // percentage
            int32 mod = 0;

            pVictim->loop_auras([&mod, step, max](AuraHolder* holder)
                {
                    const SpellEntry* info = holder->GetSpellProto();
                    if (info->SpellFamilyName != SPELLFAMILY_WARLOCK ||
                        !(info->SpellFamilyFlags & 0x71B8048C41A))
                        return true; // continue
                    mod += step * holder->GetStackAmount();
                    if (mod >= max)
                    {
                        mod = max;
                        return false; // break
                    }
                    return true;
                });

            DoneTotalMod *= (mod + 100.0f) / 100.0f;
            break;
        }
        // Starfire Bonus
        case 5481:
        {
            if (pVictim->get_aura(SPELL_AURA_PERIODIC_DAMAGE, ObjectGuid(),
                    [](AuraHolder* holder)
                    {
                        return holder->GetSpellProto()->SpellFamilyName ==
                                   SPELLFAMILY_DRUID &&
                               holder->GetSpellProto()->SpellFamilyFlags &
                                   0x0000000000200002;
                    }))
                DoneTotalMod *=
                    ((elem)->GetModifier()->m_amount + 100.0f) / 100.0f;
            break;
        }
        case 4418: // Increased Shock Damage
        case 4554: // Increased Lightning Damage
        case 4555: // Improved Moonfire
        case 5142: // Increased Lightning Damage
        case 5147: // Improved Consecration
        case 5148: // Idol of the Shooting Star
        case 6008: // Increased Lightning Damage / Totem of Hex
        {
            DoneAdvertisedBenefit += (elem)->GetModifier()->m_amount;
            break;
        }
        }
    }

    // Custom scripted damage
    switch (spellProto->SpellFamilyName)
    {
    case SPELLFAMILY_MAGE:
    {
        // Ice Lance
        if (spellProto->SpellIconID == 186)
        {
            if (pVictim->isFrozen())
                DoneTotalMod *= 3.0f;
        }
        break;
    }
    default:
        break;
    }

    // Spell bonus for pets is added as a flat amount
    if (GetTypeId() == TYPEID_UNIT && static_cast<Creature*>(this)->IsPet())
        DoneAdvertisedBenefit += static_cast<Pet*>(this)->spell_bonus();

    // apply ap bonus and benefit affected by spell power implicit coeffs
    // and
    // spell level penalties
    DoneTotal = SpellBonusWithCoeffs(spellProto, DoneTotal,
        DoneAdvertisedBenefit, 0, damagetype, true, this);

    float tmpDamage =
        (int32(pdamage) + DoneTotal * int32(stack)) * DoneTotalMod;
    // apply spellmod to Done damage (flat and pct)
    if (Player* modOwner = GetSpellModOwner())
        modOwner->ApplySpellMod(spellProto->Id,
            damagetype == DOT ? SPELLMOD_DOT : SPELLMOD_DAMAGE, tmpDamage);

    return tmpDamage > 0 ? uint32(tmpDamage) : 0;
}

/**
 * Calculates target part of spell damage bonuses,
 * will be called on each tick for periodic damage over time auras
 */
uint32 Unit::SpellDamageBonusTaken(Unit* pCaster, SpellEntry const* spellProto,
    uint32 pdamage, DamageEffectType damagetype, uint32 stack)
{
    if (!spellProto || !pCaster || damagetype == DIRECT_DAMAGE)
        return pdamage;

    uint32 schoolMask = spellProto->SchoolMask;

    // Taken total percent damage auras
    float TakenTotalMod = 1.0f;
    int32 TakenTotal = 0;

    // ..taken
    TakenTotalMod *= GetTotalAuraMultiplierByMiscMask(
        SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN, schoolMask);

    // From caster spells
    // Mod damage taken from AoE spells
    if (IsAreaOfEffectSpell(spellProto))
        TakenTotalMod *=
            GetTotalAuraMultiplier(SPELL_AURA_MOD_AOE_DAMAGE_AVOIDANCE);

    // Mangle's increase to bleed damage
    if (GetAllSpellMechanicMask(spellProto) & (1 << (MECHANIC_BLEED - 1)))
    {
        const auto& dummy_auras = GetAurasByType(SPELL_AURA_DUMMY);
        for (auto& aura : dummy_auras)
        {
            if (aura->GetSpellProto()->SpellIconID == 2312)
            {
                TakenTotalMod *=
                    (100.0f + (aura)->GetModifier()->m_amount) / 100.0f;
                break;
            }
        }
    }

    // Taken fixed damage bonus auras
    int32 TakenAdvertisedBenefit =
        SpellBaseDamageBonusTaken(GetSpellSchoolMask(spellProto));

    // apply benefit affected by spell power implicit coeffs and spell level
    // penalties
    TakenTotal = SpellBonusWithCoeffs(spellProto, TakenTotal,
        TakenAdvertisedBenefit, 0, damagetype, false, pCaster);

    float tmpDamage =
        (int32(pdamage) + TakenTotal * int32(stack)) * TakenTotalMod;

    return tmpDamage > 0 ? uint32(tmpDamage) : 0;
}

int32 Unit::SpellBaseDamageBonusDone(
    SpellSchoolMask schoolMask, uint32 Id, bool exact_mask)
{
    int32 DoneAdvertisedBenefit = 0;

    // ..done
    const Auras& dmgDone = GetAurasByType(SPELL_AURA_MOD_DAMAGE_DONE);
    for (const auto& elem : dmgDone)
    {
        auto mask = elem->GetModifier()->m_miscvalue;
        if (((exact_mask && mask == schoolMask) ||
                (!exact_mask && (mask & schoolMask) != 0)) &&
            (elem)->GetSpellProto()->EquippedItemClass ==
                -1 && // -1 == any item class (not wand then)
            (elem)->GetSpellProto()->EquippedItemInventoryTypeMask ==
                0) //  0 == any inventory type (not wand then)
            DoneAdvertisedBenefit += (elem)->GetModifier()->m_amount;
    }

    if (GetTypeId() == TYPEID_PLAYER)
    {
        // Damage bonus from stats
        const Auras& dmgDoneStatPct =
            GetAurasByType(SPELL_AURA_MOD_SPELL_DAMAGE_OF_STAT_PERCENT);
        for (const auto& elem : dmgDoneStatPct)
        {
            auto mask = elem->GetModifier()->m_miscvalue;
            if ((exact_mask && mask == schoolMask) ||
                (!exact_mask && (mask & schoolMask) != 0))
            {
                // stat used stored in miscValueB for this aura
                Stats usedStat = Stats((elem)->GetMiscBValue());
                DoneAdvertisedBenefit +=
                    int32(GetStat(usedStat) * (elem)->GetModifier()->m_amount /
                          100.0f);
            }
        }
        // ... and attack power
        const Auras& dmgDoneAP =
            GetAurasByType(SPELL_AURA_MOD_SPELL_DAMAGE_OF_ATTACK_POWER);
        for (const auto& elem : dmgDoneAP)
        {
            auto mask = elem->GetModifier()->m_miscvalue;
            if ((exact_mask && mask == schoolMask) ||
                (!exact_mask && (mask & schoolMask) != 0))
                DoneAdvertisedBenefit +=
                    int32(GetTotalAttackPowerValue(BASE_ATTACK) *
                          (elem)->GetModifier()->m_amount / 100.0f);
        }
    }

    // Spellpower Bonus from Modifiers
    Player* modOwner;
    if (Id != 0 && (modOwner = GetSpellModOwner()) != nullptr)
    {
        modOwner->ApplySpellMod(Id, SPELLMOD_SPELLPOWER, DoneAdvertisedBenefit);
    }

    return DoneAdvertisedBenefit;
}

int32 Unit::SpellBaseDamageBonusTaken(
    SpellSchoolMask schoolMask, bool exact_mask)
{
    int32 TakenAdvertisedBenefit = 0;

    // ..taken
    const Auras& dmgTaken = GetAurasByType(SPELL_AURA_MOD_DAMAGE_TAKEN);
    for (const auto& elem : dmgTaken)
    {
        auto mask = elem->GetModifier()->m_miscvalue;
        if ((exact_mask && mask == schoolMask) ||
            (!exact_mask && (mask & schoolMask) != 0))
            TakenAdvertisedBenefit += (elem)->GetModifier()->m_amount;
    }

    return TakenAdvertisedBenefit;
}

bool Unit::IsSpellCrit(Unit* pVictim, SpellEntry const* spellProto,
    SpellSchoolMask schoolMask, WeaponAttackType attackType, float crit_mod,
    const Spell* spell)
{
    // not critting spell
    if (spellProto->HasAttribute(SPELL_ATTR_EX2_CANT_CRIT))
        return false;

    // Calculate Fire totem spell's crit chance based on the Shaman's crit
    // chance
    if (spellProto->SpellFamilyName == SPELLFAMILY_SHAMAN &&
        GetTypeId() == TYPEID_UNIT && ((Creature*)this)->IsTotem() &&
        spellProto->IsFitToFamilyMask(UI64LIT(0x40000000)))
        return GetOwner()->IsSpellCrit(
            pVictim, spellProto, schoolMask, attackType, crit_mod);

    float crit_chance = 0.0f;
    switch (spellProto->DmgClass)
    {
    case SPELL_DAMAGE_CLASS_NONE:
        return false;
    case SPELL_DAMAGE_CLASS_MAGIC:
    {
        // All creatures except player pets are unable to crit
        if (GetTypeId() == TYPEID_UNIT)
        {
            bool isPlayerPet =
                ((Creature*)this)->IsPet() && ((Pet*)this)->GetOwner() &&
                ((Pet*)this)->GetOwner()->GetTypeId() == TYPEID_PLAYER;
            if (!isPlayerPet)
                return false;
        }

        if (schoolMask & SPELL_SCHOOL_MASK_NORMAL)
            crit_chance = GetUnitCriticalChance(BASE_ATTACK, pVictim);
        // For other schools
        else if (GetTypeId() == TYPEID_PLAYER)
            crit_chance = GetFloatValue(PLAYER_SPELL_CRIT_PERCENTAGE1 +
                                        GetFirstSchoolInMask(schoolMask));
        else
        {
            crit_chance = float(m_baseSpellCritChance);
            crit_chance += GetTotalAuraModifierByMiscMask(
                SPELL_AURA_MOD_SPELL_CRIT_CHANCE_SCHOOL, schoolMask);
        }
        // taken
        if (pVictim)
        {
            if (!IsPositiveSpell(spellProto->Id))
            {
                // Modify critical chance by victim
                // SPELL_AURA_MOD_ATTACKER_SPELL_CRIT_CHANCE
                crit_chance += pVictim->GetTotalAuraModifierByMiscMask(
                    SPELL_AURA_MOD_ATTACKER_SPELL_CRIT_CHANCE, schoolMask);
                // Modify critical chance by victim
                // SPELL_AURA_MOD_ATTACKER_SPELL_AND_WEAPON_CRIT_CHANCE
                crit_chance += pVictim->GetTotalAuraModifier(
                    SPELL_AURA_MOD_ATTACKER_SPELL_AND_WEAPON_CRIT_CHANCE);
                // Modify by player victim resilience
                if (pVictim->GetTypeId() == TYPEID_PLAYER)
                    crit_chance -=
                        ((Player*)pVictim)
                            ->GetRatingBonusValue(CR_CRIT_TAKEN_SPELL);
            }

            // scripted (increase crit chance ... against ... target by x%)
            const Auras& overrideCS =
                GetAurasByType(SPELL_AURA_OVERRIDE_CLASS_SCRIPTS);
            for (const auto& elem : overrideCS)
            {
                if (!((elem)->isAffectedOnSpell(spellProto)))
                    continue;
                switch ((elem)->GetModifier()->m_miscvalue)
                {
                // Shatter
                case 849:
                    if (pVictim->isFrozen())
                        crit_chance += 10.0f;
                    break;
                case 910:
                    if (pVictim->isFrozen())
                        crit_chance += 20.0f;
                    break;
                case 911:
                    if (pVictim->isFrozen())
                        crit_chance += 30.0f;
                    break;
                case 912:
                    if (pVictim->isFrozen())
                        crit_chance += 40.0f;
                    break;
                case 913:
                    if (pVictim->isFrozen())
                        crit_chance += 50.0f;
                    break;
                default:
                    break;
                }
            }
        }
        break;
    }
    case SPELL_DAMAGE_CLASS_MELEE:
    case SPELL_DAMAGE_CLASS_RANGED:
    {
        if (pVictim)
        {
            crit_chance = GetUnitCriticalChance(attackType, pVictim);
            if (!pVictim->IsStandState())
                return true; // Always crit if /sit /sleep
        }

        crit_chance += GetTotalAuraModifierByMiscMask(
            SPELL_AURA_MOD_SPELL_CRIT_CHANCE_SCHOOL, schoolMask);
        break;
    }
    default:
        return false;
    }

    crit_chance += crit_mod;

    // percent done
    // only players use intelligence for critical chance computations
    if (Player* modOwner = GetSpellModOwner())
        modOwner->ApplySpellMod(
            spellProto->Id, SPELLMOD_CRITICAL_CHANCE, crit_chance, spell);

    // Research suggests that Wands, no matter what, always have a 5% crit.
    if (spellProto->HasAttribute(
            SPELL_ATTR_EX3_REQ_WAND)) // Only Shoot has this attribute
        crit_chance = 5.0f;

    crit_chance = crit_chance > 0.0f ? crit_chance : 0.0f;
    if (roll_chance_f(crit_chance))
        return true;
    return false;
}

uint32 Unit::SpellCriticalDamageBonus(
    SpellEntry const* spellProto, uint32 damage, Unit* pVictim)
{
    // Calculate critical bonus
    int32 crit_bonus;
    switch (spellProto->DmgClass)
    {
    // for melee based spells is 100%
    case SPELL_DAMAGE_CLASS_MELEE:
    case SPELL_DAMAGE_CLASS_RANGED:
        crit_bonus = damage;
        break;
    // for spells is 50%
    default:
        crit_bonus = damage / 2;
        break;
    }

    // adds additional damage to crit_bonus (from talents)
    if (Player* modOwner = GetSpellModOwner())
        modOwner->ApplySpellMod(
            spellProto->Id, SPELLMOD_CRIT_DAMAGE_BONUS, crit_bonus);

    if (!pVictim)
        return damage += crit_bonus;

    int32 critPctDamageMod = 0;
    if (spellProto->DmgClass >= SPELL_DAMAGE_CLASS_MELEE)
    {
        if (GetWeaponAttackType(spellProto) == RANGED_ATTACK)
            critPctDamageMod += pVictim->GetTotalAuraModifier(
                SPELL_AURA_MOD_ATTACKER_RANGED_CRIT_DAMAGE);
        else
            critPctDamageMod += pVictim->GetTotalAuraModifier(
                SPELL_AURA_MOD_ATTACKER_MELEE_CRIT_DAMAGE);
    }
    else
        critPctDamageMod += pVictim->GetTotalAuraModifierByMiscMask(
            SPELL_AURA_MOD_ATTACKER_SPELL_CRIT_DAMAGE,
            GetSpellSchoolMask(spellProto));

    critPctDamageMod += GetTotalAuraModifierByMiscMask(
        SPELL_AURA_MOD_CRIT_DAMAGE_BONUS, GetSpellSchoolMask(spellProto));

    uint32 creatureTypeMask = pVictim->GetCreatureTypeMask();
    critPctDamageMod += GetTotalAuraMultiplierByMiscMask(
        SPELL_AURA_MOD_CRIT_PERCENT_VERSUS, creatureTypeMask);

    if (critPctDamageMod != 0)
        crit_bonus =
            int32(crit_bonus * float((100.0f + critPctDamageMod) / 100.0f));

    if (crit_bonus > 0)
        damage += crit_bonus;

    return damage;
}

uint32 Unit::SpellCriticalHealingBonus(
    SpellEntry const* spellProto, uint32 damage, Unit* pVictim)
{
    // Calculate critical bonus
    int32 crit_bonus;
    switch (spellProto->DmgClass)
    {
    case SPELL_DAMAGE_CLASS_MELEE: // for melee based spells is 100%
    case SPELL_DAMAGE_CLASS_RANGED:
        // TODO: write here full calculation for melee/ranged spells
        crit_bonus = damage;
        break;
    default:
        crit_bonus = damage / 2; // for spells is 50% break;
    }

    if (pVictim)
    {
        uint32 creatureTypeMask = pVictim->GetCreatureTypeMask();
        crit_bonus = int32(crit_bonus * GetTotalAuraMultiplierByMiscMask(
                                            SPELL_AURA_MOD_CRIT_PERCENT_VERSUS,
                                            creatureTypeMask));
    }

    if (crit_bonus > 0)
        damage += crit_bonus;

    return damage;
}

/**
 * Calculates caster part of healing spell bonuses,
 * also includes different bonuses dependent from target auras
 */
uint32 Unit::SpellHealingBonusDone(Unit* pVictim, SpellEntry const* spellProto,
    int32 healamount, DamageEffectType damagetype, uint32 stack)
{
    // For totems get healing bonus from owner (statue isn't totem in fact)
    if (GetTypeId() == TYPEID_UNIT && ((Creature*)this)->IsTotem() &&
        ((Totem*)this)->GetTotemType() != TOTEM_STATUE)
        if (Unit* owner = GetOwner())
            return owner->SpellHealingBonusDone(
                pVictim, spellProto, healamount, damagetype, stack);

    // No heal amount for this class spells
    if (spellProto->DmgClass == SPELL_DAMAGE_CLASS_NONE)
        return healamount < 0 ? 0 : healamount;

    // Healing Done
    // Done total percent damage auras
    float DoneTotalMod = 1.0f;
    int32 DoneTotal = 0;

    // Healing done percent
    const Auras& healingDontPct =
        GetAurasByType(SPELL_AURA_MOD_HEALING_DONE_PERCENT);
    for (const auto& elem : healingDontPct)
        DoneTotalMod *= (100.0f + (elem)->GetModifier()->m_amount) / 100.0f;

    // Done fixed damage bonus auras
    int32 DoneAdvertisedBenefit = SpellBaseHealingBonusDone(
        GetSpellSchoolMask(spellProto), spellProto->Id);

    // done scripted mod (take it from owner)
    Unit* owner = GetOwner();
    if (!owner)
        owner = this;
    const Auras& overrideCS =
        owner->GetAurasByType(SPELL_AURA_OVERRIDE_CLASS_SCRIPTS);
    for (const auto& elem : overrideCS)
    {
        if (!(elem)->isAffectedOnSpell(spellProto))
            continue;
        switch ((elem)->GetModifier()->m_miscvalue)
        {
        case 4415: // Increased Rejuvenation Healing
        case 4953:
        case 3736: // Hateful Totem of the Third Wind / Increased Lesser
                   // Healing
            // Wave / LK Arena (4/5/6) Totem of the Third Wind / Savage
            // Totem of the Third Wind
            DoneAdvertisedBenefit += (elem)->GetModifier()->m_amount;
            break;
        default:
            break;
        }
    }

    // apply ap bonus and benefit affected by spell power implicit coeffs
    // and
    // spell level penalties
    DoneTotal = SpellBonusWithCoeffs(spellProto, DoneTotal,
        DoneAdvertisedBenefit, 0, damagetype, true, this);

    // use float as more appropriate for negative values and percent
    // applying
    float heal = (healamount + DoneTotal * int32(stack)) * DoneTotalMod;
    // apply spellmod to Done amount
    if (Player* modOwner = GetSpellModOwner())
        modOwner->ApplySpellMod(spellProto->Id,
            damagetype == DOT ? SPELLMOD_DOT : SPELLMOD_DAMAGE, heal);

    return heal < 0 ? 0 : uint32(heal);
}

/**
 * Calculates target part of healing spell bonuses,
 * will be called on each tick for periodic damage over time auras
 */
uint32 Unit::SpellHealingBonusTaken(Unit* pCaster, SpellEntry const* spellProto,
    int32 healamount, DamageEffectType damagetype, uint32 stack)
{
    float TakenTotalMod = 1.0f;

    // Healing taken percent
    float minval =
        float(GetMaxNegativeAuraModifier(SPELL_AURA_MOD_HEALING_PCT));
    if (minval)
        TakenTotalMod *= (100.0f + minval) / 100.0f;

    float maxval =
        float(GetMaxPositiveAuraModifier(SPELL_AURA_MOD_HEALING_PCT));
    if (maxval)
        TakenTotalMod *= (100.0f + maxval) / 100.0f;

    // No heal amount for this class spells
    if (spellProto->DmgClass == SPELL_DAMAGE_CLASS_NONE)
    {
        healamount = int32(healamount * TakenTotalMod);
        return healamount < 0 ? 0 : healamount;
    }

    // Taken fixed damage bonus auras
    int32 TakenAdvertisedBenefit =
        SpellBaseHealingBonusTaken(GetSpellSchoolMask(spellProto));

    // Healing Done
    // Done total percent damage auras
    int32 TakenTotal = 0;

    // Blessing of Light dummy affects healing taken from Holy Light and
    // Flash
    // of Light (note: this effect is flat, and not scaling like spell
    // power)
    if (spellProto->SpellFamilyName == SPELLFAMILY_PALADIN &&
        (spellProto->SpellFamilyFlags & UI64LIT(0x00000000C0000000)))
    {
        const Auras& auraDummy = GetAurasByType(SPELL_AURA_DUMMY);
        for (const auto& elem : auraDummy)
        {
            if ((elem)->GetSpellProto()->SpellVisual == 9180)
            {
                if (((spellProto->SpellFamilyFlags &
                         UI64LIT(0x0000000040000000)) &&
                        (elem)->GetEffIndex() ==
                            EFFECT_INDEX_1)) // Flash of Light
                {
                    TakenTotal += (elem)->GetModifier()->m_amount;
                    if (pCaster->has_aura(38320)) // Libram of Souls Redeemed
                        TakenTotal += 60;
                }
                if (((spellProto->SpellFamilyFlags &
                         UI64LIT(0x0000000080000000)) &&
                        (elem)->GetEffIndex() == EFFECT_INDEX_0)) // Holy Light
                {
                    TakenTotal += (elem)->GetModifier()->m_amount;
                    if (pCaster->has_aura(38320)) // Libram of Souls Redeemed
                        TakenTotal += 120;
                }
            }
        }
    }

    // apply benefit affected by spell power implicit coeffs and spell level
    // penalties
    TakenTotal = SpellBonusWithCoeffs(spellProto, TakenTotal,
        TakenAdvertisedBenefit, 0, damagetype, false, pCaster);

    // Healing Way dummy affects healing taken from Healing Wave
    if (spellProto->SpellFamilyName == SPELLFAMILY_SHAMAN &&
        (spellProto->SpellFamilyFlags & UI64LIT(0x0000000000000040)))
    {
        const Auras& auraDummy = GetAurasByType(SPELL_AURA_DUMMY);
        for (const auto& elem : auraDummy)
            if ((elem)->GetId() == 29203)
                TakenTotalMod *=
                    ((elem)->GetModifier()->m_amount + 100.0f) / 100.0f;
    }

    // Alchemist Stone Bonus +40%
    if (spellProto->SpellFamilyName == SPELLFAMILY_POTION &&
        pCaster->has_aura(17619))
    {
        TakenTotalMod *= 1.4f;
    }

    // use float as more appropriate for negative values and percent
    // applying
    float heal = (healamount + TakenTotal * int32(stack)) * TakenTotalMod;

    return heal < 0 ? 0 : uint32(heal);
}

int32 Unit::SpellBaseHealingBonusDone(SpellSchoolMask schoolMask, uint32 Id)
{
    int32 AdvertisedBenefit = 0;

    const Auras& healingDone = GetAurasByType(SPELL_AURA_MOD_HEALING_DONE);
    for (const auto& elem : healingDone)
        if (((elem)->GetModifier()->m_miscvalue & schoolMask) != 0)
            AdvertisedBenefit += (elem)->GetModifier()->m_amount;

    // Healing bonus of spirit, intellect and strength
    if (GetTypeId() == TYPEID_PLAYER)
    {
        // Healing bonus from stats
        const Auras& healingDoneStatPct =
            GetAurasByType(SPELL_AURA_MOD_SPELL_HEALING_OF_STAT_PERCENT);
        for (const auto& elem : healingDoneStatPct)
        {
            // stat used dependent from misc value (stat index)
            Stats usedStat =
                Stats((elem)
                          ->GetSpellProto()
                          ->EffectMiscValue[(elem)->GetEffIndex()]);
            AdvertisedBenefit += int32(
                GetStat(usedStat) * (elem)->GetModifier()->m_amount / 100.0f);
        }

        // ... and attack power
        const Auras& healingDoneAP =
            GetAurasByType(SPELL_AURA_MOD_SPELL_HEALING_OF_ATTACK_POWER);
        for (const auto& elem : healingDoneAP)
            if ((elem)->GetModifier()->m_miscvalue & schoolMask)
                AdvertisedBenefit +=
                    int32(GetTotalAttackPowerValue(BASE_ATTACK) *
                          (elem)->GetModifier()->m_amount / 100.0f);
    }

    // Spellpower Bonus from Modifiers
    Player* modOwner;
    if (Id != 0 && (modOwner = GetSpellModOwner()) != nullptr)
    {
        modOwner->ApplySpellMod(Id, SPELLMOD_SPELLPOWER, AdvertisedBenefit);
    }

    return AdvertisedBenefit;
}

int32 Unit::SpellBaseHealingBonusTaken(SpellSchoolMask schoolMask)
{
    int32 AdvertisedBenefit = 0;
    const Auras& healingTaken = GetAurasByType(SPELL_AURA_MOD_HEALING);
    for (const auto& elem : healingTaken)
        if ((elem)->GetModifier()->m_miscvalue & schoolMask)
            AdvertisedBenefit += (elem)->GetModifier()->m_amount;

    return AdvertisedBenefit;
}

bool Unit::IsImmunedToDamage(SpellSchoolMask shoolMask)
{
    // If m_immuneToSchool type contain this school type, IMMUNE damage.
    SpellImmuneList const& schoolList = m_spellImmune[IMMUNITY_SCHOOL];
    for (const auto& elem : schoolList)
        if (elem.type & shoolMask)
            return true;

    // If m_immuneToDamage type contain magic, IMMUNE damage.
    SpellImmuneList const& damageList = m_spellImmune[IMMUNITY_DAMAGE];
    for (const auto& elem : damageList)
        if (elem.type & shoolMask)
            return true;

    return false;
}

bool Unit::IsImmuneToSpell(SpellEntry const* spellInfo)
{
    if (!spellInfo)
        return false;

    // You're always immune to damage if game master or in spirit of
    // redemption
    if (IsDamagingSpell(spellInfo))
    {
        if ((GetTypeId() == TYPEID_PLAYER && ((Player*)this)->isGameMaster()) ||
            GetShapeshiftForm() == FORM_SPIRITOFREDEMPTION)
            return true;
    }

    // TODO add spellEffect immunity checks!, player with flag in bg is
    // immune
    // to immunity buffs from other friendly players!
    // SpellImmuneList const& dispelList = m_spellImmune[IMMUNITY_EFFECT];

    SpellImmuneList const& dispelList = m_spellImmune[IMMUNITY_DISPEL];
    for (const auto& elem : dispelList)
        if (elem.type == spellInfo->Dispel)
            return true;

    if (!spellInfo->HasAttribute(
            SPELL_ATTR_EX_UNAFFECTED_BY_SCHOOL_IMMUNE) && // unaffected by
                                                          // school immunity
        !spellInfo->HasAttribute(
            SPELL_ATTR_EX_DISPEL_AURAS_ON_IMMUNITY)) // can remove immune
                                                     // (by
    // dispell or immune it)
    {
        SpellImmuneList const& schoolList = m_spellImmune[IMMUNITY_SCHOOL];
        for (const auto& elem : schoolList)
            if (!(IsPositiveSpell(elem.spell_id) &&
                    IsPositiveSpell(spellInfo->Id)) &&
                (elem.type & GetSpellSchoolMask(spellInfo)))
                return true;
    }
    // SPELL_ATTR_EX_UNAFFECTED_BY_SCHOOL_IMMUNE only applies if target
    // isn't
    // invulnerable (immune to everything)
    else if (spellInfo->HasAttribute(
                 SPELL_ATTR_EX_UNAFFECTED_BY_SCHOOL_IMMUNE) &&
             !spellInfo->HasAttribute(SPELL_ATTR_UNAFFECTED_BY_INVULNERABILITY))
    {
        uint32 total_mask = 0;
        const SpellImmuneList& school_list = m_spellImmune[IMMUNITY_SCHOOL];
        for (const auto& elem : school_list)
            total_mask |= elem.type;
        if (total_mask == 127)
            return true;
    }

    if (spellInfo->Mechanic &&
        IsImmuneToMechanic(Mechanics(spellInfo->Mechanic)))
        return true;

    if (GetTypeId() == TYPEID_UNIT)
    {
        // NPC Immune to Poisons
        if (spellInfo->Dispel == DISPEL_POISON &&
            static_cast<const Creature*>(this)->GetCreatureInfo()->flags_extra &
                CREATURE_FLAG_EXTRA_IMMUNE_TO_POISON)
            return true;

        // NPC Immune to Diseases
        if (spellInfo->Dispel == DISPEL_DISEASE &&
            static_cast<const Creature*>(this)->GetCreatureInfo()->flags_extra &
                CREATURE_FLAG_EXTRA_IMMUNE_TO_DISEASE)
            return true;

        // Curse of Tongues should proably be immuned fully (not just the haste)
        // despite having MOD_LANGUAGE as well (TODO: General rule)
        if (static_cast<const Creature*>(this)->GetCreatureInfo()->flags_extra &
                CREATURE_FLAG_EXTRA_IMMUNE_TO_HASTE_DECREASE &&
            (spellInfo->Id == 1714 || spellInfo->Id == 11719))
            return true;
    }

    return false;
}

bool Unit::IsImmuneToSpellEffect(
    const SpellEntry* info, SpellEffectIndex index) const
{
    // If m_immuneToEffect type contain this effect type, IMMUNE effect.
    uint32 effect = info->Effect[index];
    SpellImmuneList const& effectList = m_spellImmune[IMMUNITY_EFFECT];
    for (const auto& elem : effectList)
        if (elem.type == effect)
            return true;

    if (info->EffectMechanic[index] &&
        IsImmuneToMechanic((Mechanics)info->EffectMechanic[index]))
        return true;

    if (uint32 effect = info->Effect[index])
    {
        if (effect == SPELL_EFFECT_POWER_BURN && GetTypeId() == TYPEID_UNIT &&
            static_cast<const Creature*>(this)->GetCreatureInfo()->flags_extra &
                CREATURE_FLAG_EXTRA_IMMUNE_TO_MANA_BURN)
            return true;
    }

    if (uint32 aura = info->EffectApplyAuraName[index])
    {
        SpellImmuneList const& list = m_spellImmune[IMMUNITY_STATE];
        for (const auto& elem : list)
            if (elem.type == aura)
                return true;

        if (GetTypeId() == TYPEID_UNIT)
        {
            if (aura == SPELL_AURA_PERIODIC_LEECH &&
                static_cast<const Creature*>(this)
                        ->GetCreatureInfo()
                        ->flags_extra &
                    CREATURE_FLAG_EXTRA_IMMUNE_TO_LIFE_STEAL)
                return true;

            if (effect == SPELL_AURA_PERIODIC_MANA_LEECH &&
                static_cast<const Creature*>(this)
                        ->GetCreatureInfo()
                        ->flags_extra &
                    CREATURE_FLAG_EXTRA_IMMUNE_TO_MANA_BURN)
                return true;

            if (aura == SPELL_AURA_HASTE_SPELLS &&
                info->EffectBasePoints[index] < 0 &&
                static_cast<const Creature*>(this)
                        ->GetCreatureInfo()
                        ->flags_extra &
                    CREATURE_FLAG_EXTRA_IMMUNE_TO_HASTE_DECREASE)
                return true;
        }
    }

    return false;
}

/**
 * Returns true if friendly healing should not be able to affect the target.
 */
bool Unit::IsImmuneToHealing() const
{
    // School Immunity with bitmask 127 (e.g., Cyclone & Banish) means we
    // can
    // not heal this target
    const Auras& auraImmune = GetAurasByType(SPELL_AURA_SCHOOL_IMMUNITY);
    for (const auto& elem : auraImmune)
    {
        const SpellEntry* spellProto = (elem)->GetSpellProto();
        if ((spellProto->EffectApplyAuraName[0] == SPELL_AURA_SCHOOL_IMMUNITY &&
                spellProto->EffectMiscValue[0] == 127) ||
            (spellProto->EffectApplyAuraName[1] == SPELL_AURA_SCHOOL_IMMUNITY &&
                spellProto->EffectMiscValue[1] == 127) ||
            (spellProto->EffectApplyAuraName[2] == SPELL_AURA_SCHOOL_IMMUNITY &&
                spellProto->EffectMiscValue[2] == 127))
        {
            return true;
        }
    }

    return false;
}

bool Unit::IsImmuneToMechanic(Mechanics mechanic) const
{
    SpellImmuneList const& mechanicList = m_spellImmune[IMMUNITY_MECHANIC];
    for (const auto& elem : mechanicList)
        if (elem.type == mechanic)
            return true;

    const Auras& immuneAuraApply =
        GetAurasByType(SPELL_AURA_MECHANIC_IMMUNITY_MASK);
    for (const auto& elem : immuneAuraApply)
        if ((elem)->GetModifier()->m_miscvalue & (1 << (mechanic - 1)))
            return true;

    return false;
}

/**
 * Calculates caster part of melee damage bonuses,
 * also includes different bonuses dependent from target auras
 */
uint32 Unit::MeleeDamageBonusDone(Unit* pVictim, uint32 pdamage,
    WeaponAttackType attType, SpellEntry const* spellProto,
    DamageEffectType damagetype, uint32 stack)
{
    if (!pVictim || pdamage == 0 ||
        (spellProto &&
            spellProto->HasAttribute(SPELL_ATTR_EX3_NO_DAMAGE_BONUS)))
        return pdamage;

    // differentiate for weapon damage based spells
    bool isWeaponDamageBasedSpell =
        !(spellProto && (damagetype == DOT || IsSpellHaveEffect(spellProto,
                                                  SPELL_EFFECT_SCHOOL_DAMAGE)));
    Item* pWeapon =
        GetTypeId() == TYPEID_PLAYER ?
            ((Player*)this)->GetWeaponForAttack(attType, true, false) :
            nullptr;
    uint32 creatureTypeMask = pVictim->GetCreatureTypeMask();
    uint32 schoolMask = spellProto ?
                            spellProto->SchoolMask :
                            static_cast<uint32>(GetMeleeDamageSchoolMask());

    // FLAT damage bonus auras
    // =======================
    int32 DoneFlat = 0;
    int32 APbonus = 0;

    // ..done flat, already included in weapon damage based spells
    if (!isWeaponDamageBasedSpell)
    {
        const Auras& modDmgDone = GetAurasByType(SPELL_AURA_MOD_DAMAGE_DONE);
        for (const auto& elem : modDmgDone)
        {
            if ((elem)->GetModifier()->m_miscvalue &
                    schoolMask && // schoolmask has to fit with the
                                  // intrinsic
                                  // spell school
                (elem)->GetModifier()->m_miscvalue &
                    GetMeleeDamageSchoolMask() && // AND schoolmask has to
                                                  // fit
                // with weapon damage school
                // (essential for non-physical
                // spells)
                (((elem)->GetSpellProto()->EquippedItemClass ==
                     -1) || // general, weapon independent
                    (pWeapon &&
                        pWeapon->IsFitToSpellRequirements(
                            (elem)->GetSpellProto())))) // OR used weapon fits
                                                        // aura requirements
            {
                DoneFlat += (elem)->GetModifier()->m_amount;
            }
        }
    }

    // Apply spell power bonus for spells with coeffs in spell_bonus_data
    if (spellProto && spellProto->SchoolMask & SPELL_SCHOOL_MASK_MAGIC)
    {
        if (auto bonus =
                sSpellMgr::Instance()->GetSpellBonusData(spellProto->Id))
            if (bonus->direct_damage)
                DoneFlat += SpellBaseDamageBonusDone(
                    (SpellSchoolMask)spellProto->SchoolMask, spellProto->Id);
    }

    // Pets that do school-damage white-attacks should benefit from master's
    // spell power (spell_bonus() is already 35% of master's SP, so apply
    // flat)
    if (GetTypeId() == TYPEID_UNIT && static_cast<Creature*>(this)->IsPet() &&
        (schoolMask & SPELL_SCHOOL_MASK_NORMAL) == 0 && !spellProto)
        DoneFlat += static_cast<Pet*>(this)->spell_bonus();

    // ..done flat (by creature type mask)
    DoneFlat += GetTotalAuraModifierByMiscMask(
        SPELL_AURA_MOD_DAMAGE_DONE_CREATURE, creatureTypeMask);

    // ..done flat (base at attack power for marked target and base at
    // attack
    // power for creature type)
    if (attType == RANGED_ATTACK)
    {
        APbonus += pVictim->GetTotalAuraModifier(
            SPELL_AURA_RANGED_ATTACK_POWER_ATTACKER_BONUS);
        APbonus += GetTotalAuraModifierByMiscMask(
            SPELL_AURA_MOD_RANGED_ATTACK_POWER_VERSUS, creatureTypeMask);
    }
    else
    {
        APbonus += pVictim->GetTotalAuraModifier(
            SPELL_AURA_MELEE_ATTACK_POWER_ATTACKER_BONUS);
        APbonus += GetTotalAuraModifierByMiscMask(
            SPELL_AURA_MOD_MELEE_ATTACK_POWER_VERSUS, creatureTypeMask);
    }

    // PERCENT damage auras
    // ====================
    float DonePercent = 1.0f;

    const Auras& modDmgPctDone =
        GetAurasByType(SPELL_AURA_MOD_DAMAGE_PERCENT_DONE);
    for (const auto& elem : modDmgPctDone)
    {
        if (!isWeaponDamageBasedSpell)
        {
            if ((elem)->GetModifier()->m_miscvalue &
                    schoolMask && // schoolmask has to fit with the
                                  // intrinsic
                                  // spell school
                (elem)->GetModifier()->m_miscvalue &
                    GetMeleeDamageSchoolMask() && // AND schoolmask has to
                                                  // fit
                // with weapon damage school
                // (essential for non-physical
                // spells)
                (((elem)->GetSpellProto()->EquippedItemClass ==
                     -1) || // general, weapon independent
                    (pWeapon &&
                        pWeapon->IsFitToSpellRequirements(
                            (elem)->GetSpellProto())))) // OR used weapon fits
                                                        // aura requirements
            {
                DonePercent *=
                    ((elem)->GetModifier()->m_amount + 100.0f) / 100.0f;
            }
            else if (spellProto && spellProto->DmgClass == 2 &&
                     spellProto->SchoolMask == 2 &&
                     ((elem)->GetModifier()->m_miscvalue & schoolMask) &&
                     !((elem)->GetModifier()->m_miscvalue &
                         GetMeleeDamageSchoolMask()))
            {
                // Holy Melee Spells do not always require a weapon
                DonePercent *=
                    ((elem)->GetModifier()->m_amount + 100.0f) / 100.0f;
            }
        }
        else
        {
            // Weapon damage based spells for auras that are holy damage
            if (spellProto && spellProto->DmgClass == 2 &&
                spellProto->SchoolMask == 2 &&
                ((elem)->GetModifier()->m_miscvalue & schoolMask) &&
                !((elem)->GetModifier()->m_miscvalue &
                    GetMeleeDamageSchoolMask()))
            {
                DonePercent *=
                    ((elem)->GetModifier()->m_amount + 100.0f) / 100.0f;
            }
        }
    }

    if (!isWeaponDamageBasedSpell && attType == OFF_ATTACK)
        DonePercent *= GetModifierValue(
            UNIT_MOD_DAMAGE_OFFHAND, TOTAL_PCT); // no school check required

    // ..done pct (by creature type mask)
    DonePercent *= GetTotalAuraMultiplierByMiscMask(
        SPELL_AURA_MOD_DAMAGE_DONE_VERSUS, creatureTypeMask);

    // special dummys/class scripts and other effects
    // =============================================
    Unit* owner = GetOwner();
    if (!owner)
        owner = this;

    // Rogue's "Dirty Deeds" talent
    if (getClass() == CLASS_ROGUE && spellProto &&
        pVictim->HasAuraState(AURA_STATE_HEALTHLESS_35_PERCENT))
    {
        const auto& cs = GetAurasByType(SPELL_AURA_OVERRIDE_CLASS_SCRIPTS);
        for (const auto& elem : cs)
        {
            auto misc = elem->GetMiscValue();
            if (misc != 6427 && misc != 6428)
                continue;

            DonePercent *= (misc == 6427) ? 1.1f : 1.2f;
            break;
        }
    }

    // final calculation
    // =================

    float DoneTotal = 0.0f;

    // scaling of non weapon based spells
    if (!isWeaponDamageBasedSpell)
    {
        // apply ap bonus and benefit affected by spell power implicit coeffs
        // and spell level penalties
        DoneTotal = SpellBonusWithCoeffs(
            spellProto, DoneTotal, DoneFlat, APbonus, damagetype, true, this);
    }
    // weapon damage based spells
    else if (APbonus || DoneFlat)
    {
        bool normalized = spellProto ? IsSpellHaveEffect(spellProto,
                                           SPELL_EFFECT_NORMALIZED_WEAPON_DMG) :
                                       false;
        DoneTotal +=
            int32(APbonus / 14.0f * GetAPMultiplier(attType, normalized));

        // for weapon damage based spells we still have to apply damage done
        // percent mods
        // (that are already included into pdamage) to not-yet included
        // DoneFlat
        // e.g. from doneVersusCreature, apBonusVs...
        UnitMods unitMod;
        switch (attType)
        {
        default:
        case BASE_ATTACK:
            unitMod = UNIT_MOD_DAMAGE_MAINHAND;
            break;
        case OFF_ATTACK:
            unitMod = UNIT_MOD_DAMAGE_OFFHAND;
            break;
        case RANGED_ATTACK:
            unitMod = UNIT_MOD_DAMAGE_RANGED;
            break;
        }

        DoneTotal += DoneFlat;

        DoneTotal *= GetModifierValue(unitMod, TOTAL_PCT);
    }

    float tmpDamage =
        float(int32(pdamage) + DoneTotal * int32(stack)) * DonePercent;

    // apply spellmod to Done damage
    if (spellProto)
    {
        if (Player* modOwner = GetSpellModOwner())
            modOwner->ApplySpellMod(spellProto->Id,
                damagetype == DOT ? SPELLMOD_DOT : SPELLMOD_DAMAGE, tmpDamage);
    }

    // bonus result can be negative
    return tmpDamage > 0 ? uint32(tmpDamage) : 0;
}

/**
 * Calculates target part of melee damage bonuses,
 * will be called on each tick for periodic damage over time auras
 */
uint32 Unit::MeleeDamageBonusTaken(Unit* pCaster, uint32 pdamage,
    WeaponAttackType attType, SpellEntry const* spellProto,
    DamageEffectType damagetype, uint32 stack)
{
    if (!pCaster)
        return pdamage;

    if (pdamage == 0)
        return pdamage;

    // differentiate for weapon damage based spells
    bool isWeaponDamageBasedSpell =
        !(spellProto && (damagetype == DOT || IsSpellHaveEffect(spellProto,
                                                  SPELL_EFFECT_SCHOOL_DAMAGE)));
    uint32 schoolMask =
        spellProto ? spellProto->SchoolMask :
                     static_cast<uint32>(pCaster->GetMeleeDamageSchoolMask());
    uint32 mechanicMask = spellProto ? GetAllSpellMechanicMask(spellProto) : 0;

    // Shred also have bonus as MECHANIC_BLEED damages
    if (spellProto && spellProto->SpellFamilyName == SPELLFAMILY_DRUID &&
        spellProto->SpellFamilyFlags & UI64LIT(0x00008000))
        mechanicMask |= (1 << (MECHANIC_BLEED - 1));

    // FLAT damage bonus auras
    // =======================
    int32 TakenFlat = 0;

    // ..taken flat (base at attack power for marked target and base at
    // attack
    // power for creature type)
    if (attType == RANGED_ATTACK)
        TakenFlat += GetTotalAuraModifier(SPELL_AURA_MOD_RANGED_DAMAGE_TAKEN);
    else
        TakenFlat += GetTotalAuraModifier(SPELL_AURA_MOD_MELEE_DAMAGE_TAKEN);

    // ..taken flat (by school mask)
    TakenFlat +=
        GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_DAMAGE_TAKEN, schoolMask);

    // Weapon damage based holy spells - bonuses are hardcoded
    if ((isWeaponDamageBasedSpell && schoolMask == 2) ||
        // Same is true for Seal of Righteousness
        (spellProto && spellProto->SpellFamilyName == SPELLFAMILY_PALADIN &&
            spellProto->SpellIconID == 25 &&
            spellProto->SpellFamilyFlags == nullptr))
    {
        TakenFlat = 0;
    }

    // PERCENT damage auras
    // ====================
    float TakenPercent = 1.0f;

    // ..taken pct (by school mask)
    TakenPercent *= GetTotalAuraMultiplierByMiscMask(
        SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN, schoolMask);

    // ..taken pct (melee/ranged)
    if (attType == RANGED_ATTACK)
        TakenPercent *=
            GetTotalAuraMultiplier(SPELL_AURA_MOD_RANGED_DAMAGE_TAKEN_PCT);
    else
        TakenPercent *=
            GetTotalAuraMultiplier(SPELL_AURA_MOD_MELEE_DAMAGE_TAKEN_PCT);

    // ..taken pct (aoe avoidance)
    if (spellProto && IsAreaOfEffectSpell(spellProto))
        TakenPercent *=
            GetTotalAuraMultiplier(SPELL_AURA_MOD_AOE_DAMAGE_AVOIDANCE);

    // special dummys/class scripts and other effects
    // =============================================

    // .. taken (dummy auras)
    const Auras& dummyAuras = GetAurasByType(SPELL_AURA_DUMMY);
    for (const auto& dummyAura : dummyAuras)
    {
        switch ((dummyAura)->GetSpellProto()->SpellIconID)
        {
        case 2109: // Cheating Death
            if ((dummyAura)->GetModifier()->m_miscvalue &
                SPELL_SCHOOL_MASK_NORMAL)
            {
                if (GetTypeId() != TYPEID_PLAYER)
                    continue;

                float mod =
                    ((Player*)this)->GetRatingBonusValue(CR_CRIT_TAKEN_MELEE) *
                    (-8.0f);
                if (mod < float((dummyAura)->GetModifier()->m_amount))
                    mod = float((dummyAura)->GetModifier()->m_amount);

                TakenPercent *= (mod + 100.0f) / 100.0f;
            }
            break;
        case 2312: // Mangle
            if (mechanicMask & (1 << (MECHANIC_BLEED - 1)))
                TakenPercent *=
                    (100.0f + (dummyAura)->GetModifier()->m_amount) / 100.0f;
            break;
        }
    }

    // final calculation
    // =================

    // scaling of non weapon based spells
    if (!isWeaponDamageBasedSpell)
    {
        // apply benefit affected by spell power implicit coeffs and spell
        // level
        // penalties
        TakenFlat = SpellBonusWithCoeffs(
            spellProto, 0, TakenFlat, 0, damagetype, false, pCaster);
    }

    float tmpDamage =
        float(int32(pdamage) + TakenFlat * int32(stack)) * TakenPercent;

    // bonus result can be negative
    return tmpDamage > 0 ? uint32(tmpDamage) : 0;
}

void Unit::ApplySpellImmune(
    const Aura* owner, uint32 op, uint32 type, bool apply)
{
    if (apply)
    {
        for (auto itr = m_spellImmune[op].begin();
             itr != m_spellImmune[op].end(); ++itr)
        {
            if (itr->owner == owner && itr->type == type)
            {
                m_spellImmune[op].erase(itr);
                break;
            }
        }

        SpellImmune Immune;
        Immune.owner = owner;
        Immune.spell_id = owner ? owner->GetId() : 0;
        Immune.type = type;
        m_spellImmune[op].push_back(Immune);
    }
    else
    {
        for (auto itr = m_spellImmune[op].begin();
             itr != m_spellImmune[op].end(); ++itr)
        {
            if (itr->owner == owner && itr->type == type)
            {
                m_spellImmune[op].erase(itr);
                break;
            }
        }
    }
}

void Unit::ApplySpellDispelImmunity(
    const Aura* owner, DispelType type, bool apply)
{
    ApplySpellImmune(owner, IMMUNITY_DISPEL, type, apply);

    if (apply && owner &&
        owner->GetSpellProto()->HasAttribute(
            SPELL_ATTR_EX_DISPEL_AURAS_ON_IMMUNITY))
        remove_auras_if([type](AuraHolder* holder)
            {
                return holder->GetSpellProto()->Dispel == type;
            });
}

float Unit::GetWeaponProcChance() const
{
    // normalized proc chance for weapon attack speed
    // (odd formula...)
    if (isAttackReady(BASE_ATTACK))
        return (GetAttackTime(BASE_ATTACK) * 1.8f / 1000.0f);
    else if (haveOffhandWeapon() && isAttackReady(OFF_ATTACK))
        return (GetAttackTime(OFF_ATTACK) * 1.6f / 1000.0f);

    return 0.0f;
}

float Unit::GetPPMProcChance(WeaponAttackType type, float PPM) const
{
    if (PPM <= 0.0f)
        return 0.0f;

    // PPM Scales with haste and reflects current attack time
    auto weapon_speed = GetFloatValue(UNIT_FIELD_BASEATTACKTIME + type);

    // Expected returned range is [0, 100]
    return weapon_speed * PPM / 600.0f;
}

void Unit::Mount(uint32 mount, uint32 spellId)
{
    if (!mount)
        return;

    remove_auras_if([](AuraHolder* h)
        {
            return h->GetSpellProto()->AuraInterruptFlags &
                   AURA_INTERRUPT_FLAG_MOUNTING;
        });

    SetUInt32Value(UNIT_FIELD_MOUNTDISPLAYID, mount);

    SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_MOUNT);

    if (GetTypeId() == TYPEID_PLAYER)
    {
        // Called by Taxi system / GM command
        if (!spellId)
        {
            ((Player*)this)->UnsummonPetTemporaryIfAny();
        }
        // Called by mount aura
        else if (sSpellStore.LookupEntry(spellId))
        {
            /* Only permanent pets should be unsummoned when we mount */
            if (Pet* pet = GetPet())
            {
                if (pet->IsPermanentPetFor((Player*)this) &&
                    !((Player*)this)->InArena())
                {
                    ((Player*)this)->UnsummonPetTemporaryIfAny();
                }
                else
                {
                    pet->ApplyModeFlags(PET_MODE_DISABLE_ACTIONS, true);
                }
            }
        }
    }
}

void Unit::Unmount(bool from_aura)
{
    if (!IsMounted())
        return;

    remove_auras_if([](AuraHolder* h)
        {
            return h->GetSpellProto()->AuraInterruptFlags &
                   AURA_INTERRUPT_FLAG_NOT_MOUNTED;
        });

    SetUInt32Value(UNIT_FIELD_MOUNTDISPLAYID, 0);
    RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_MOUNT);

    // Called NOT by Taxi system / GM command
    if (from_aura)
    {
        // In retail TBC flying mounts slowed you down when you dismounted
        // (so
        // you couldn't keep the speed forward)
        // This only applied to mounts (druid form would still keep the
        // forward
        // velocity)
        // This update speed call makes that happen by ignoring the order of
        // auras in the spell_dbc and forcing a speed update prematurely.
        UpdateSpeed(MOVE_FLIGHT, true);

        WorldPacket data(SMSG_DISMOUNT, 8);
        data << GetPackGUID();
        SendMessageToSet(&data, true);
    }

    if (GetTypeId() == TYPEID_PLAYER && IsInWorld())
    {
        if (Pet* pet = GetPet())
            pet->ApplyModeFlags(PET_MODE_DISABLE_ACTIONS, false);
        else
            static_cast<Player*>(this)->ResummonPetTemporaryUnSummonedIfAny();
    }
}

void Unit::SetInCombatWith(Unit* enemy)
{
    Unit* eOwner = enemy->GetCharmerOrOwnerOrSelf();
    if (eOwner->IsPvP())
    {
        SetInCombatState(true, enemy);
        return;
    }

    // check for duel
    if (eOwner->GetTypeId() == TYPEID_PLAYER && ((Player*)eOwner)->duel)
    {
        if (Player const* myOwner = GetCharmerOrOwnerPlayerOrPlayerItself())
        {
            if (myOwner->IsInDuelWith((Player const*)eOwner))
            {
                SetInCombatState(true, enemy);
                return;
            }
        }
    }

    SetInCombatState(false, enemy);
}

void Unit::AdoptUnitCombatState(Unit* target)
{
    if (!target->getHostileRefManager().getSize())
        return;

    HostileReference* hostRef = target->getHostileRefManager().getFirst();
    while (hostRef)
    {
        Unit* enemy = hostRef->getSource()->getOwner();

        if (enemy)
        {
            Unit* enemyOwner = enemy->GetCharmerOrOwnerOrSelf();
            if (enemyOwner->IsPvP())
                SetInCombatState(true, enemy);
            else if (enemyOwner->GetTypeId() == TYPEID_PLAYER &&
                     ((Player*)enemyOwner)->duel)
            {
                if (Player const* myOwner =
                        GetCharmerOrOwnerPlayerOrPlayerItself())
                {
                    if (myOwner->IsInDuelWith((Player const*)enemyOwner))
                        SetInCombatState(true, enemy);
                }
            }
            else
                SetInCombatState(false, enemy);
            if (GetTypeId() == TYPEID_PLAYER &&
                enemy->GetTypeId() == TYPEID_UNIT && enemy->CanHaveThreatList())
                enemy->AddThreat(this); // Initial threat
        }

        hostRef = hostRef->next();
    }
}

void Unit::SetInCombatState(bool PvP, Unit* enemy)
{
    // only alive units can be in combat
    if (!isAlive() || (enemy && !enemy->isAlive()))
        return;

    if (GetTypeId() == TYPEID_UNIT && !static_cast<Creature*>(this)->IsPet() &&
        static_cast<Creature*>(this)->IsInEvadeMode())
        return;

    if (PvP)
    {
        _min_combat_timer.SetInterval(4800);
        _min_combat_timer.SetCurrent(0);
    }

    bool creatureNotInCombat = GetTypeId() == TYPEID_UNIT &&
                               !HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IN_COMBAT);

    if (!HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IN_COMBAT))
    {
        for (uint32 i = CURRENT_FIRST_NON_MELEE_SPELL; i < CURRENT_MAX_SPELL;
             ++i)
        {
            Spell* spell = GetCurrentSpell(CurrentSpellTypes(i));
            if (!spell)
                continue;
            if (IsNonCombatSpell(spell->m_spellInfo))
                InterruptSpell(CurrentSpellTypes(i), false);
            else if (i == CURRENT_CHANNELED_SPELL &&
                     spell->m_spellInfo->ChannelInterruptFlags &
                         CHANNEL_FLAG_ENTER_COMBAT)
                InterruptSpell(CurrentSpellTypes(i), false);
        }
        // FIXME: This flag does not actually mean ENTER_COMBAT!
        /*remove_auras_if([](AuraHolder* h)
            {
                return h->GetSpellProto()->AuraInterruptFlags &
                       AURA_INTERRUPT_FLAG_ENTER_COMBAT;
            });*/
    }

    SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IN_COMBAT);

    if (isCharmed() ||
        (GetTypeId() != TYPEID_PLAYER && ((Creature*)this)->IsPet()))
    {
        SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PET_IN_COMBAT);
        // Invoke combat for owner
        if (Unit* owner = GetCharmerOrOwner())
        {
            owner->SetInCombatState(PvP, enemy);
            if (enemy && owner->GetTypeId() == TYPEID_UNIT &&
                owner->IsHostileTo(enemy))
            {
                if (static_cast<Creature*>(owner)->AI() &&
                    owner->getVictim() == nullptr)
                    static_cast<Creature*>(owner)->AI()->AttackStart(enemy);
                else
                    static_cast<Creature*>(owner)->AddThreat(enemy);
            }
        }
    }

    if (creatureNotInCombat)
    {
        Creature* pCreature = (Creature*)this;

        if (!pCreature->AI() || !pCreature->AI()->IsPacified())
            SetStandState(UNIT_STAND_STATE_STAND);

        if (pCreature->AI())
            pCreature->AI()->EnterCombat(enemy);
        pCreature->ResetKitingLeashPos();

        // Set as active object. We need to update mobs in combat, or they
        // will
        // be bugged when reloaded
        if (!pCreature->isActiveObject()) // Don't set if already active
        {
            pCreature->SetActiveObjectState(true);
            pCreature->GetMap()->CreatureEnterCombat(pCreature);
        }

        if (InstanceData* mapInstance = GetInstanceData())
            mapInstance->OnCreatureEnterCombat(pCreature);
    }
}

void Unit::ClearInCombat()
{
    _min_combat_timer.SetInterval(0);
    RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IN_COMBAT);

    if (GetTypeId() == TYPEID_PLAYER)
    {
        if (isCharmed())
            RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PET_IN_COMBAT);
        static_cast<Player*>(this)->honor_clear_dmg_done();
    }
    else
    {
        if (isCharmed() || ((Creature*)this)->IsPet())
            RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PET_IN_COMBAT);

        // Player's state will be cleared in Player::UpdateContestedPvP
        clearUnitState(UNIT_STAT_ATTACK_PLAYER);

        // Reset low health speed
        ((Creature*)this)->ResetLowHealthSpeed();
        UpdateSpeed(MOVE_RUN, false);
    }
}

bool Unit::isTargetableForAttack(bool inverseAlive /*=false*/) const
{
    if (GetTypeId() == TYPEID_PLAYER && ((Player*)this)->isGameMaster())
        return false;

    if (HasFlag(UNIT_FIELD_FLAGS,
            UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE))
        return false;

    // inversealive is needed for some spells which need to be casted at
    // dead
    // targets (aoe)
    if (isAlive() == inverseAlive)
        return false;

    // If unit is dead it is not targetable, but if faking death, then they
    // are
    // targetable
    if (hasUnitState(UNIT_STAT_DIED) && !HasAuraType(SPELL_AURA_FEIGN_DEATH))
        return false;

    return IsInWorld() && !IsTaxiFlying();
}

int32 Unit::ModifyHealth(int32 dVal)
{
    int32 gain = 0;

    if (dVal == 0)
        return 0;

    int32 curHealth = (int32)GetHealth();

    int32 val = dVal + curHealth;
    if (val <= 0)
    {
        SetHealth(0);
        return -curHealth;
    }

    int32 maxHealth = (int32)GetMaxHealth();

    if (val < maxHealth)
    {
        SetHealth(val);
        gain = val - curHealth;
    }
    else if (curHealth != maxHealth)
    {
        SetHealth(maxHealth);
        gain = maxHealth - curHealth;
    }

    return gain;
}

int32 Unit::ModifyPower(Powers power, int32 dVal)
{
    int32 gain = 0;

    if (dVal == 0)
        return 0;

    int32 curPower = (int32)GetPower(power);

    int32 val = dVal + curPower;
    if (val <= 0)
    {
        SetPower(power, 0);
        return -curPower;
    }

    int32 maxPower = (int32)GetMaxPower(power);

    if (val < maxPower)
    {
        SetPower(power, val);
        gain = val - curPower;
    }
    else if (curPower != maxPower)
    {
        SetPower(power, maxPower);
        gain = maxPower - curPower;
    }

    return gain;
}

bool Unit::can_be_seen_by(const Unit* u, const WorldObject* viewPoint,
    bool inVisibleList, bool is3dDistance, bool isThreatCheck) const
{
    if (!u || !IsInMap(u) || !viewPoint)
        return false;

    // Always can see self
    if (u == this)
        return true;

    // Quest Visibility (essentially a simpler version of Phasing for TBC)
    // Checked for if one is a player and one is a creature
    if ((GetTypeId() == TYPEID_UNIT || u->GetTypeId() == TYPEID_UNIT) &&
        (GetTypeId() == TYPEID_PLAYER || u->GetTypeId() == TYPEID_PLAYER))
    {
        Player* plr = (Player*)(GetTypeId() == TYPEID_PLAYER ? this : u);
        Creature* c = (Creature*)(GetTypeId() == TYPEID_UNIT ? this : u);
        if (!plr->isGameMaster())
            if (!c->MeetsQuestVisibility(plr))
                return false;
    }

    // In arena we can only see units on our team during preparation
    if (GetMap()->IsBattleArena())
    {
        auto bg_map = dynamic_cast<BattleGroundMap*>(GetMap());
        if (bg_map && bg_map->GetBG() &&
            bg_map->GetBG()->GetStatus() < STATUS_IN_PROGRESS &&
            !IsWithinDistInMap(u, 30.0f))
            return false;
    }

    // player visible for other player if not logout and at same transport
    // including case when player is out of world
    bool at_same_transport =
        GetTypeId() == TYPEID_PLAYER && u->GetTypeId() == TYPEID_PLAYER &&
        !((Player*)this)->GetSession()->PlayerLogout() &&
        !((Player*)u)->GetSession()->PlayerLogout() &&
        !((Player*)this)->GetSession()->PlayerLoading() &&
        !((Player*)u)->GetSession()->PlayerLoading() &&
        ((Player*)this)->GetTransport() &&
        ((Player*)this)->GetTransport() == ((Player*)u)->GetTransport();

    // not in world
    if (!at_same_transport && (!IsInWorld() || !u->IsInWorld()))
        return false;

    // forbidden to seen (while Removing corpse)
    if (m_Visibility == VISIBILITY_REMOVE_CORPSE)
        return false;

    // Arena: cannot be seen by anyone while in ghost form
    if (GetTypeId() == TYPEID_PLAYER &&
        HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST) && GetMap()->IsBattleArena())
        return false;

    Map& _map = *u->GetMap();
    // Grid dead/alive checks
    if (u->GetTypeId() == TYPEID_PLAYER)
    {
        // non visible at grid for any stealth state
        if (!IsVisibleInGridForPlayer((Player*)u))
            return false;
    }
    else
    {
        // all dead creatures/players not visible for any creatures
        if (!u->isAlive() || !isAlive())
            return false;
    }

    // different visible distance checks
    if (u->IsTaxiFlying()) // what see player in flight
    {
        // use object grey distance for all (only see objects any way)
        if (!IsWithinDistInMap(viewPoint,
                World::GetMaxVisibleDistanceInFlight() +
                    (inVisibleList ? World::GetVisibleObjectGreyDistance() :
                                     0.0f),
                is3dDistance))
            return false;
    }
    else if (!at_same_transport) // distance for show player/pet/creature
                                 // (no
                                 // transport case)
    {
        // Any units far than max visible distance for viewer or not in our
        // map
        // are not visible too
        float dist = _map.GetVisibilityDistance();
        if (GetTypeId() == TYPEID_UNIT &&
            static_cast<const Creature*>(this)->special_vis_mob())
            dist = static_cast<const Creature*>(this)->special_vis_dist();
        if (viewPoint->GetTypeId() == TYPEID_UNIT &&
            static_cast<const Creature*>(viewPoint)->special_vis_mob())
            dist = static_cast<const Creature*>(viewPoint)->special_vis_dist();

        // Require object to be closer than vis dist if not at client yet,
        // to
        // prevent flickering when moving in and out
        if (!inVisibleList)
            dist -= 10.0f;

        if (!IsWithinDistInMap(viewPoint, dist, is3dDistance) &&
            !(isThreatCheck && this->GetMap()->IsDungeon()))
            return false;
    }

    // always seen by owner
    if (GetCharmerOrOwnerGuid() == u->GetObjectGuid())
        return true;

    // isInvisibleForAlive() those units can only be seen by dead or if
    // other
    // unit is also invisible for alive.. if an isinvisibleforalive unit
    // dies we
    // should be able to see it too
    if (u->isAlive() && isAlive() &&
        isInvisibleForAlive() != u->isInvisibleForAlive())
        if (u->GetTypeId() != TYPEID_PLAYER || !((Player*)u)->isGameMaster())
            return false;

    // Battleground Spirit Guides; only see your own faction's healer
    if (u->GetMap()->IsBattleGround())
    {
        auto spirit_guide_fn =
            [](const Unit* u1, const Unit* u2, uint32 entry, Team team) -> bool
        {
            return u1->GetTypeId() == TYPEID_UNIT &&
                   u2->GetTypeId() == TYPEID_PLAYER &&
                   static_cast<const Creature*>(u1)->GetEntry() == entry &&
                   static_cast<const Player*>(u2)->GetTeam() == team &&
                   !static_cast<const Player*>(u2)->isGameMaster();
        };

        if (spirit_guide_fn(this, u, 13116, HORDE) ||
            spirit_guide_fn(u, this, 13116, HORDE) ||
            spirit_guide_fn(this, u, 13117, ALLIANCE) ||
            spirit_guide_fn(u, this, 13117, ALLIANCE))
            return false;
    }

    // Visible units, always are visible for all units, except for units
    // under
    // invisibility
    if (m_Visibility == VISIBILITY_ON && u->m_invisibilityMask == 0)
        return true;

    // GMs see any players, not higher GMs and all units
    if (u->GetTypeId() == TYPEID_PLAYER && ((Player*)u)->isGameMaster())
    {
        if (GetTypeId() == TYPEID_PLAYER)
            return ((Player*)this)->GetSession()->GetSecurity() <=
                   ((Player*)u)->GetSession()->GetSecurity();
        else
            return true;
    }

    // non faction visibility non-breakable for non-GMs
    if (m_Visibility == VISIBILITY_OFF)
        return false;

    // Raw invisibility (Neither player can have invisibility, for mobs,
    // however, only the target matters)
    bool invisible;
    if (u->GetTypeId() == TYPEID_PLAYER)
        invisible = (m_invisibilityMask != 0 || u->m_invisibilityMask != 0);
    else
        invisible = m_invisibilityMask != 0;

    // detectable invisibility case
    if (invisible &&
        (
            // Invisible units, always are visible for units under same
            // invisibility type
            (m_invisibilityMask & u->m_invisibilityMask) != 0 ||
            // Invisible units, always are visible for unit that can detect
            // this
            // invisibility (have appropriate level for detect)
            u->canDetectInvisibilityOf(this) ||
            // Units that can detect invisibility always are visible for
            // units
            // that can be detected
            canDetectInvisibilityOf(u)))
    {
        invisible = false;
    }

    // special cases for always overwrite invisibility/stealth
    if (invisible || m_Visibility == VISIBILITY_GROUP_STEALTH)
    {
        // non-hostile case
        if (!u->IsHostileTo(this))
        {
            // player see other player with stealth/invisibility only if he
            // in
            // same group or raid or same team (raid/team case dependent
            // from
            // conf setting)
            if (GetTypeId() == TYPEID_PLAYER && u->GetTypeId() == TYPEID_PLAYER)
            {
                if (((Player*)this)->IsGroupVisibleFor(((Player*)u)))
                    return true;

                // else apply same rules as for hostile case (detecting
                // check
                // for stealth)
            }
        }

        // none other cases for detect invisibility, so invisible
        if (invisible)
            return false;

        // else apply stealth detecting check
    }

    if (!can_stealth_against(u))
    {
        auto itr = std::find(stealth_detected_by_.begin(),
            stealth_detected_by_.end(), u->GetObjectGuid());
        if (itr == stealth_detected_by_.end())
            const_cast<Unit*>(this)->stealth_detected_by_.push_back(
                u->GetObjectGuid());
        return true;
    }

    // unit got in stealth in this moment and must ignore old detected state
    if (m_Visibility == VISIBILITY_GROUP_NO_DETECT)
        return false;

    // GM invisibility checks early, invisibility if any detectable, so if
    // not
    // stealth then visible
    if (m_Visibility != VISIBILITY_GROUP_STEALTH)
        return true;

    // IF WE GET HERE THEN THE ONLY THING THAT CAN AFFECT VISIBILITY IS
    // STEALTH
    // STEALTH VISIBILITY IS UPDATED IN Unit::update_stealth()

    // This is a a vector of units that can currently see us despite us
    // being
    // stealthed
    return std::find(stealth_detected_by_.begin(), stealth_detected_by_.end(),
               u->GetObjectGuid()) != stealth_detected_by_.end();
}

bool Unit::can_be_hit_by_delayed_spell_stealth_check(const Unit* u) const
{
    // u: is caster of spell, *this is target of spell

    if (GetVisibility() == VISIBILITY_OFF)
        return false;

    if (GetTypeId() != TYPEID_PLAYER)
        return true;

    if (!IsHostileTo(u))
        return true;

    // Only rogue's vanish allows the ignoring of a spell
    auto aura =
        get_aura(SPELL_AURA_MOD_STEALTH, GetObjectGuid(), [](AuraHolder* holder)
            {
                return holder->GetSpellProto()->SpellIconID == 252;
            });
    if (!aura)
        return true;

    if (!can_stealth_against(u) ||
        std::find(stealth_detected_by_.begin(), stealth_detected_by_.end(),
            u->GetObjectGuid()) != stealth_detected_by_.end())
        return true;

    return false;
}

void Unit::UpdateVisibilityAndView()
{
    static const AuraType auratypes[] = {
        SPELL_AURA_BIND_SIGHT, SPELL_AURA_FAR_SIGHT, SPELL_AURA_NONE};
    for (AuraType const* type = &auratypes[0]; *type != SPELL_AURA_NONE; ++type)
    {
        const Auras& auras = m_modAuras[*type];
        if (auras.empty())
            continue;

        std::vector<AuraHolder*> remove_holders;
        for (const auto& aura : auras)
        {
            Unit* owner = (aura)->GetCaster();
            if (!owner || !can_be_seen_by(owner, this))
                remove_holders.push_back((aura)->GetHolder());
        }

        for (auto holder : remove_holders)
            RemoveAuraHolder(holder);
    }

    GetViewPoint().Call_UpdateVisibilityForOwner();
    UpdateObjectVisibility();
    ScheduleAINotify(0);
    GetViewPoint().Event_ViewPointVisibilityChanged();
}

void Unit::SetVisibility(UnitVisibility x)
{
    m_Visibility = x;

    // Visibility from stealth is not applied right away, but at next
    // update_stealth() call
    if (x == VISIBILITY_GROUP_STEALTH || x == VISIBILITY_GROUP_NO_DETECT)
        return;

    if (IsInWorld())
        UpdateVisibilityAndView();
}

bool Unit::canDetectInvisibilityOf(Unit const* u) const
{
    // You can see invisible targets with hunter's mark on them, if
    // you're in the party of the owning hunter
    bool check_hunters_mark = true;
    const auto& invis = u->GetAurasByType(SPELL_AURA_MOD_INVISIBILITY);
    for (const auto& aura : invis)
        if (aura->GetModifier()->m_amount > 9000)
            check_hunters_mark = false;
    if (check_hunters_mark)
    {
        auto& auras = u->GetAurasByType(SPELL_AURA_MOD_STALKED);
        for (auto aura : auras)
        {
            Unit* caster = aura->GetCaster();
            if (!caster)
                continue;
            if (caster == this ||
                (caster->GetTypeId() == TYPEID_PLAYER &&
                    caster->IsInPartyWith(const_cast<Unit*>(this))))
                return true;
        }
    }

    if (uint32 mask = (m_detectInvisibilityMask & u->m_invisibilityMask))
    {
        // Test invisibility detection levels
        for (int32 i = 0; i < 32; ++i)
        {
            if (((1 << i) & mask) == 0)
                continue;

            // find invisibility level
            int32 invLevel = 0;
            const Auras& modInvis =
                u->GetAurasByType(SPELL_AURA_MOD_INVISIBILITY);
            for (const auto& modInvi : modInvis)
                if ((modInvi)->GetModifier()->m_miscvalue == i &&
                    invLevel < (modInvi)->GetModifier()->m_amount)
                    invLevel = (modInvi)->GetModifier()->m_amount;

            // find invisibility detect level
            int32 detectLevel = 0;
            const Auras& modInvisDetect =
                GetAurasByType(SPELL_AURA_MOD_INVISIBILITY_DETECTION);
            for (const auto& elem : modInvisDetect)
                if ((elem)->GetModifier()->m_miscvalue == i &&
                    detectLevel < (elem)->GetModifier()->m_amount)
                    detectLevel = (elem)->GetModifier()->m_amount;

            if (i == 6 &&
                GetTypeId() == TYPEID_PLAYER) // special drunk detection case
                detectLevel = ((Player*)this)->GetDrunkValue();

            if (invLevel <= detectLevel)
                return true;
        }
    }

    return false;
}

float Unit::stealth_detect_dist(const Unit* target) const
{
    // NOTE: See doc/research/stealth.txt for details.

    auto& stealth_auras = target->GetAurasByType(SPELL_AURA_MOD_STEALTH);
    if (stealth_auras.empty())
        return MAX_VISIBILITY_DISTANCE;

    // s: stealth points, d: detection points
    float s = 0;
    float d = getLevel() * 5.0f;

    // Our most potent SPELL_AURA_MOD_STEALTH becomes our stealth points
    for (auto& a : stealth_auras)
    {
        // Boss level stealth: can never be seen
        if (a->GetModifier()->m_amount > 9000)
            return 0.0f;

        if (a->GetModifier()->m_amount > s)
            s = a->GetModifier()->m_amount;
    }

    if (HasAuraType(SPELL_AURA_DETECT_STEALTH))
        return MAX_VISIBILITY_DISTANCE;

    // Calculate our bonus detection points
    auto& detection_auras = GetAurasByType(SPELL_AURA_MOD_STEALTH_DETECT);
    for (const auto& detection_aura : detection_auras)
        if ((detection_aura)->GetModifier()->m_miscvalue == 0)
            d += (detection_aura)->GetModifier()->m_amount;

    // Calculate target's bonus stealth points
    auto& stealthlevel_auras =
        target->GetAurasByType(SPELL_AURA_MOD_STEALTH_LEVEL);
    for (const auto& stealthlevel_aura : stealthlevel_auras)
        s += (stealthlevel_aura)->GetModifier()->m_amount;

    float y = 9.0f + (d - s) * (1.5f / 5.0f);
    y = estd::rangify(1.5f, 30.0f, y);

    return y;
}

void Unit::UpdateSpeed(UnitMoveType mtype, bool forced, float ratio)
{
    float main_speed_mod = 1.0f;
    float main_passive_mod = 1.0f;
    float stack_bonus = 1.0f;
    float non_stack_bonus = 1.0f;

    switch (mtype)
    {
    case MOVE_WALK:
        break;
    case MOVE_RUN:
    {
        if (IsMounted()) // Use on mount auras
        {
            if (int32 main = GetMaxPositiveAuraModifier(
                    SPELL_AURA_MOD_INCREASE_MOUNTED_SPEED))
                main_speed_mod = (100.0f + main) / 100.0f;
            stack_bonus =
                GetTotalAuraMultiplier(SPELL_AURA_MOD_MOUNTED_SPEED_ALWAYS);
            non_stack_bonus =
                (100.0f + GetMaxPositiveAuraModifier(
                              SPELL_AURA_MOD_MOUNTED_SPEED_NOT_STACK)) /
                100.0f;
        }
        else
        {
            int32 main = 0, passive = 0;
            const auto& auras = GetAurasByType(SPELL_AURA_MOD_INCREASE_SPEED);
            for (const auto& aura : auras)
            {
                if (aura->GetHolder()->IsPassive())
                {
                    if (aura->GetModifier()->m_amount > passive)
                        passive = aura->GetModifier()->m_amount;
                }
                else
                {
                    if (aura->GetModifier()->m_amount > main)
                        main = aura->GetModifier()->m_amount;
                }
            }

            if (main)
                main_speed_mod = (100.0f + main) / 100.0f;
            if (passive)
                main_passive_mod = (100.0f + passive) / 100.0f;

            stack_bonus = GetTotalAuraMultiplier(SPELL_AURA_MOD_SPEED_ALWAYS);
            non_stack_bonus = (100.0f + GetMaxPositiveAuraModifier(
                                            SPELL_AURA_MOD_SPEED_NOT_STACK)) /
                              100.0f;
        }
        break;
    }
    case MOVE_RUN_BACK:
        return;
    case MOVE_SWIM:
    {
        if (int32 main =
                GetMaxPositiveAuraModifier(SPELL_AURA_MOD_INCREASE_SWIM_SPEED))
            main_speed_mod = (100.0f + main) / 100.0f;
        break;
    }
    case MOVE_SWIM_BACK:
        return;
    case MOVE_FLIGHT:
    {
        if (IsMounted()) // Use on mount auras
        {
            if (int32 main = GetMaxPositiveAuraModifier(
                    SPELL_AURA_MOD_FLIGHT_SPEED_MOUNTED))
                main_speed_mod = (100.0f + main) / 100.0f;
            stack_bonus = GetTotalAuraMultiplier(
                SPELL_AURA_MOD_FLIGHT_SPEED_MOUNTED_STACKING);
            non_stack_bonus =
                (100.0f +
                    GetMaxPositiveAuraModifier(
                        SPELL_AURA_MOD_FLIGHT_SPEED_MOUNTED_NOT_STACKING)) /
                100.0f;
        }
        else // Use not mount (shapeshift for example) auras (should stack)
        {
            if (int32 main = GetTotalAuraModifier(SPELL_AURA_MOD_FLIGHT_SPEED))
                main_speed_mod = (100.0f + main) / 100.0f;
            stack_bonus =
                GetTotalAuraMultiplier(SPELL_AURA_MOD_FLIGHT_SPEED_STACKING);
            non_stack_bonus =
                (100.0f + GetMaxPositiveAuraModifier(
                              SPELL_AURA_MOD_FLIGHT_SPEED_NOT_STACKING)) /
                100.0f;
        }
        break;
    }
    case MOVE_FLIGHT_BACK:
        return;
    default:
        logging.error("Unit::UpdateSpeed: Unsupported move type (%d)", mtype);
        return;
    }

    float bonus = non_stack_bonus > stack_bonus ? non_stack_bonus : stack_bonus;
    // now we ready for speed calculation
    float speed = main_speed_mod * main_passive_mod * bonus;

    switch (mtype)
    {
    case MOVE_RUN:
    case MOVE_SWIM:
    case MOVE_FLIGHT:
    {
        // Normalize speed by 191 aura SPELL_AURA_USE_NORMAL_MOVEMENT_SPEED
        // if
        // need
        // TODO: possible affect only on MOVE_RUN
        if (int32 normalization = GetMaxPositiveAuraModifier(
                SPELL_AURA_USE_NORMAL_MOVEMENT_SPEED))
        {
            // Use speed from aura
            float max_speed = normalization / baseMoveSpeed[mtype];
            if (speed > max_speed)
                speed = max_speed;
        }
        break;
    }
    default:
        break;
    }

    // for player case, we look for some custom rates
    if (GetTypeId() == TYPEID_PLAYER)
    {
        if (getDeathState() == CORPSE)
            speed *= sWorld::Instance()->getConfig(
                ((Player*)this)->InBattleGround() ?
                    CONFIG_FLOAT_GHOST_RUN_SPEED_BG :
                    CONFIG_FLOAT_GHOST_RUN_SPEED_WORLD);
    }

    // Apply strongest slow aura mod to speed
    int32 slow = GetMaxNegativeAuraModifier(SPELL_AURA_MOD_DECREASE_SPEED);
    if (slow)
        speed *= (100.0f + slow) / 100.0f;

    if (GetTypeId() == TYPEID_UNIT)
    {
        switch (mtype)
        {
        case MOVE_RUN:
            speed *= ((Creature*)this)->GetCreatureInfo()->speed_run;
            ratio *= ((Creature*)this)->GetLowHealthSpeedRate();
            break;
        case MOVE_WALK:
            speed *= ((Creature*)this)->GetCreatureInfo()->speed_walk;
            break;
        default:
            break;
        }
    }

    // If a pet is not in combat it shares the movement speed of the owner
    // (assuming it is indeed higher)
    if (mtype == MOVE_RUN || mtype == MOVE_WALK || mtype == MOVE_SWIM)
    {
        if (GetTypeId() == TYPEID_UNIT &&
            static_cast<Creature*>(this)->IsPet() && slow == 0 &&
            hasUnitState(UNIT_STAT_FOLLOW) && !isInCombat())
        {
            Unit* owner = GetOwner();
            if (owner && !owner->isInCombat() &&
                speed * ratio < owner->GetSpeedRate(mtype))
            {
                SetSpeedRate(mtype, owner->GetSpeedRate(mtype), forced);
                return;
            }
        }
    }

    SetSpeedRate(mtype, speed * ratio, forced);
}

float Unit::GetSpeed(UnitMoveType mtype) const
{
    return m_speed_rate[mtype] * baseMoveSpeed[mtype];
}

struct SetSpeedRateHelper
{
    explicit SetSpeedRateHelper(UnitMoveType _mtype, bool _forced)
      : mtype(_mtype), forced(_forced)
    {
    }
    void operator()(Unit* unit) const { unit->UpdateSpeed(mtype, forced); }
    UnitMoveType mtype;
    bool forced;
};

void Unit::SetSpeedRate(UnitMoveType mtype, float rate, bool forced)
{
    if (rate < 0)
        rate = 0.0f;

    // Update speed only on change
    if (m_speed_rate[mtype] != rate)
    {
        m_speed_rate[mtype] = rate;

        const uint16 SetSpeed2Opc_table[MAX_MOVE_TYPE][2] = {
            {MSG_MOVE_SET_WALK_SPEED, SMSG_FORCE_WALK_SPEED_CHANGE},
            {MSG_MOVE_SET_RUN_SPEED, SMSG_FORCE_RUN_SPEED_CHANGE},
            {MSG_MOVE_SET_RUN_BACK_SPEED, SMSG_FORCE_RUN_BACK_SPEED_CHANGE},
            {MSG_MOVE_SET_SWIM_SPEED, SMSG_FORCE_SWIM_SPEED_CHANGE},
            {MSG_MOVE_SET_SWIM_BACK_SPEED, SMSG_FORCE_SWIM_BACK_SPEED_CHANGE},
            {MSG_MOVE_SET_TURN_RATE, SMSG_FORCE_TURN_RATE_CHANGE},
            {MSG_MOVE_SET_FLIGHT_SPEED, SMSG_FORCE_FLIGHT_SPEED_CHANGE},
            {MSG_MOVE_SET_FLIGHT_BACK_SPEED,
             SMSG_FORCE_FLIGHT_BACK_SPEED_CHANGE},
        };

        if (forced)
        {
            if (GetTypeId() == TYPEID_PLAYER)
            {
                // register forced speed changes for
                // WorldSession::HandleForceSpeedChangeAck
                // and do it only for real sent packets and use run for
                // run/mounted as client expected
                ++((Player*)this)->m_forced_speed_changes[mtype];

                // Invalidate speed checking for warden (if one such check
                // is
                // currently pending)
                if (WorldSession* session =
                        static_cast<Player*>(this)->GetSession())
                    session->invalidate_warden_dynamic(WARDEN_DYN_CHECK_SPEEDS);
            }

            if (IsCharmerOrOwnerPlayerOrPlayerItself())
            {
                WorldPacket data(SetSpeed2Opc_table[mtype][1], 18);
                data << GetPackGUID();
                data << (uint32)0; // moveEvent, NUM_PMOVE_EVTS = 0x39
                if (mtype == MOVE_RUN)
                    data << uint8(0); // new 2.1.0
                data << float(GetSpeed(mtype));
                SendMessageToSet(&data, true);
            }
        }
        else
        {
            m_movementInfo.time = WorldTimer::getMSTime(); // FIXME: If this
                                                           // is set for
                                                           // players we
            // might see stuttering client-side;
            // since we're cutting
            // WorldSession::SynchronizeMovement
            // out of the equation

            // TODO: Actually such opcodes should (always?) be packed with
            // SMSG_COMPRESSED_MOVES
            if (IsCharmerOrOwnerPlayerOrPlayerItself())
            {
                WorldPacket data(Opcodes(SetSpeed2Opc_table[mtype][0]), 64);
                data << GetPackGUID();
                data << m_movementInfo;
                data << float(GetSpeed(mtype));
                SendMessageToSet(&data, true);
            }
        }

        if (GetTypeId() == TYPEID_PLAYER)
        {
            // Anti-Cheat: Ignore next movement update packet
            if (static_cast<Player*>(this)->move_validator)
                static_cast<Player*>(this)
                    ->move_validator->ignore_next_packet();
        }
    }

    CallForAllControlledUnits(SetSpeedRateHelper(mtype, forced),
        CONTROLLED_PET | CONTROLLED_GUARDIANS | CONTROLLED_CHARM |
            CONTROLLED_MINIPET);
}

void Unit::SetDeathState(DeathState s)
{
    if (s != ALIVE && s != JUST_ALIVED)
    {
        CombatStop();
        DeleteThreatList();
        ClearComboPointHolders(); // any combo points pointed to unit lost
                                  // at it death

        if (IsNonMeleeSpellCasted(false))
            InterruptNonMeleeSpells(false);
    }

    if (s == JUST_DIED)
    {
        remove_auras_if(
            [this](AuraHolder* h)
            {
                return !(h->IsPassive() && h->GetCaster() == this &&
                           player_or_pet()) &&
                       !h->IsDeathPersistent();
            },
            AURA_REMOVE_BY_DEATH);
        RemoveGuardians();
        UnsummonAllTotems();

        movement_gens.on_event(movement::EVENT_DEATH);

        ModifyAuraState(AURA_STATE_HEALTHLESS_20_PERCENT, false);
        ModifyAuraState(AURA_STATE_HEALTHLESS_35_PERCENT, false);
        // remove aurastates allowing special moves
        ClearAllReactives();
        ClearDiminishings();
    }
    else if (s == JUST_ALIVED)
    {
        // Clear skinnable for NPCs or players in battlegrounds
        RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SKINNABLE);
        // Reset damage taken counters for NPCs
        if (GetTypeId() == TYPEID_UNIT)
        {
            static_cast<Creature*>(this)->legit_dmg_taken = 0;
            static_cast<Creature*>(this)->total_dmg_taken = 0;
            static_cast<Creature*>(this)->player_dmg_taken = 0;
        }
    }

    if (m_deathState != ALIVE && s == ALIVE)
    {
        //_ApplyAllAuraMods();
    }
    m_deathState = s;
}

/*########################################
########                          ########
########       AGGRO SYSTEM       ########
########                          ########
########################################*/

bool Unit::CanHaveThreatList() const
{
    if (GetTypeId() != TYPEID_UNIT)
        return false;

    if (!isAlive())
        return false;

    Creature const* creature = ((Creature const*)this);

    if (creature->IsTotem())
        return false;

    if (creature->IsPlayerPet())
        return false;

    if (creature->GetCharmerGuid().IsPlayer())
        return false;

    if (creature->IsInEvadeMode())
        return false;

    return true;
}

//======================================================================

float Unit::ApplyTotalThreatModifier(float threat, SpellSchoolMask schoolMask)
{
    if (!HasAuraType(SPELL_AURA_MOD_THREAT))
        return threat;

    if (schoolMask == SPELL_SCHOOL_MASK_NONE)
        return threat;

    SpellSchools school = GetFirstSchoolInMask(schoolMask);

    return threat * m_threatModifier[school];
}

//======================================================================

void Unit::AddThreat(Unit* pVictim, float threat /*= 0.0f*/,
    bool crit /*= false*/,
    SpellSchoolMask schoolMask /*= SPELL_SCHOOL_MASK_NONE*/,
    SpellEntry const* threatSpell /*= NULL*/, bool byGroup /*= false*/,
    bool untauntable_threat /*= false*/)
{
    if (!isAlive() || !pVictim->isAlive())
        return;

    // Must be in same map to add threat
    if (!pVictim->IsInWorld() || pVictim->GetMap() != GetMap())
        return;

    // Only mobs can manage threat lists
    if (CanHaveThreatList())
    {
        // Invoke Group Event Aggro if this combat state does not already
        // stem
        // from the group
        Creature* pCreature = (Creature*)this;
        if (pCreature->GetGroup() != nullptr && !byGroup &&
            !pCreature->hasUnitState(UNIT_STAT_CONTROLLED))
        {
            // Skip group event if we have target in our threat list already
            const ThreatList& tl = getThreatManager().getThreatList();
            bool hasAlready = false;
            for (const auto& elem : tl)
            {
                if ((elem)->getTarget() == pVictim)
                {
                    hasAlready = true;
                    break;
                }
            }
            if (!hasAlready)
            {
                if (pCreature->GetGroup()->HasFlag(
                        CREATURE_GROUP_FLAG_MOVEMENT_NO_COMBAT))
                {
                    // Reset our speed rates on aggro
                    pCreature->SetSpeedRate(MOVE_WALK,
                        pCreature->GetCreatureInfo()->speed_walk, true);
                    pCreature->SetSpeedRate(MOVE_RUN,
                        pCreature->GetCreatureInfo()->speed_run, true);
                }
                else
                {
                    pCreature->GetMap()
                        ->GetCreatureGroupMgr()
                        .ProcessGroupEvent(pCreature->GetGroup()->GetId(),
                            CREATURE_GROUP_EVENT_AGGRO, pVictim);
                }
            }
        }
        else if (pCreature->GetGroup() == nullptr)
        {
            SetInCombatWith(pVictim);
            pVictim->SetInCombatWith(this);
            if (getVictim() == nullptr && pCreature->AI())
                pCreature->AI()->AttackStart(pVictim);
        }

        m_ThreatManager.addThreat(
            pVictim, threat, crit, schoolMask, threatSpell, untauntable_threat);

        // If victim is mind-controlled, we need to add threat to controller as
        // well
        if (Unit* mc = pVictim->GetCharmer())
        {
            if (mc->GetTypeId() == TYPEID_PLAYER)
                m_ThreatManager.addCharmer(mc);
        }
    }
}

//======================================================================

void Unit::DeleteThreatList()
{
    m_ThreatManager.clearReferences();
}

//======================================================================

void Unit::TauntApply(Unit* taunter)
{
    assert(GetTypeId() == TYPEID_UNIT);

    if (!taunter || (taunter->GetTypeId() == TYPEID_PLAYER &&
                        ((Player*)taunter)->isGameMaster()))
        return;

    if (!CanHaveThreatList())
        return;

    // Only attack taunter if this is a valid target and we're not currently
    // CCd
    if (!hasUnitState(UNIT_STAT_STUNNED | UNIT_STAT_DIED) &&
        !IsSecondChoiceTarget(taunter, true) && !IsAffectedByThreatIgnoringCC())
    {
        SetTargetGuid(taunter->GetObjectGuid());
        SetInFront(taunter);

        if (((Creature*)this)->AI())
            ((Creature*)this)->AI()->AttackStart(taunter);
    }

    m_ThreatManager.tauntApply(taunter);
}

//======================================================================

void Unit::TauntFadeOut(Unit* taunter)
{
    assert(GetTypeId() == TYPEID_UNIT);

    if (!taunter || (taunter->GetTypeId() == TYPEID_PLAYER &&
                        ((Player*)taunter)->isGameMaster()))
        return;

    if (!CanHaveThreatList())
        return;

    m_ThreatManager.tauntFadeOut(taunter);
}

void Unit::SaveThreatList()
{
    const auto& tl = m_ThreatManager.getThreatList();

    saved_threat.clear();
    saved_threat.reserve(tl.size());

    for (auto& entry : tl)
        saved_threat.push_back(
            std::make_pair(entry->getUnitGuid(), entry->getThreat()));
}

void Unit::RestoreThreatList()
{
    m_ThreatManager.clearReferences();
    for (auto& pair : saved_threat)
    {
        if (auto unit = GetMap()->GetUnit(pair.first))
            m_ThreatManager.addThreatDirectly(unit, pair.second);
    }
}

//======================================================================

bool Unit::IsSecondChoiceTarget(Unit* pTarget, bool checkThreatArea)
{
    assert(pTarget && GetTypeId() == TYPEID_UNIT);

    if (checkThreatArea &&
        static_cast<Creature*>(this)->IsOutOfThreatArea(pTarget))
        return true;

    if (static_cast<Creature*>(this)->AI() &&
        static_cast<Creature*>(this)->AI()->IgnoreTarget(pTarget))
        return true;

    return false;
}

//======================================================================

// FIXME: This function should be put in Creature
bool Unit::SelectHostileTarget(bool force /*= false*/)
{
    // function provides main threat functionality
    // next-victim-selection algorithm and evade mode are called
    // threat list sorting etc.

    assert(GetTypeId() == TYPEID_UNIT);

    Creature* creature = static_cast<Creature*>(this);

    if (!isAlive())
        return false;

    // This function only useful once AI has been initialized
    if (!creature->AI())
        return false;

    // Selecting hostile target should not happen if we're charmed
    if (GetCharmerGuid())
        return false;

    // Don't select new target if CC'd
    if (IsAffectedByThreatIgnoringCC() &&
        !m_ThreatManager.isThreatListEmptyOrUseless() && !force)
    {
        if (creature->evading())
            creature->stop_evade();
        return false;
    }

    // Prevent target selection if we're currently charging (unless force
    // specified)
    if (movement_gens.top_id() == movement::gen::charge && !force)
    {
        if (creature->evading())
            creature->stop_evade();
        return false;
    }

    Unit* target = nullptr;
    Unit* oldTarget = getVictim();

    // See what target the threat manager has in store for us, and verify its
    // validity
    if (!m_ThreatManager.isThreatListEmpty())
        target = m_ThreatManager.getHostileTarget();

    if (target)
    {
        if (!hasUnitState(UNIT_STAT_STUNNED | UNIT_STAT_DIED) &&
            !((Creature*)this)->AI()->IsPacified())
        {
            auto cthis = static_cast<Creature*>(this);
            bool pacified = cthis->AI()->IsPacified();
            if (!pacified && (cthis->GetCastedAtTarget().IsEmpty() ||
                                 HasAuraType(SPELL_AURA_MOD_TAUNT)))
                SetInFront(target);
            if (oldTarget != target)
                ((Creature*)this)->AI()->AttackStart(target);

            // check if currently selected target is reachable
            // NOTE: path alrteady generated from AttackStart()
            // NOTE2: In continents we need to do extra logic to see if the
            // target is flying away (which will result in an instant evade)
            if (!pacified && movement_gens.top_id() == movement::gen::chase &&
                !GetMap()->Instanceable() &&
                !m_movementInfo.HasMovementFlag(MOVEFLAG_LEVITATING))
            {
                if (target->m_movementInfo.HasMovementFlag(MOVEFLAG_FLYING2) &&
                    !CanReachWithMeleeAttack(target))
                    goto goto_evade_mode;
            }
        }
        return true;
    }

    // no target but something prevent go to evade mode
    if (!isInCombat())
        return false;

goto_evade_mode:

    // teleport to home pos if too far away & in a continent map
    if (!GetMap()->Instanceable())
    {
        if (auto gen = dynamic_cast<movement::HomeMovementGenerator*>(
                movement_gens.get(movement::gen::home)))
        {
            float x, y, z;
            gen->get_combat_start_pos(x, y, z);
            if (GetDistance(x, y, z) >= KITING_LEASH_TELEPORT)
                creature->KitingLeashTeleportHome();
        }
    }

    creature->AI()->EnterEvadeMode();

    return false;
}

//======================================================================
//======================================================================
//======================================================================

int32 Unit::CalculateSpellDamage(Unit const* target,
    SpellEntry const* spellProto, SpellEffectIndex effect_index,
    int32 const* effBasePoints)
{
    Player* unitPlayer =
        (GetTypeId() == TYPEID_PLAYER) ? (Player*)this : nullptr;

    uint8 comboPoints = unitPlayer ? unitPlayer->GetComboPoints() : 0;

    int32 level = int32(getLevel());
    if (level > (int32)spellProto->maxLevel && spellProto->maxLevel > 0)
        level = (int32)spellProto->maxLevel;
    else if (level < (int32)spellProto->baseLevel)
        level = (int32)spellProto->baseLevel;
    level -= (int32)spellProto->spellLevel;

    int32 baseDice = int32(spellProto->EffectBaseDice[effect_index]);
    float basePointsPerLevel =
        spellProto->EffectRealPointsPerLevel[effect_index];
    float randomPointsPerLevel = spellProto->EffectDicePerLevel[effect_index];
    int32 basePoints = effBasePoints ?
                           *effBasePoints - baseDice :
                           spellProto->EffectBasePoints[effect_index];

    basePoints += int32(level * basePointsPerLevel);
    int32 randomPoints = int32(spellProto->EffectDieSides[effect_index] +
                               level * randomPointsPerLevel);
    float comboDamage = spellProto->EffectPointsPerComboPoint[effect_index];

    switch (randomPoints)
    {
    case 0:
    case 1:
        basePoints += baseDice;
        break; // range 1..1
    default:
    {
        // range can have positive (1..rand) and negative (rand..1) values,
        // so
        // order its for irand
        int32 randvalue = baseDice >= randomPoints ?
                              irand(randomPoints, baseDice) :
                              irand(baseDice, randomPoints);

        basePoints += randvalue;
        break;
    }
    }

    int32 value = basePoints;

    // random damage
    if (comboDamage != 0 && unitPlayer && target &&
        (target->GetObjectGuid() == unitPlayer->GetComboTargetGuid()))
        value += (int32)(comboDamage * comboPoints);

    if (Player* modOwner = GetSpellModOwner())
    {
        modOwner->ApplySpellMod(spellProto->Id, SPELLMOD_ALL_EFFECTS, value);

        switch (effect_index)
        {
        case EFFECT_INDEX_0:
            modOwner->ApplySpellMod(spellProto->Id, SPELLMOD_EFFECT1, value);
            break;
        case EFFECT_INDEX_1:
            modOwner->ApplySpellMod(spellProto->Id, SPELLMOD_EFFECT2, value);
            break;
        case EFFECT_INDEX_2:
            modOwner->ApplySpellMod(spellProto->Id, SPELLMOD_EFFECT3, value);
            break;
        }
    }

    if (IsAffectableByLvlDmgCalc(spellProto, effect_index) &&
        spellProto->baseLevel > 0)
    {
        auto caster_level = getLevel();
        if (spellProto->maxLevel && caster_level > spellProto->maxLevel)
            caster_level = spellProto->maxLevel;

        // Damage scale an extra 64.25% for spells with base level = 20 and the
        // NPC is high-level TBC NPC
        float extra_factor = 1.0f;
        if (spellProto->baseLevel == 20 && GetTypeId() == TYPEID_UNIT &&
            static_cast<Creature*>(this)->expansion_level() == 1 &&
            caster_level > 50)
        {
            extra_factor = 1.6425f;
        }

        value *= (sSpellMgr::Instance()->spell_level_calc_damage(caster_level) /
                     sSpellMgr::Instance()->spell_level_calc_damage(
                         spellProto->baseLevel)) *
                 extra_factor;
    }

    if (GetTypeId() == TYPEID_UNIT && ((Creature*)this)->AI())
        ((Creature*)this)
            ->AI()
            ->SpellDamageCalculation(target, value, spellProto, effect_index);

    return value;
}

int32 Unit::CalculateAuraDuration(SpellEntry const* spellProto,
    uint32 effectMask, int32 duration, Unit const* /*caster*/)
{
    if (duration <= 0)
        return duration;

    // Mind flay duration not modded by MECHANIC_DURATION_MOD
    if (spellProto->SpellFamilyName == SPELLFAMILY_PRIEST &&
        spellProto->SpellFamilyFlags & 0x800000)
        return duration;

    int32 mechanicMod = 0;
    uint32 mechanicMask = GetSpellMechanicMask(spellProto, effectMask);

    for (int32 mechanic = FIRST_MECHANIC; mechanic < MAX_MECHANIC; ++mechanic)
    {
        if (!(mechanicMask & (1 << (mechanic - 1))))
            continue;

        int32 stackingMod = GetTotalAuraModifierByMiscValue(
            SPELL_AURA_MECHANIC_DURATION_MOD, mechanic);

        // Silence modifier done in SpellAuras constructor
        // TODO: Why did we opt to do this? Any reason I'm missing?
        int32 nonStackingMod = 0;
        if (mechanic != MECHANIC_SILENCE)
            nonStackingMod = GetMaxNegativeAuraModifierByMiscValue(
                SPELL_AURA_MECHANIC_DURATION_MOD_NOT_STACK, mechanic);

        mechanicMod =
            std::min(mechanicMod, std::min(stackingMod, nonStackingMod));
    }

    int32 durationMod = mechanicMod;

    if (durationMod != 0)
    {
        duration = int32(int64(duration) * (100 + durationMod) / 100);

        if (duration < 0)
            duration = 0;
    }

    return duration;
}

DiminishingLevels Unit::GetDiminishing(DiminishingGroup group)
{
    for (auto& elem : m_Diminishing)
    {
        if (elem.DRGroup != group)
            continue;

        if (!elem.hitCount || !elem.hitTime)
            return DIMINISHING_LEVEL_1;

        return static_cast<DiminishingLevels>(elem.hitCount);
    }

    return DIMINISHING_LEVEL_1;
}

void Unit::IncrDiminishing(DiminishingGroup group)
{
    for (auto& elem : m_Diminishing)
    {
        if (elem.DRGroup != group)
            continue;
        if (elem.hitCount < DIMINISHING_LEVEL_IMMUNE)
            elem.hitCount += 1;
        elem.hitTime = WorldTimer::getMSTime();
        return;
    }

    m_Diminishing.push_back(
        DiminishingReturn(group, WorldTimer::getMSTime(), DIMINISHING_LEVEL_2));
}

void Unit::RefreshDiminishing(DiminishingGroup group)
{
    for (auto& elem : m_Diminishing)
    {
        if (elem.DRGroup != group)
            continue;
        elem.hitTime = WorldTimer::getMSTime();
        return;
    }
}

void Unit::UpdateDiminishing()
{
    uint32 now = WorldTimer::getMSTime();

    for (auto i = m_Diminishing.begin(); i != m_Diminishing.end();)
    {
        // Every 5 seconds we clear DR older than or equl to 15 seconds
        if (i->hitTime + 15000 <= now)
            i = m_Diminishing.erase(i);
        else
            ++i;
    }
}

void Unit::ApplyDiminishingToDuration(DiminishingGroup group, int32& duration,
    Unit* caster, DiminishingLevels Level, bool isReflected)
{
    if (duration == -1 || group == DIMINISHING_NONE ||
        (!isReflected && caster->IsFriendlyTo(this)) ||
        !caster->player_controlled())
        return;

    Unit* owner = GetCharmerOrOwner();
    if (owner)
    {
        if (Unit* iowner = owner->GetCharmerOrOwner())
            owner = iowner;
    }
    else
        owner = this;
    bool subject_to_player_dr = owner->GetTypeId() == TYPEID_PLAYER;

    // Duration of crowd control abilities on pvp target is limited to 10 sec
    if (duration > 10000 && IsDiminishingReturnsGroupDurationLimited(group) &&
        subject_to_player_dr)
        duration = 10000;

    float mod = 1.0f;

    if ((GetDiminishingReturnsGroupType(group) == DRTYPE_PLAYER &&
            subject_to_player_dr) ||
        GetDiminishingReturnsGroupType(group) == DRTYPE_ALL)
    {
        DiminishingLevels diminish = Level;
        switch (diminish)
        {
        case DIMINISHING_LEVEL_1:
            break;
        case DIMINISHING_LEVEL_2:
            mod = 0.5f;
            break;
        case DIMINISHING_LEVEL_3:
            mod = 0.25f;
            break;
        case DIMINISHING_LEVEL_IMMUNE:
            mod = 0.0f;
            break;
        default:
            break;
        }
    }

    duration = int32(duration * mod);
}

bool Unit::isVisibleForInState(
    Player const* u, WorldObject const* viewPoint, bool inVisibleList) const
{
    return can_be_seen_by(u, viewPoint, inVisibleList, false);
}

/// returns true if creature can't be seen by alive units
bool Unit::isInvisibleForAlive() const
{
    if (m_AuraFlags & UNIT_AURAFLAG_ALIVE_INVISIBLE)
        return true;
    // TODO: maybe spiritservices also have just an aura
    return isSpiritService();
}

uint32 Unit::GetCreatureType() const
{
    if (GetTypeId() == TYPEID_PLAYER)
    {
        SpellShapeshiftFormEntry const* ssEntry =
            sSpellShapeshiftFormStore.LookupEntry(GetShapeshiftForm());
        if (ssEntry && ssEntry->creatureType > 0)
            return ssEntry->creatureType;
        else
            return CREATURE_TYPE_HUMANOID;
    }
    else
        return ((Creature*)this)->GetCreatureInfo()->type;
}

/*#######################################
########                         ########
########       STAT SYSTEM       ########
########                         ########
#######################################*/

bool Unit::HandleStatModifier(
    UnitMods unitMod, UnitModifierType modifierType, float amount, bool apply)
{
    if (unitMod >= UNIT_MOD_END || modifierType >= MODIFIER_TYPE_END)
    {
        logging.error(
            "ERROR in HandleStatModifier(): nonexistent UnitMods or wrong "
            "UnitModifierType!");
        return false;
    }

    float val = 1.0f;

    switch (modifierType)
    {
    case BASE_VALUE:
    case TOTAL_VALUE:
        m_auraModifiersGroup[unitMod][modifierType] += apply ? amount : -amount;
        break;
    case BASE_PCT:
    case TOTAL_PCT:
        if (amount <= -100.0f) // small hack-fix for -100% modifiers
            amount = -200.0f;

        val = (100.0f + amount) / 100.0f;
        m_auraModifiersGroup[unitMod][modifierType] *=
            apply ? val : (1.0f / val);
        break;

    default:
        break;
    }

    // Attack Power needs to be saved both positive and negative,
    // unfortunatley
    // the statsystem
    // cannot be efficiently extended to do this. So the solution has to be
    // seperated from the other mods.
    // See: http://www.wowwiki.com/API_UnitAttackPower and
    // http://www.wowwiki.com/API_UnitRangedAttackPower
    if (modifierType == TOTAL_VALUE &&
        (unitMod == UNIT_MOD_ATTACK_POWER ||
            unitMod == UNIT_MOD_ATTACK_POWER_RANGED))
    {
        bool rap = unitMod == UNIT_MOD_ATTACK_POWER_RANGED;
        if (amount > 0)
        {
            int index = !rap ? UNIT_AP_BUFF_POS : UNIT_RAP_BUFF_POS;
            if (apply)
                ap_buffs_[index] += (uint32)amount;
            else
                ap_buffs_[index] -= std::min((uint32)amount, ap_buffs_[index]);
        }
        else if (amount < 0)
        {
            int index = !rap ? UNIT_AP_BUFF_NEG : UNIT_RAP_BUFF_NEG;
            uint32 abs_amount = (uint32)std::abs((int32)amount);
            if (apply)
                ap_buffs_[index] += abs_amount;
            else
                ap_buffs_[index] -= std::min(abs_amount, ap_buffs_[index]);
        }
    }

    if (!CanModifyStats())
        return false;

    switch (unitMod)
    {
    case UNIT_MOD_STAT_STRENGTH:
    case UNIT_MOD_STAT_AGILITY:
    case UNIT_MOD_STAT_STAMINA:
    case UNIT_MOD_STAT_INTELLECT:
    case UNIT_MOD_STAT_SPIRIT:
        UpdateStats(GetStatByAuraGroup(unitMod));
        break;

    case UNIT_MOD_ARMOR:
        UpdateArmor();
        break;
    case UNIT_MOD_HEALTH:
        UpdateMaxHealth();
        break;

    case UNIT_MOD_MANA:
    case UNIT_MOD_RAGE:
    case UNIT_MOD_FOCUS:
    case UNIT_MOD_ENERGY:
    case UNIT_MOD_HAPPINESS:
        UpdateMaxPower(GetPowerTypeByAuraGroup(unitMod));
        break;

    case UNIT_MOD_RESISTANCE_HOLY:
    case UNIT_MOD_RESISTANCE_FIRE:
    case UNIT_MOD_RESISTANCE_NATURE:
    case UNIT_MOD_RESISTANCE_FROST:
    case UNIT_MOD_RESISTANCE_SHADOW:
    case UNIT_MOD_RESISTANCE_ARCANE:
        UpdateResistances(GetSpellSchoolByAuraGroup(unitMod));
        break;

    case UNIT_MOD_ATTACK_POWER:
        UpdateAttackPowerAndDamage();
        break;
    case UNIT_MOD_ATTACK_POWER_RANGED:
        UpdateAttackPowerAndDamage(true);
        break;

    case UNIT_MOD_DAMAGE_MAINHAND:
        UpdateDamagePhysical(BASE_ATTACK);
        break;
    case UNIT_MOD_DAMAGE_OFFHAND:
        UpdateDamagePhysical(OFF_ATTACK);
        break;
    case UNIT_MOD_DAMAGE_RANGED:
        UpdateDamagePhysical(RANGED_ATTACK);
        break;

    default:
        break;
    }

    return true;
}

float Unit::GetModifierValue(
    UnitMods unitMod, UnitModifierType modifierType) const
{
    if (unitMod >= UNIT_MOD_END || modifierType >= MODIFIER_TYPE_END)
    {
        logging.error(
            "attempt to access nonexistent modifier value from UnitMods!");
        return 0.0f;
    }

    if (modifierType == TOTAL_PCT &&
        m_auraModifiersGroup[unitMod][modifierType] <= 0.0f)
        return 0.0f;

    return m_auraModifiersGroup[unitMod][modifierType];
}

// flat_incr: pets add stats from the owner, which needs to scale with
// TOTAL_PCT
float Unit::GetTotalStatValue(Stats stat, float flat_incr) const
{
    UnitMods unitMod = UnitMods(UNIT_MOD_STAT_START + stat);

    if (m_auraModifiersGroup[unitMod][TOTAL_PCT] <= 0.0f)
        return 0.0f;

    // value = ((base_value * base_pct) + total_value) * total_pct
    float value =
        m_auraModifiersGroup[unitMod][BASE_VALUE] + GetCreateStat(stat);
    value *= m_auraModifiersGroup[unitMod][BASE_PCT];
    value += m_auraModifiersGroup[unitMod][TOTAL_VALUE];
    value += flat_incr;
    value *= m_auraModifiersGroup[unitMod][TOTAL_PCT];

    return value;
}

float Unit::GetTotalAuraModValue(UnitMods unitMod) const
{
    if (unitMod >= UNIT_MOD_END)
    {
        logging.error(
            "attempt to access nonexistent UnitMods in "
            "GetTotalAuraModValue()!");
        return 0.0f;
    }

    if (m_auraModifiersGroup[unitMod][TOTAL_PCT] <= 0.0f)
        return 0.0f;

    float value = m_auraModifiersGroup[unitMod][BASE_VALUE];
    value *= m_auraModifiersGroup[unitMod][BASE_PCT];
    value += m_auraModifiersGroup[unitMod][TOTAL_VALUE];
    value *= m_auraModifiersGroup[unitMod][TOTAL_PCT];

    return value;
}

SpellSchools Unit::GetSpellSchoolByAuraGroup(UnitMods unitMod) const
{
    SpellSchools school = SPELL_SCHOOL_NORMAL;

    switch (unitMod)
    {
    case UNIT_MOD_RESISTANCE_HOLY:
        school = SPELL_SCHOOL_HOLY;
        break;
    case UNIT_MOD_RESISTANCE_FIRE:
        school = SPELL_SCHOOL_FIRE;
        break;
    case UNIT_MOD_RESISTANCE_NATURE:
        school = SPELL_SCHOOL_NATURE;
        break;
    case UNIT_MOD_RESISTANCE_FROST:
        school = SPELL_SCHOOL_FROST;
        break;
    case UNIT_MOD_RESISTANCE_SHADOW:
        school = SPELL_SCHOOL_SHADOW;
        break;
    case UNIT_MOD_RESISTANCE_ARCANE:
        school = SPELL_SCHOOL_ARCANE;
        break;

    default:
        break;
    }

    return school;
}

Stats Unit::GetStatByAuraGroup(UnitMods unitMod) const
{
    Stats stat = STAT_STRENGTH;

    switch (unitMod)
    {
    case UNIT_MOD_STAT_STRENGTH:
        stat = STAT_STRENGTH;
        break;
    case UNIT_MOD_STAT_AGILITY:
        stat = STAT_AGILITY;
        break;
    case UNIT_MOD_STAT_STAMINA:
        stat = STAT_STAMINA;
        break;
    case UNIT_MOD_STAT_INTELLECT:
        stat = STAT_INTELLECT;
        break;
    case UNIT_MOD_STAT_SPIRIT:
        stat = STAT_SPIRIT;
        break;

    default:
        break;
    }

    return stat;
}

Powers Unit::GetPowerTypeByAuraGroup(UnitMods unitMod) const
{
    switch (unitMod)
    {
    case UNIT_MOD_MANA:
        return POWER_MANA;
    case UNIT_MOD_RAGE:
        return POWER_RAGE;
    case UNIT_MOD_FOCUS:
        return POWER_FOCUS;
    case UNIT_MOD_ENERGY:
        return POWER_ENERGY;
    case UNIT_MOD_HAPPINESS:
        return POWER_HAPPINESS;
    default:
        return POWER_MANA;
    }

    return POWER_MANA;
}

float Unit::GetTotalAttackPowerValue(WeaponAttackType attType) const
{
    if (attType == RANGED_ATTACK)
    {
        int32 ap =
            GetInt32Value(UNIT_FIELD_RANGED_ATTACK_POWER) +
            (int16)GetUInt16Value(UNIT_FIELD_RANGED_ATTACK_POWER_MODS, 0) +
            (int16)GetUInt16Value(UNIT_FIELD_RANGED_ATTACK_POWER_MODS, 1);
        if (ap < 0)
            return 0.0f;
        return ap * (1.0f + GetFloatValue(
                                UNIT_FIELD_RANGED_ATTACK_POWER_MULTIPLIER));
    }
    else
    {
        int32 ap = GetInt32Value(UNIT_FIELD_ATTACK_POWER) +
                   (int16)GetUInt16Value(UNIT_FIELD_ATTACK_POWER_MODS, 0) +
                   (int16)GetUInt16Value(UNIT_FIELD_ATTACK_POWER_MODS, 1);
        if (ap < 0)
            return 0.0f;
        return ap * (1.0f + GetFloatValue(UNIT_FIELD_ATTACK_POWER_MULTIPLIER));
    }
}

float Unit::GetWeaponDamageRange(
    WeaponAttackType attType, WeaponDamageRange type) const
{
    if (attType == OFF_ATTACK && !haveOffhandWeapon())
        return 0.0f;

    return m_weaponDamage[attType][type];
}

bool Unit::CanFreeMove() const
{
    return !hasUnitState(UNIT_STAT_CONFUSED | UNIT_STAT_FLEEING |
                         UNIT_STAT_TAXI_FLIGHT | UNIT_STAT_ROOT |
                         UNIT_STAT_STUNNED | UNIT_STAT_DISTRACTED) &&
           GetOwnerGuid().IsEmpty();
}

float Unit::GetAverageLevelHp(uint32 pLevel)
{
    switch (pLevel)
    {
    case 1:
    case 2:
    case 3:
        return 52.65208748;
    case 4:
    case 5:
    case 6:
        return 87.34090909;
    case 7:
    case 8:
    case 9:
        return 146.27683616;
    case 10:
    case 11:
    case 12:
        return 192.39074074;
    case 13:
    case 14:
    case 15:
        return 264.29081633;
    case 16:
    case 17:
    case 18:
        return 338.73602484;
    case 19:
    case 20:
    case 21:
        return 420.56190476;
    case 22:
    case 23:
    case 24:
        return 553.96180556;
    case 25:
    case 26:
    case 27:
        return 698.83076923;
    case 28:
    case 29:
    case 30:
        return 717.44528875;
    case 31:
    case 32:
    case 33:
        return 1022.85377358;
    case 34:
    case 35:
    case 36:
        return 1201.06451613;
    case 37:
    case 38:
    case 39:
        return 1431.82960894;
    case 40:
    case 41:
    case 42:
        return 1713.70397112;
    case 43:
    case 44:
    case 45:
        return 1959.40048544;
    case 46:
    case 47:
    case 48:
        return 2222.57522124;
    case 49:
    case 50:
    case 51:
        return 2336.87083333;
    case 52:
    case 53:
    case 54:
        return 2841.40740741;
    case 55:
    case 56:
    case 57:
        return 2932.03508772;
    case 58:
    case 59:
    case 60:
        return 3158.58320611;
    case 61:
    case 62:
        return 4221.35459184;
    case 63:
    case 64:
        return 4879.65476190;
    case 65:
    case 66:
        return 5486.08737864;
    case 67:
    case 68:
        return 5790.94794953;
    case 69:
    case 70:
        return 6320.24368687;
    case 71:
    case 72:
    case 73:
        return 6510.99350649;
    default:
        return 1;
    }
}

void Unit::SetLevel(uint32 lvl)
{
    SetUInt32Value(UNIT_FIELD_LEVEL, lvl);

    // group update
    if ((GetTypeId() == TYPEID_PLAYER) && ((Player*)this)->GetGroup())
        ((Player*)this)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_LEVEL);
}

void Unit::SetHealth(uint32 val)
{
    uint32 maxHealth = GetMaxHealth();
    if (maxHealth < val)
        val = maxHealth;

    SetUInt32Value(UNIT_FIELD_HEALTH, val);

    // group update
    if (GetTypeId() == TYPEID_PLAYER)
    {
        if (((Player*)this)->GetGroup())
            ((Player*)this)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_CUR_HP);
    }
    else if (((Creature*)this)->IsPet())
    {
        Pet* pet = ((Pet*)this);
        if (pet->isControlled())
        {
            Unit* owner = GetOwner();
            if (owner && (owner->GetTypeId() == TYPEID_PLAYER) &&
                ((Player*)owner)->GetGroup())
                ((Player*)owner)
                    ->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_PET_CUR_HP);
        }
    }
}

void Unit::SetMaxHealth(uint32 val)
{
    uint32 health = GetHealth();
    SetUInt32Value(UNIT_FIELD_MAXHEALTH, val);

    // group update
    if (GetTypeId() == TYPEID_PLAYER)
    {
        if (((Player*)this)->GetGroup())
            ((Player*)this)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_MAX_HP);
    }
    else if (((Creature*)this)->IsPet())
    {
        Pet* pet = ((Pet*)this);
        if (pet->isControlled())
        {
            Unit* owner = GetOwner();
            if (owner && (owner->GetTypeId() == TYPEID_PLAYER) &&
                ((Player*)owner)->GetGroup())
                ((Player*)owner)
                    ->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_PET_MAX_HP);
        }
    }

    if (val < health)
        SetHealth(val);
}

void Unit::SetHealthPercent(float percent)
{
    uint32 newHealth = GetMaxHealth() * percent / 100.0f;
    SetHealth(newHealth);
}

void Unit::SetPower(Powers power, uint32 val)
{
    if (GetPower(power) == val)
        return;

    uint32 maxPower = GetMaxPower(power);
    if (maxPower < val)
        val = maxPower;

    SetStatInt32Value(UNIT_FIELD_POWER1 + power, val);

    // group update
    if (GetTypeId() == TYPEID_PLAYER)
    {
        if (((Player*)this)->GetGroup())
            ((Player*)this)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_CUR_POWER);
    }
    else if (((Creature*)this)->IsPet())
    {
        Pet* pet = ((Pet*)this);
        if (pet->isControlled())
        {
            Unit* owner = GetOwner();
            if (owner && (owner->GetTypeId() == TYPEID_PLAYER) &&
                ((Player*)owner)->GetGroup())
                ((Player*)owner)
                    ->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_PET_CUR_POWER);
        }

        // Update the pet's character sheet with happiness damage bonus
        if (pet->getPetType() == HUNTER_PET && power == POWER_HAPPINESS)
        {
            pet->UpdateDamagePhysical(BASE_ATTACK);
        }
    }
}

void Unit::SetMaxPower(Powers power, uint32 val)
{
    uint32 cur_power = GetPower(power);
    SetStatInt32Value(UNIT_FIELD_MAXPOWER1 + power, val);

    // group update
    if (GetTypeId() == TYPEID_PLAYER)
    {
        if (((Player*)this)->GetGroup())
            ((Player*)this)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_MAX_POWER);
    }
    else if (((Creature*)this)->IsPet())
    {
        Pet* pet = ((Pet*)this);
        if (pet->isControlled())
        {
            Unit* owner = GetOwner();
            if (owner && (owner->GetTypeId() == TYPEID_PLAYER) &&
                ((Player*)owner)->GetGroup())
                ((Player*)owner)
                    ->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_PET_MAX_POWER);
        }
    }

    if (val < cur_power)
        SetPower(power, val);
}

void Unit::ApplyPowerMod(Powers power, uint32 val, bool apply)
{
    ApplyModUInt32Value(UNIT_FIELD_POWER1 + power, val, apply);

    // group update
    if (GetTypeId() == TYPEID_PLAYER)
    {
        if (((Player*)this)->GetGroup())
            ((Player*)this)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_CUR_POWER);
    }
    else if (((Creature*)this)->IsPet())
    {
        Pet* pet = ((Pet*)this);
        if (pet->isControlled())
        {
            Unit* owner = GetOwner();
            if (owner && (owner->GetTypeId() == TYPEID_PLAYER) &&
                ((Player*)owner)->GetGroup())
                ((Player*)owner)
                    ->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_PET_CUR_POWER);
        }
    }
}

void Unit::ApplyMaxPowerMod(Powers power, uint32 val, bool apply)
{
    ApplyModUInt32Value(UNIT_FIELD_MAXPOWER1 + power, val, apply);

    // group update
    if (GetTypeId() == TYPEID_PLAYER)
    {
        if (((Player*)this)->GetGroup())
            ((Player*)this)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_MAX_POWER);
    }
    else if (((Creature*)this)->IsPet())
    {
        Pet* pet = ((Pet*)this);
        if (pet->isControlled())
        {
            Unit* owner = GetOwner();
            if (owner && (owner->GetTypeId() == TYPEID_PLAYER) &&
                ((Player*)owner)->GetGroup())
                ((Player*)owner)
                    ->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_PET_MAX_POWER);
        }
    }
}

uint32 Unit::GetCreatePowers(Powers power) const
{
    switch (power)
    {
    case POWER_HEALTH:
        return 0; // is it really should be here?
    case POWER_MANA:
        return GetCreateMana();
    case POWER_RAGE:
        return 1000;
    case POWER_FOCUS:
        return (GetTypeId() == TYPEID_PLAYER ||
                        !((Creature const*)this)->IsPet() ||
                        ((Pet const*)this)->getPetType() != HUNTER_PET ?
                    0 :
                    100);
    case POWER_ENERGY:
        return 100;
    case POWER_HAPPINESS:
        return (GetTypeId() == TYPEID_PLAYER ||
                        !((Creature const*)this)->IsPet() ||
                        ((Pet const*)this)->getPetType() != HUNTER_PET ?
                    0 :
                    1050000);
    default:
        break;
    }

    return 0;
}

void Unit::AddToWorld()
{
    Object::AddToWorld();
    ScheduleAINotify(0);
}

void Unit::RemoveFromWorld()
{
    // Invalidate queued spell hits
    spell_queue_.clear();

    // cleanup
    if (IsInWorld())
    {
        Uncharm();
        RemoveGuardians();
        RemoveAllGameObjects();
        RemoveAllDynObjects();

        if (!guarding_holders_)
            CleanDisabledAuras();

        GetViewPoint().Event_RemovedFromWorld();
        remove_auras(SPELL_AURA_BIND_SIGHT);
    }

    Object::RemoveFromWorld();
}

void Unit::CleanupsBeforeDelete()
{
    // Invalidate queued spell hits
    spell_queue_.clear();

    if (m_uint32Values) // only for fully created object
    {
        InterruptNonMeleeSpells(true);
        m_Events.KillAllEvents(false); // non-delatable (currently casted
        // spells) will not deleted now but it
        // will deleted at call in
        // Map::RemoveAllObjectsInRemoveList
        CombatStop();
        ClearComboPointHolders();
        DeleteThreatList();
        if (GetTypeId() == TYPEID_PLAYER)
            getHostileRefManager().setOnlineOfflineState(false);
        else
            getHostileRefManager().deleteReferences();
        remove_auras(AURA_REMOVE_BY_DELETE);
    }
    WorldObject::CleanupsBeforeDelete();
}

CharmInfo* Unit::InitCharmInfo(Unit* charm)
{
    if (!m_charmInfo)
        m_charmInfo = new CharmInfo(charm);
    return m_charmInfo;
}

void Unit::DeleteCharmInfo()
{
    delete m_charmInfo;
    m_charmInfo = nullptr;
}

CharmInfo::CharmInfo(Unit* unit)
  : m_unit(unit), m_CommandState(COMMAND_FOLLOW), m_reactState(REACT_PASSIVE),
    m_petnumber(0), m_isReturning(false), m_stayX(0), m_stayY(0), m_stayZ(0)
{
    for (auto& elem : m_charmspells)
        elem.SetActionAndType(0, ACT_DISABLED);
}

void CharmInfo::InitPetActionBar()
{
    // the first 3 SpellOrActions are attack, follow and stay
    for (uint32 i = 0;
         i < ACTION_BAR_INDEX_PET_SPELL_START - ACTION_BAR_INDEX_START; ++i)
        SetActionBar(
            ACTION_BAR_INDEX_START + i, COMMAND_ATTACK - i, ACT_COMMAND);

    // middle 4 SpellOrActions are spells/special attacks/abilities
    for (uint32 i = 0;
         i < ACTION_BAR_INDEX_PET_SPELL_END - ACTION_BAR_INDEX_PET_SPELL_START;
         ++i)
        SetActionBar(ACTION_BAR_INDEX_PET_SPELL_START + i, 0, ACT_DISABLED);

    // last 3 SpellOrActions are reactions
    for (uint32 i = 0;
         i < ACTION_BAR_INDEX_END - ACTION_BAR_INDEX_PET_SPELL_END; ++i)
        SetActionBar(ACTION_BAR_INDEX_PET_SPELL_END + i, COMMAND_ATTACK - i,
            ACT_REACTION);
}

void CharmInfo::InitEmptyActionBar()
{
    SetActionBar(ACTION_BAR_INDEX_START, COMMAND_ATTACK, ACT_COMMAND);
    for (uint32 x = ACTION_BAR_INDEX_START + 1; x < ACTION_BAR_INDEX_END; ++x)
        SetActionBar(x, 0, ACT_PASSIVE);
}

void CharmInfo::InitPossessCreateSpells()
{
    InitEmptyActionBar(); // charm action bar

    if (m_unit->GetTypeId() == TYPEID_PLAYER) // possessed players don't have
                                              // spells, keep the action bar
                                              // empty
        return;

    for (uint32 i = 0; i < CREATURE_MAX_SPELLS; ++i)
    {
        uint32 spell = static_cast<Creature*>(m_unit)->m_spells[i];
        if (spell == 0 && static_cast<Creature*>(m_unit)->GetCreatureInfo())
            spell =
                static_cast<Creature*>(m_unit)->GetCreatureInfo()->spells[i];

        if (IsPassiveSpell(spell))
            m_unit->CastSpell(m_unit, spell, true);
        else
            AddSpellToActionBar(spell, ACT_PASSIVE);
    }
}

void CharmInfo::InitCharmCreateSpells()
{
    if (m_unit->GetTypeId() ==
        TYPEID_PLAYER) // charmed players don't have spells
    {
        InitEmptyActionBar();
        return;
    }

    // If mob is a part of chess event we need to give them a special action
    // bar
    // (Minus the normal pet commands)
    bool initPetAB = true;
    if (m_unit->GetMapId() == 532)
    {
        // All Chess pieces:
        uint32 e = m_unit->GetEntry();
        if (e == 21684 || e == 21683 || e == 21682 || e == 21664 ||
            e == 21160 || e == 17211 || e == 21752 || e == 21750 ||
            e == 21747 || e == 21748 || e == 21726 || e == 17469)
        {
            initPetAB = false;
            // Don't give them a normal pet action bar:
            for (uint32 i = ACTION_BAR_INDEX_START; i < ACTION_BAR_INDEX_END;
                 ++i)
                SetActionBar(i, 0, ACT_DISABLED);
        }
    }

    if (initPetAB)
        InitPetActionBar();

    for (uint32 x = 0; x < CREATURE_MAX_SPELLS; ++x)
    {
        uint32 spellId = ((Creature*)m_unit)->m_spells[x];

        if (!spellId)
        {
            m_charmspells[x].SetActionAndType(spellId, ACT_DISABLED);
            continue;
        }

        const SpellEntry* spellInfo = sSpellStore.LookupEntry(spellId);

        if (spellInfo->Attributes & SPELL_ATTR_PASSIVE)
        {
            m_unit->CastSpell(m_unit, spellId, true);
            m_charmspells[x].SetActionAndType(spellId, ACT_PASSIVE);
        }
        else if (spellInfo->AttributesEx & SPELL_ATTR_EX_NOT_AUTOCASTABLE)
        {
            m_charmspells[x].SetActionAndType(spellId, ACT_PASSIVE);
            AddSpellToActionBar(spellId, ACT_PASSIVE);
        }
        else
        {
            m_charmspells[x].SetActionAndType(spellId, ACT_DISABLED);

            ActiveStates newstate;
            bool onlyselfcast = true;

            if (!spellInfo)
                onlyselfcast = false;
            for (uint32 i = 0; i < 3 && onlyselfcast;
                 ++i) // nonexistent spell will not make any problems as
                      // onlyselfcast would be false -> break right away
            {
                if (spellInfo->EffectImplicitTargetA[i] != TARGET_SELF &&
                    spellInfo->EffectImplicitTargetA[i] != 0)
                    onlyselfcast = false;
            }

            if (onlyselfcast || !IsPositiveSpell(spellId)) // only self cast and
                                                           // spells versus
                                                           // enemies are
                                                           // autocastable
                newstate = ACT_DISABLED;
            else
                newstate = ACT_PASSIVE;

            AddSpellToActionBar(spellId, newstate);
        }
    }
}

bool CharmInfo::AddSpellToActionBar(uint32 spell_id, ActiveStates newstate)
{
    uint32 first_id = sSpellMgr::Instance()->GetFirstSpellInChain(spell_id);
    const SpellEntry* spellInfo = sSpellStore.LookupEntry(spell_id);

    // Decide state if we're passed an undecided one
    if (newstate == ACT_DECIDE)
    {
        if (spellInfo->AttributesEx & SPELL_ATTR_EX_NOT_AUTOCASTABLE ||
            spellInfo->Attributes & SPELL_ATTR_PASSIVE)
            newstate = ACT_PASSIVE;
        else
            newstate = ACT_DISABLED;
    }

    // new spell rank can be already listed
    for (auto& elem : PetActionBar)
    {
        if (uint32 action = elem.GetAction())
        {
            if (elem.IsActionBarForSpell() &&
                sSpellMgr::Instance()->GetFirstSpellInChain(action) == first_id)
            {
                elem.SetActionAndType(spell_id, newstate);
                return true;
            }
        }
    }

    // or use empty slot in other case
    for (uint8 i = 0; i < MAX_UNIT_ACTION_BAR_INDEX; ++i)
    {
        if (!PetActionBar[i].GetAction() &&
            PetActionBar[i].IsActionBarForSpell())
        {
            SetActionBar(i, spell_id, newstate);
            return true;
        }
    }
    return false;
}

bool CharmInfo::RemoveSpellFromActionBar(uint32 spell_id)
{
    uint32 first_id = sSpellMgr::Instance()->GetFirstSpellInChain(spell_id);

    for (uint8 i = 0; i < MAX_UNIT_ACTION_BAR_INDEX; ++i)
    {
        if (uint32 action = PetActionBar[i].GetAction())
        {
            if (PetActionBar[i].IsActionBarForSpell() &&
                sSpellMgr::Instance()->GetFirstSpellInChain(action) == first_id)
            {
                SetActionBar(i, 0, ACT_DISABLED);
                return true;
            }
        }
    }

    return false;
}

void CharmInfo::ToggleCreatureAutocast(uint32 spellid, bool apply)
{
    const SpellEntry* spellInfo = sSpellStore.LookupEntry(spellid);
    if (!spellInfo)
        return;
    if (spellInfo->Attributes & SPELL_ATTR_PASSIVE ||
        spellInfo->AttributesEx & SPELL_ATTR_EX_NOT_AUTOCASTABLE)
        return;

    for (auto& elem : m_charmspells)
        if (spellid == elem.GetAction())
            elem.SetType(apply ? ACT_ENABLED : ACT_DISABLED);
}

void CharmInfo::SetPetNumber(uint32 petnumber, bool statwindow)
{
    m_petnumber = petnumber;
    if (statwindow)
        m_unit->SetUInt32Value(UNIT_FIELD_PETNUMBER, m_petnumber);
    else
        m_unit->SetUInt32Value(UNIT_FIELD_PETNUMBER, 0);
}

void CharmInfo::LoadPetActionBar(const std::string& data)
{
    InitPetActionBar();

    Tokens tokens = StrSplit(data, " ");

    if (tokens.size() != (ACTION_BAR_INDEX_END - ACTION_BAR_INDEX_START) * 2)
        return; // non critical, will reset to default

    int index;
    Tokens::iterator iter;
    for (iter = tokens.begin(), index = ACTION_BAR_INDEX_START;
         index < ACTION_BAR_INDEX_END; ++iter, ++index)
    {
        // use unsigned cast to avoid sign negative format use at long->
        // ActiveStates (int) conversion
        uint8 type = (uint8)atol((*iter).c_str());
        ++iter;
        uint32 action = atol((*iter).c_str());

        PetActionBar[index].SetActionAndType(action, ActiveStates(type));

        // check correctness
        if (PetActionBar[index].IsActionBarForSpell() &&
            !sSpellStore.LookupEntry(PetActionBar[index].GetAction()))
            SetActionBar(index, 0, ACT_DISABLED);
    }
}

void CharmInfo::BuildActionBar(WorldPacket* data)
{
    for (auto& elem : PetActionBar)
        *data << uint32(elem.packedData);
}

void CharmInfo::SetSpellAutocast(uint32 spell_id, bool state)
{
    for (auto& elem : PetActionBar)
    {
        if (spell_id == elem.GetAction() && elem.IsActionBarForSpell() &&
            elem.GetType() != ACT_PASSIVE)
        {
            elem.SetType(state ? ACT_ENABLED : ACT_DISABLED);
            break;
        }
    }
}

void CharmInfo::SaveStayPosition(float Posx, float Posy, float Posz)
{
    m_stayX = Posx;
    m_stayY = Posy;
    m_stayZ = Posz;
}

bool Unit::isFrozen() const
{
    return HasAuraState(AURA_STATE_FROZEN);
}

struct ProcTriggeredData
{
    ProcTriggeredData(SpellProcEventEntry const* _spellProcEvent,
        AuraHolder* _triggeredByHolder)
      : spellProcEvent(_spellProcEvent), triggeredByHolder(_triggeredByHolder)
    {
    }
    SpellProcEventEntry const* spellProcEvent;
    AuraHolder* triggeredByHolder;
};

typedef std::list<ProcTriggeredData> ProcTriggeredList;
typedef std::list<uint32> RemoveSpellList;

uint32 createProcExtendMask(
    SpellNonMeleeDamage* damageInfo, SpellMissInfo missCondition)
{
    uint32 procEx = PROC_EX_NONE;
    // Check victim state
    if (missCondition != SPELL_MISS_NONE)
        switch (missCondition)
        {
        case SPELL_MISS_MISS:
            procEx |= PROC_EX_MISS;
            break;
        case SPELL_MISS_RESIST:
            procEx |= PROC_EX_RESIST;
            break;
        case SPELL_MISS_DODGE:
            procEx |= PROC_EX_DODGE;
            break;
        case SPELL_MISS_PARRY:
            procEx |= PROC_EX_PARRY;
            break;
        case SPELL_MISS_BLOCK:
            procEx |= PROC_EX_BLOCK;
            break;
        case SPELL_MISS_EVADE:
            procEx |= PROC_EX_EVADE;
            break;
        case SPELL_MISS_IMMUNE:
            procEx |= PROC_EX_IMMUNE;
            break;
        case SPELL_MISS_IMMUNE2:
            procEx |= PROC_EX_IMMUNE;
            break;
        case SPELL_MISS_DEFLECT:
            procEx |= PROC_EX_DEFLECT;
            break;
        case SPELL_MISS_ABSORB:
            procEx |= PROC_EX_ABSORB;
            break;
        case SPELL_MISS_REFLECT:
            procEx |= PROC_EX_REFLECT;
            break;
        default:
            break;
        }
    else
    {
        // On block
        if (damageInfo->blocked)
            procEx |= PROC_EX_BLOCK;
        // On absorb
        if (damageInfo->absorb)
            procEx |= PROC_EX_ABSORB;
        // On crit
        if (damageInfo->HitInfo & SPELL_HIT_TYPE_CRIT)
            procEx |= PROC_EX_CRITICAL_HIT;
        else
            procEx |= PROC_EX_NORMAL_HIT;
    }
    return procEx;
}

void Unit::ProcDamageAndSpellFor(bool isVictim, Unit* pTarget, uint32 procFlag,
    uint32 procExtra, WeaponAttackType attType, SpellEntry const* procSpell,
    proc_amount amount, ExtraAttackType extraAttackType, uint32 extraAttackId)
{
    // For melee/ranged based attack need update skills and set some Aura
    // states
    if (procFlag & MELEE_BASED_TRIGGER_MASK)
    {
        // On weapon based attack: update skill (for victim and attacker)
        // Works on all types; dodge, parry, immune, evade, etc
        if (GetTypeId() == TYPEID_PLAYER &&
            pTarget->GetTypeId() != TYPEID_PLAYER &&
            !static_cast<Creature*>(pTarget)->IsPlayerPet())
        {
            static_cast<Player*>(this)->UpdateCombatSkills(
                pTarget, attType, isVictim);
        }

        // If exist crit/parry/dodge/block need update aura state (for
        // victim
        // and attacker)
        if (procExtra & (PROC_EX_CRITICAL_HIT | PROC_EX_PARRY | PROC_EX_DODGE |
                            PROC_EX_BLOCK))
        {
            // for victim
            if (isVictim)
            {
                // if victim and dodge attack
                if (procExtra & PROC_EX_DODGE)
                {
                    // Update AURA_STATE on dodge
                    if (getClass() != CLASS_ROGUE ||
                        GetTypeId() != TYPEID_PLAYER) // skip Rogue Riposte
                    {
                        ModifyAuraState(AURA_STATE_DEFENSE, true);
                        StartReactiveTimer(REACTIVE_DEFENSE);
                    }
                }
                // if victim and parry attack
                if (procExtra & PROC_EX_PARRY)
                {
                    // For Hunters only Counterattack (skip Mongoose bite)
                    if (getClass() == CLASS_HUNTER)
                    {
                        ModifyAuraState(AURA_STATE_HUNTER_PARRY, true);
                        StartReactiveTimer(REACTIVE_HUNTER_PARRY);
                    }
                    else
                    {
                        ModifyAuraState(AURA_STATE_DEFENSE, true);
                        StartReactiveTimer(REACTIVE_DEFENSE);
                    }
                }
                // if and victim block attack
                if (procExtra & PROC_EX_BLOCK)
                {
                    ModifyAuraState(AURA_STATE_DEFENSE, true);
                    StartReactiveTimer(REACTIVE_DEFENSE);
                }
            }
            else // For attacker
            {
                // Overpower on victim dodge
                if (procExtra & PROC_EX_DODGE)
                {
                    overpower_target = pTarget->GetObjectGuid();
                    if (GetTypeId() == TYPEID_PLAYER &&
                        getClass() == CLASS_WARRIOR)
                    {
                        static_cast<Player*>(this)->AddComboPoints(pTarget, 1);
                        StartReactiveTimer(REACTIVE_OVERPOWER);
                    }
                    else if (GetTypeId() == TYPEID_UNIT)
                    {
                        StartReactiveTimer(REACTIVE_OVERPOWER);
                    }
                }
                // Enable AURA_STATE_CRIT on crit
                if (procExtra & PROC_EX_CRITICAL_HIT)
                {
                    ModifyAuraState(AURA_STATE_CRIT, true);
                    StartReactiveTimer(REACTIVE_CRIT);
                    if (getClass() == CLASS_HUNTER)
                    {
                        ModifyAuraState(AURA_STATE_HUNTER_CRIT_STRIKE, true);
                        StartReactiveTimer(REACTIVE_HUNTER_CRIT);
                    }
                }
            }
        }
    }

    ProcTriggeredList procTriggered;
    // Fill procTriggered list
    for (auto& elem : m_auraHolders)
    {
        // skip disabled auras
        if (elem.second->IsDisabled())
            continue;

        // Skip spells that should only proc on cast. They're handled in
        // Unit::ProcSpellsOnCast
        SpellEntry const* procSpellEntry = elem.second->GetSpellProto();
        if (procSpellEntry && IsSpellProcOnCast(procSpellEntry))
            continue;

        SpellProcEventEntry const* spellProcEvent = nullptr;
        if (!IsTriggeredAtSpellProcEvent(pTarget, elem.second, procSpell,
                procFlag, procExtra, attType, isVictim, spellProcEvent))
            continue;

        // check proc exceptions
        if (procSpell &&
            !sSpellMgr::Instance()->CheckSpellProcException(
                procSpell->Id, elem.second->GetId()))
            continue;

        // If the aura has a cast item that is a weapon, and it differs from
        // the
        // attack type, it cannot proc. (E.g., Windfury Weapon cannot proc
        // from
        // the off-hand if it is only enchanted on the main-hand.)
        Item* cast_item =
            GetTypeId() == TYPEID_PLAYER && elem.second->GetCastItemGuid() ?
                static_cast<Player*>(this)->GetItemByGuid(
                    elem.second->GetCastItemGuid()) :
                nullptr;
        if (cast_item && cast_item->GetProto()->Class == ITEM_CLASS_WEAPON)
        {
            if (cast_item->slot().main_hand() && attType != BASE_ATTACK)
                continue;
            if (cast_item->slot().off_hand() && attType != OFF_ATTACK)
                continue;
            if (cast_item->slot().ranged() && attType != RANGED_ATTACK)
                continue;
        }

        procTriggered.push_back(ProcTriggeredData(spellProcEvent, elem.second));
    }

    // Nothing found
    if (procTriggered.empty())
        return;

    // Handle effects proceed this time
    for (ProcTriggeredList::const_iterator itr = procTriggered.begin();
         itr != procTriggered.end(); ++itr)
    {
        // Some auras can be disabled in function called in this loop
        // (except
        // the first one, ofc)
        AuraHolder* triggeredByHolder = itr->triggeredByHolder;
        if (triggeredByHolder->IsDisabled())
            continue;

        SpellProcEventEntry const* spellProcEvent = itr->spellProcEvent;
        bool useCharges = triggeredByHolder->GetAuraCharges() > 0;
        bool procSuccess = true;
        bool anyAuraProc = false;

        // For players set spell cooldown if need (if cooldown > 0 it's safe
        // to
        // assume *this is a player)
        uint32 cooldown = 0;
        if (GetTypeId() == TYPEID_PLAYER && spellProcEvent &&
            spellProcEvent->cooldown)
            cooldown = spellProcEvent->cooldown;

        // Check so the proc event is not on cooldown
        if (cooldown > 0 &&
            static_cast<Player*>(this)->HasProcEventCooldown(
                triggeredByHolder->GetId()))
            continue;

        for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
        {
            Aura* triggeredByAura =
                triggeredByHolder->GetAura(SpellEffectIndex(i));
            if (!triggeredByAura)
                continue;

            Modifier* auraModifier = triggeredByAura->GetModifier();

            if (procSpell)
            {
                // Default value
                bool active = !amount.empty() || amount.forced();
                // Applying Auras without direct damage/healing, exclude
                // periodic ticks
                if (!active &&
                    !(procFlag & (PROC_FLAG_ON_DO_PERIODIC |
                                     PROC_FLAG_ON_TAKE_PERIODIC)) &&
                    procSpell->powerType == POWER_MANA)
                {
                    uint32 EventProcFlags = 0;
                    uint32 EventProcEx = 0;

                    if (spellProcEvent)
                    {
                        EventProcFlags = spellProcEvent->procFlags;
                        EventProcEx = spellProcEvent->procEx;
                    }
                    if (EventProcFlags == 0)
                    {
                        EventProcFlags =
                            triggeredByAura->GetSpellProto()->procFlags;
                    }

                    // exclude procs on applying Auras for periodic proc
                    // events,
                    // since they should proc on ticks not applying. Exclude
                    // events without procflag 14/16
                    if (!(EventProcFlags & (PROC_FLAG_ON_DO_PERIODIC |
                                               PROC_FLAG_ON_TAKE_PERIODIC)) &&
                        EventProcFlags &
                            (PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL |
                                PROC_FLAG_SUCCESSFUL_NEGATIVE_SPELL_HIT))
                    {
                        // 14 Successful cast positive spell && 16
                        // Successful
                        // damage from harmful spell cast -> can proc form
                        // all
                        // spells with mana costs.
                        if ((EventProcFlags &
                                PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL) &&
                            (EventProcFlags &
                                PROC_FLAG_SUCCESSFUL_NEGATIVE_SPELL_HIT))
                        {
                            active = procSpell->manaCost > 0 ||
                                     procSpell->ManaCostPercentage > 0 ||
                                     procSpell->manaCostPerlevel > 0;
                        }
                        // 14 Successful cast positive spell only on direct
                        // healing or HoTs
                        else if (EventProcFlags &
                                 PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL)
                        {
                            if (!(EventProcEx & PROC_EX_DIRECT_ONLY))
                            {
                                for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
                                    if (procSpell->EffectApplyAuraName[i] ==
                                        SPELL_AURA_PERIODIC_HEAL)
                                        active =
                                            procSpell->manaCost > 0 ||
                                            procSpell->ManaCostPercentage > 0 ||
                                            procSpell->manaCostPerlevel > 0;
                            }
                        }
                        // 16 Successful damage from harmful spell
                        // cast(PROC_FLAG_SUCCESSFUL_NEGATIVE_SPELL_HIT)
                        // only
                        // trigger for enemy targeted spells
                        else
                        {
                            if (!(EventProcEx & PROC_EX_DIRECT_ONLY) &&
                                isVictim == false && IsHostileTo(pTarget))
                            {
                                active = procSpell->manaCost > 0 ||
                                         procSpell->ManaCostPercentage > 0 ||
                                         procSpell->manaCostPerlevel > 0;
                            }
                        }
                    }
                }

                if (spellProcEvent)
                {
                    if (spellProcEvent->spellFamilyMask[i])
                    {
                        if (!procSpell->IsFitToFamilyMask(
                                spellProcEvent->spellFamilyMask[i]))
                            continue;
                        // Procs of Hots should only occur when healing is
                        // done
                        if (procExtra & PROC_EX_PERIODIC_POSITIVE &&
                            amount.healing() == 0)
                            continue;
                        // Hack: Don't trigger nightfall if it's not an
                        // active
                        // proc
                        if (!active && procSpell &&
                            (triggeredByAura->GetId() == 18094 ||
                                triggeredByAura->GetId() == 18095))
                            continue;
                    }
                    // don't check dbc FamilyFlags if schoolMask exists
                    else if (!triggeredByAura->CanProcFrom(procSpell,
                                 spellProcEvent->procEx, procExtra, active,
                                 !spellProcEvent->schoolMask))
                        continue;
                }
                else if (!triggeredByAura->CanProcFrom(
                             procSpell, PROC_EX_NONE, procExtra, active, true))
                    continue;
            }

            SpellAuraProcResult procResult =
                (*this.*AuraProcHandler[auraModifier->m_auraname])(pTarget,
                    amount, triggeredByAura, procSpell, procFlag, procExtra,
                    cooldown, extraAttackType, extraAttackId);

            switch (procResult)
            {
            case SPELL_AURA_PROC_CANT_TRIGGER:
                continue;
            case SPELL_AURA_PROC_FAILED:
                procSuccess = false;
                break;
            case SPELL_AURA_PROC_OK:
                // Add cooldown to proccing aura too, if they have a
                // registered
                // spell proc event cooldown
                // (mangos adds it to the proccd spell only by default)
                // Note:
                // Don't add it on NullProcs
                if (cooldown > 0 &&
                    AuraProcHandler[auraModifier->m_auraname] !=
                        &Unit::HandleNULLProc)
                    static_cast<Player*>(this)->AddProcEventCooldown(
                        triggeredByHolder->GetId(), cooldown * IN_MILLISECONDS);
                break;
            }
            // Cold Blood
            if (triggeredByAura->GetSpellProto()->Id == 14177)
            {
                if (procSpell)
                {
                    switch (procSpell->Id)
                    {
                    // TO DO: Does mutilate do this with other effects? If
                    // so
                    // need to reset each modifier rather than just cold
                    // blood
                    // Mutilate do not take charges on main hand attack as
                    // the
                    // OH attack must crit as well.
                    case 5374:
                    case 34414:
                    case 34416:
                    case 34419:
                    {
                        useCharges = false;
                        if (SpellModifier* coldBlood =
                                ((Player*)this)
                                    ->GetSpellMod(
                                        SPELLMOD_CRITICAL_CHANCE, 14177))
                            coldBlood->charges = 1;
                    }
                    break;
                    default:
                        break;
                    }
                }
            }

            anyAuraProc = true;
        }

        // Remove charge (aura can be removed by triggers)
        if (useCharges && procSuccess && anyAuraProc &&
            !triggeredByHolder->IsDisabled())
        {
            // If last charge dropped add spell to remove list
            if (triggeredByHolder->DropAuraCharge())
                RemoveAuraHolder(triggeredByHolder);
        }
    }
}

SpellSchoolMask Unit::GetMeleeDamageSchoolMask() const
{
    return SPELL_SCHOOL_MASK_NORMAL;
}

Player* Unit::GetSpellModOwner() const
{
    if (GetTypeId() == TYPEID_PLAYER)
        return (Player*)this;
    if (((Creature*)this)->IsPet() || ((Creature*)this)->IsTotem())
    {
        Unit* owner = GetOwner();
        if (owner && owner->GetTypeId() == TYPEID_PLAYER)
            return (Player*)owner;
    }
    return nullptr;
}

///----------Pet responses methods-----------------
void Unit::SendPetCastFail(uint32 spellid, SpellCastResult msg)
{
    if (msg == SPELL_CAST_OK)
        return;

    Unit* owner = GetCharmerOrOwner();
    if (!owner || owner->GetTypeId() != TYPEID_PLAYER)
        return;

    WorldPacket data(SMSG_PET_CAST_FAILED, 4 + 1);
    data << uint32(spellid);
    data << uint8(msg);
    static_cast<Player*>(owner)->GetSession()->send_packet(std::move(data));
}

void Unit::SendPetActionFeedback(uint8 msg)
{
    Unit* owner = GetOwner();
    if (!owner || owner->GetTypeId() != TYPEID_PLAYER)
        return;

    WorldPacket data(SMSG_PET_ACTION_FEEDBACK, 1);
    data << uint8(msg);
    static_cast<Player*>(owner)->GetSession()->send_packet(std::move(data));
}

void Unit::SendPetTalk(uint32 pettalk)
{
    Unit* owner = GetOwner();
    if (!owner || owner->GetTypeId() != TYPEID_PLAYER)
        return;

    WorldPacket data(SMSG_PET_ACTION_SOUND, 8 + 4);
    data << GetObjectGuid();
    data << uint32(pettalk);
    static_cast<Player*>(owner)->GetSession()->send_packet(std::move(data));
}

void Unit::SendPetAIReaction()
{
    Unit* owner = GetOwner();
    if (!owner || owner->GetTypeId() != TYPEID_PLAYER)
        return;

    WorldPacket data(SMSG_AI_REACTION, 8 + 4);
    data << GetObjectGuid();
    data << uint32(AI_REACTION_HOSTILE);
    static_cast<Player*>(owner)->GetSession()->send_packet(std::move(data));
}

///----------End of Pet responses methods----------

void Unit::StopMoving()
{
    clearUnitState(UNIT_STAT_MOVING);

    if (!IsInWorld() || movespline->Finalized())
        return;

    UpdateSplinePosition();
    movement::MoveSplineInit init(*this);
    init.Stop();
}

void Unit::CharmUnit(
    bool apply, bool removeByDelete, Unit* target, AuraHolder* aura)
{
    if (GetTypeId() != TYPEID_PLAYER)
        return;

    Player* caster = (Player*)this;
    Camera& camera = caster->GetCamera();

    if (apply)
    {
        target->addUnitState(UNIT_STAT_CONTROLLED);

        // Add threat to caster, to make sure caster stays in combat during
        // MC
        if (target->GetTypeId() == TYPEID_UNIT &&
            static_cast<Creature*>(target)->AI())
        {
            if (!target->getVictim())
                static_cast<Creature*>(target)->AI()->AttackStart(caster);
            else
                target->AddThreat(caster);
        }

        target->SaveThreatList();

        target->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PLAYER_CONTROLLED);
        target->SetCharmerGuid(caster->GetObjectGuid());
        target->setFaction(caster->getFaction());

        target->CallForAllControlledUnits(
            [caster](Unit* pet)
            {
                pet->setFaction(caster->getFaction());
                pet->CombatStop(true, true);
                pet->DeleteThreatList();
                pet->getHostileRefManager().deleteReferences();
            },
            CONTROLLED_PET | CONTROLLED_GUARDIANS | CONTROLLED_CHARM |
                CONTROLLED_TOTEMS);

        // target should became visible at SetView call(if not visible
        // before):
        // otherwise client\caster will ignore packets from the
        // target(SetClientControl for example)
        camera.SetView(target);
        caster->SetCharm(target);
        if (caster->GetTypeId() == TYPEID_PLAYER)
            caster->SetMovingUnit(target);

        // Stop attack swing and current casts
        target->InterruptNonMeleeSpells(false);
        if (!target->AttackStop() && target->GetTypeId() == TYPEID_PLAYER)
            static_cast<Player*>(target)->SendAttackSwingCancelAttack();

        // Remove target from any now friendly NPCs threat list
        std::vector<Unit*> ignorers;
        auto& mgr = target->getHostileRefManager();
        for (auto itr = mgr.begin(); itr != mgr.end(); ++itr)
        {
            auto unit = itr->getSource()->getOwner();
            if (unit->GetTypeId() == TYPEID_UNIT && !target->IsHostileTo(unit))
                ignorers.push_back(unit);
        }
        // < -100: remove target
        for (auto& u : ignorers)
            u->getThreatManager().modifyThreatPercent(target, -101);

        if (CharmInfo* charmInfo = target->InitCharmInfo(target))
        {
            charmInfo->InitPossessCreateSpells();
            charmInfo->SetReactState(REACT_PASSIVE);
            charmInfo->SetCommandState(COMMAND_STAY);
        }

        caster->PossessSpellInitialize();

        if (target->GetTypeId() == TYPEID_UNIT)
        {
            if (auto ai = static_cast<Creature*>(target)->AI())
                ai->OnCharmed(true);
        }

        // Set run mode pre-possess to allow toggling it back if it changes
        if (target->GetTypeId() == TYPEID_PLAYER)
        {
            Player* pl = (Player*)target;
            pl->PrePossessRunMode = pl->GetRunMode();
        }
        else
        {
            // Activate running for creatures
            static_cast<Creature*>(target)->SetWalk(false);

            // Cause hostile actions to all nearby units
            ScheduleAINotify(0);
        }

        target->movement_gens.push(
            new movement::ControlledMovementGenerator(caster, aura));
    }
    else
    {
        caster->SetCharm(nullptr);
        if (caster->GetTypeId() == TYPEID_PLAYER)
            caster->SetMovingUnit(caster);

        // there is a possibility that target became invisible for
        // client\caster
        // at ResetView call:
        // it must be called after movement control unapplying, not before!
        // the
        // reason is same as at aura applying
        camera.ResetView();

        caster->RemovePetActionBar();

        // on delete only do caster related effects
        if (removeByDelete)
            return;

        target->clearUnitState(UNIT_STAT_CONTROLLED);

        target->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PLAYER_CONTROLLED);
        target->SetCharmerGuid(ObjectGuid());

        if (target->GetTypeId() == TYPEID_PLAYER)
        {
            ((Player*)target)->setFactionForRace(target->getRace());
        }
        else if (target->GetTypeId() == TYPEID_UNIT)
        {
            CreatureInfo const* cinfo = ((Creature*)target)->GetCreatureInfo();
            target->setFaction(cinfo->faction_A);
        }

        // Cancel blacklisting of added caster references
        if (target->GetTypeId() == TYPEID_UNIT)
        {
            for (auto itr = target->getHostileRefManager().begin();
                 itr != target->getHostileRefManager().end(); ++itr)
            {
                Unit* attacker;
                if ((attacker = itr->getSource()->getOwner()) != nullptr &&
                    attacker->GetTypeId() == TYPEID_UNIT)
                {
                    attacker->getThreatManager().removeCharmer(caster);
                }
            }
        }

        // Stop attack swing and current casts
        target->InterruptNonMeleeSpells(false);
        if (!target->AttackStop() && target->GetTypeId() == TYPEID_PLAYER)
            static_cast<Player*>(target)->SendAttackSwingCancelAttack();

        // Remove target from any now friendly NPCs threat list
        std::vector<Unit*> ignorers;
        auto& mgr = target->getHostileRefManager();
        bool player = target->GetTypeId() == TYPEID_PLAYER;
        for (auto itr = mgr.begin(); itr != mgr.end(); ++itr)
        {
            auto unit = itr->getSource()->getOwner();
            if (unit->GetTypeId() == TYPEID_UNIT && !target->IsHostileTo(unit))
                ignorers.push_back(unit);
        }
        // < -100: remove target
        for (auto& u : ignorers)
            u->getThreatManager().modifyThreatPercent(target, -101);

        // Restore old threatlist
        target->DeleteThreatList();
        target->RestoreThreatList();

        target->CallForAllControlledUnits(
            [target](Unit* pet)
            {
                pet->setFaction(target->getFaction());
                pet->CombatStop(true, true);
                pet->DeleteThreatList();
                pet->getHostileRefManager().deleteReferences();
            },
            CONTROLLED_PET | CONTROLLED_GUARDIANS | CONTROLLED_CHARM |
                CONTROLLED_TOTEMS);

        if (target->GetTypeId() == TYPEID_UNIT)
        {
            target->AttackedBy(caster);
            if (auto ai = static_cast<Creature*>(target)->AI())
                ai->OnCharmed(false);
        }

        // Send previous run/walk mode to client if it changed during
        // possession
        if (target->GetTypeId() == TYPEID_PLAYER)
        {
            Player* pl = (Player*)target;
            if (pl->PrePossessRunMode != pl->GetRunMode())
                pl->SetRunMode(pl->PrePossessRunMode, true);
        }

        target->DeleteCharmInfo();

        target->movement_gens.remove_if([aura](const movement::Generator* gen)
            {
                if (auto controlled = dynamic_cast<
                        const movement::ControlledMovementGenerator*>(gen))
                    return controlled->holder() == aura;
                return false;
            });
    }
}

void Unit::finished_path(std::vector<G3D::Vector3> path, uint32 id)
{
    if (auto top = movement_gens.top())
        top->finished_path(std::move(path), id);
}

void Unit::spell_pathgen_callback(std::vector<G3D::Vector3> path, uint32 id)
{
    for (int i = 0; i < CURRENT_MAX_SPELL; ++i)
        if (Spell* spell = GetCurrentSpell(CurrentSpellTypes(i)))
            if (spell->waiting_for_path == id)
            {
                spell->waiting_for_path = 0;
                spell->pregenerated_path = std::move(path);
                spell->path_gen_finished = true;
                return;
            }
}

bool Unit::can_pathing_cheat() const
{
    return GetTypeId() == TYPEID_UNIT &&
           static_cast<const Creature*>(this)->IsPet() &&
           hasUnitState(UNIT_STAT_FOLLOW) && GetOwnerGuid().IsPlayer() &&
           GetOwner() &&
           static_cast<const Pet*>(this)->IsPermanentPetFor(
               static_cast<Player*>(GetOwner())) &&
           !GetMap()->IsBattleArena();
}

void Unit::Root(bool enable)
{
    if (GetTypeId() == TYPEID_PLAYER)
        m_movementInfo.SetMovementFlags(MOVEFLAG_NONE);

    if (enable)
        m_movementInfo.AddMovementFlag(MOVEFLAG_ROOT);
    else
        m_movementInfo.RemoveMovementFlag(MOVEFLAG_ROOT);

    WorldPacket data;
    if (GetTypeId() == TYPEID_PLAYER)
        data.initialize(enable ? SMSG_FORCE_MOVE_ROOT : SMSG_FORCE_MOVE_UNROOT);
    else
        data.initialize(
            enable ? SMSG_SPLINE_MOVE_ROOT : SMSG_SPLINE_MOVE_UNROOT);

    data << GetPackGUID();
    if (GetTypeId() == TYPEID_PLAYER)
        data << uint32(0);

    // Send force root/unroot to controlling player
    if (auto controller = GetCharmerOrOwnerPlayerOrPlayerItself())
        controller->GetSession()->send_packet(std::move(data));
    // Send force root/unroot for NPCs to everyone
    else
        SendMessageToSet(&data, false);
}

void Unit::SetFeignDeath(bool apply, ObjectGuid casterGuid, uint32 /*spellID*/)
{
    if (apply)
    {
        if (GetTypeId() != TYPEID_PLAYER)
            StopMoving();
        else
            ((Player*)this)->m_movementInfo.SetMovementFlags(MOVEFLAG_NONE);

        AttackStop();

        // blizz like 2.0.x
        SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_UNK_29);
        // blizz like 2.0.x
        SetFlag(UNIT_FIELD_FLAGS_2, UNIT_FLAG2_FEIGN_DEATH);
        // blizz like 2.0.x
        SetFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_DEAD);

        addUnitState(UNIT_STAT_DIED);
        remove_auras_if([](AuraHolder* h)
            {
                return h->GetSpellProto()->AuraInterruptFlags &
                       AURA_INTERRUPT_FLAG_IMMUNE_OR_LOST_SELECTION;
            });

        // prevent interrupt message
        if (casterGuid == GetObjectGuid())
            FinishSpell(CURRENT_GENERIC_SPELL, false);
        InterruptNonMeleeSpells(true);
    }
    else
    {
        // blizz like 2.0.x
        RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_UNK_29);
        // blizz like 2.0.x
        RemoveFlag(UNIT_FIELD_FLAGS_2, UNIT_FLAG2_FEIGN_DEATH);
        // blizz like 2.0.x
        RemoveFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_DEAD);

        clearUnitState(UNIT_STAT_DIED);

        if (GetTypeId() != TYPEID_PLAYER && isAlive())
            movement_gens.remove_all(movement::gen::stopped);
    }
}

bool Unit::IsSitState() const
{
    uint8 s = getStandState();
    return s == UNIT_STAND_STATE_SIT_CHAIR ||
           s == UNIT_STAND_STATE_SIT_LOW_CHAIR ||
           s == UNIT_STAND_STATE_SIT_MEDIUM_CHAIR ||
           s == UNIT_STAND_STATE_SIT_HIGH_CHAIR || s == UNIT_STAND_STATE_SIT;
}

bool Unit::IsStandState() const
{
    uint8 s = getStandState();
    return !IsSitState() && s != UNIT_STAND_STATE_SLEEP &&
           s != UNIT_STAND_STATE_KNEEL;
}

void Unit::SetStandState(uint8 state)
{
    SetByteValue(UNIT_FIELD_BYTES_1, 0, state);

    if (IsStandState())
        remove_auras_if([](AuraHolder* h)
            {
                return h->GetSpellProto()->AuraInterruptFlags &
                       AURA_INTERRUPT_FLAG_NOT_SEATED;
            });

    if (GetTypeId() == TYPEID_PLAYER)
    {
        WorldPacket data(SMSG_STANDSTATE_UPDATE, 1);
        data << (uint8)state;
        static_cast<Player*>(this)->GetSession()->send_packet(std::move(data));
    }
}

bool Unit::IsAffectedByThreatIgnoringCC() const
{
    // UnitState checks are cheaper, rather than using HasAuraType like the
    // other is CCd functions
    return HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_CONFUSED) ||
           HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_FLEEING) ||
           hasUnitState(UNIT_STAT_FLEEING) ||
           hasUnitState(UNIT_STAT_CONFUSED) ||
           hasUnitState(UNIT_STAT_CONTROLLED) ||
           hasUnitState(UNIT_STAT_STUNNED);
}

void Unit::SetDisplayId(uint32 modelId)
{
    SetUInt32Value(UNIT_FIELD_DISPLAYID, modelId);

    UpdateModelData();

    if (GetTypeId() == TYPEID_UNIT && ((Creature*)this)->IsPet())
    {
        Pet* pet = ((Pet*)this);
        if (!pet->isControlled())
            return;
        Unit* owner = GetOwner();
        if (owner && (owner->GetTypeId() == TYPEID_PLAYER) &&
            ((Player*)owner)->GetGroup())
            ((Player*)owner)
                ->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_PET_MODEL_ID);
    }
}

void Unit::UpdateModelData()
{
    // Testing seems to indicate that players don't scale with size
    // modifications, whereas NPCs do
    if (GetTypeId() == TYPEID_PLAYER)
    {
        SetFloatValue(UNIT_FIELD_BOUNDINGRADIUS, DEFAULT_BOUNDING_RADIUS);
        SetFloatValue(UNIT_FIELD_COMBATREACH, 1.5f);
        return;
    }

    // Pets that are part of a player class seems to keep a standarized
    // bounding/combat reach
    if (static_cast<Creature*>(this)->IsPet())
    {
        auto pet = static_cast<Pet*>(this);
        if (pet->GetOwnerGuid().IsPlayer() &&
            (pet->getPetType() == HUNTER_PET ||
                pet->getPetType() == SUMMON_PET))
        {
            SetFloatValue(UNIT_FIELD_BOUNDINGRADIUS, GetObjectScale() * 2.0f);
            SetFloatValue(UNIT_FIELD_COMBATREACH, GetObjectScale() * 3.0f);
            return;
        }
    }

    if (CreatureModelInfo const* modelInfo =
            sObjectMgr::Instance()->GetCreatureModelInfo(GetDisplayId()))
    {
        // we expect values in database to be relative to scale = 1.0
        SetFloatValue(UNIT_FIELD_BOUNDINGRADIUS,
            GetObjectScale() * modelInfo->bounding_radius);
        SetFloatValue(
            UNIT_FIELD_COMBATREACH, GetObjectScale() * modelInfo->combat_reach);
    }
}

void Unit::ClearComboPointHolders()
{
    while (!m_ComboPointHolders.empty())
    {
        uint32 lowguid = *m_ComboPointHolders.begin();

        Player* plr = sObjectMgr::Instance()->GetPlayer(
            ObjectGuid(HIGHGUID_PLAYER, lowguid));
        if (plr &&
            plr->GetComboTargetGuid() == GetObjectGuid()) // recheck for safe
            plr->ClearComboPoints(); // remove also guid from
                                     // m_ComboPointHolders;
        else
            m_ComboPointHolders.erase(lowguid); // or remove manually
    }
}

void Unit::ClearAllReactives()
{
    for (auto& elem : m_reactiveTimer)
        elem = 0;

    if (HasAuraState(AURA_STATE_DEFENSE))
        ModifyAuraState(AURA_STATE_DEFENSE, false);
    if (getClass() == CLASS_HUNTER && HasAuraState(AURA_STATE_HUNTER_PARRY))
        ModifyAuraState(AURA_STATE_HUNTER_PARRY, false);
    if (HasAuraState(AURA_STATE_CRIT))
        ModifyAuraState(AURA_STATE_CRIT, false);
    if (getClass() == CLASS_HUNTER &&
        HasAuraState(AURA_STATE_HUNTER_CRIT_STRIKE))
        ModifyAuraState(AURA_STATE_HUNTER_CRIT_STRIKE, false);

    if (getClass() == CLASS_WARRIOR && GetTypeId() == TYPEID_PLAYER)
        ((Player*)this)->ClearComboPoints();
}

void Unit::UpdateReactives(uint32 p_time)
{
    for (int i = 0; i < MAX_REACTIVE; ++i)
    {
        ReactiveType reactive = ReactiveType(i);

        if (!m_reactiveTimer[reactive])
            continue;

        if (m_reactiveTimer[reactive] <= p_time)
        {
            m_reactiveTimer[reactive] = 0;

            switch (reactive)
            {
            case REACTIVE_DEFENSE:
                if (HasAuraState(AURA_STATE_DEFENSE))
                    ModifyAuraState(AURA_STATE_DEFENSE, false);
                break;
            case REACTIVE_HUNTER_PARRY:
                if (getClass() == CLASS_HUNTER &&
                    HasAuraState(AURA_STATE_HUNTER_PARRY))
                    ModifyAuraState(AURA_STATE_HUNTER_PARRY, false);
                break;
            case REACTIVE_CRIT:
                if (HasAuraState(AURA_STATE_CRIT))
                    ModifyAuraState(AURA_STATE_CRIT, false);
                break;
            case REACTIVE_HUNTER_CRIT:
                if (getClass() == CLASS_HUNTER &&
                    HasAuraState(AURA_STATE_HUNTER_CRIT_STRIKE))
                    ModifyAuraState(AURA_STATE_HUNTER_CRIT_STRIKE, false);
                break;
            case REACTIVE_OVERPOWER:
                if (getClass() == CLASS_WARRIOR && GetTypeId() == TYPEID_PLAYER)
                    ((Player*)this)->ClearComboPoints();
                overpower_target.Clear();
                break;
            default:
                break;
            }
        }
        else
        {
            m_reactiveTimer[reactive] -= p_time;
        }
    }
}

Unit* Unit::SelectRandomUnfriendlyTarget(
    Unit* except /*= NULL*/, float radius /*= ATTACK_DISTANCE*/) const
{
    auto type = IsControlledByPlayer() ?
                    maps::checks::friendly_status::not_friendly :
                    maps::checks::friendly_status::hostile;

    auto targets = maps::visitors::yield_set<Unit, Player, Creature, Pet,
        SpecialVisCreature, TemporarySummon>{}(
        this, radius, maps::checks::friendly_status{this, type});

    // Remove unappropriate targets
    for (auto itr = targets.begin(); itr != targets.end();)
    {
        if (except == *itr || !IsWithinWmoLOSInMap(*itr))
            itr = targets.erase(itr);
        else
            ++itr;
    }

    // No appropriate targets
    if (targets.empty())
        return nullptr;

    // Select randomly
    return *(targets.begin() + urand(0, targets.size() - 1));
}

Unit* Unit::SelectRandomFriendlyTarget(
    Unit* except /*= NULL*/, float radius /*= ATTACK_DISTANCE*/) const
{
    auto targets = maps::visitors::yield_set<Unit, Player, Creature, Pet,
        SpecialVisCreature, TemporarySummon>{}(
        this, radius, maps::checks::friendly_status{
                          this, maps::checks::friendly_status::friendly});

    // Remove unappropriate targets
    for (auto itr = targets.begin(); itr != targets.end();)
    {
        if (except == *itr || !IsWithinWmoLOSInMap(*itr))
            itr = targets.erase(itr);
        else
            ++itr;
    }

    // No appropriate targets
    if (targets.empty())
        return nullptr;

    // Select randomly
    return *(targets.begin() + urand(0, targets.size() - 1));
}

Unit* Unit::SelectNearestPetTarget(Unit* except, float radius) const
{
    Unit* target = nullptr;

    // We check hostility against the outermost owner
    const Unit* hostility_unit = this;
    if (Unit* owner_inner = GetOwner())
    {
        hostility_unit = owner_inner;
        // Has an outer owner if pet is summoned by a totem
        if (Unit* owner_outer = owner_inner->GetOwner())
            hostility_unit = owner_outer;
    }

    auto targets = maps::visitors::yield_set<Unit, Player, Creature, Pet,
        SpecialVisCreature, TemporarySummon, Totem>{}(
        this, radius, maps::checks::friendly_status{hostility_unit,
                          maps::checks::friendly_status::hostile});

    // Select the closest appropriate target
    float lastDistance = 10000.0f;
    for (auto& t : targets)
    {
        float dist = GetDistance(t);
        if (dist >= lastDistance)
            continue;

        if (except == t || !IsWithinWmoLOSInMap(t) ||
            (t)->GetCreatureType() == CREATURE_TYPE_CRITTER ||
            (t)->HasBreakOnDamageCCAura() || !(t)->can_be_seen_by(this, this) ||
            !(t)->isTargetableForAttack() ||
            (t)->HasAuraType(SPELL_AURA_FEIGN_DEATH) ||
            (t)->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_PLAYER_ATTACKABLE))
            continue;

        lastDistance = dist;
        target = t;
    }

    return target;
}

void Unit::SelectUnfriendlyInRange(
    std::vector<Unit*>& targets, float radius, bool ignoreLos) const
{
    targets = maps::visitors::yield_set<Unit, Player, Creature, Pet,
        SpecialVisCreature, TemporarySummon>{}(
        this, radius, maps::checks::friendly_status{
                          this, maps::checks::friendly_status::hostile});

    if (ignoreLos)
        return;

    // We need to remove all targets not in line of sight
    for (auto itr = targets.begin(); itr != targets.end();)
    {
        if (!IsWithinWmoLOSInMap(*itr))
            itr = targets.erase(itr);
        else
            ++itr;
    }
}

void Unit::ApplyAttackTimePercentMod(
    WeaponAttackType att, float val, bool apply)
{
    float previousAttackSpeed = m_modAttackSpeedPct[att];
    if (val > 0)
    {
        ApplyPercentModFloatVar(m_modAttackSpeedPct[att], val, !apply);
        ApplyPercentModFloatValue(UNIT_FIELD_BASEATTACKTIME + att, val, !apply);
    }
    else
    {
        ApplyPercentModFloatVar(m_modAttackSpeedPct[att], -val, apply);
        ApplyPercentModFloatValue(UNIT_FIELD_BASEATTACKTIME + att, -val, apply);
    }

    // We need to modify the attack timer to have the current swing
    // affected:
    if (previousAttackSpeed > 0.0f && m_attackTimer[att])
    {
        float deltaAttackSpeed = m_modAttackSpeedPct[att] / previousAttackSpeed;
        m_attackTimer[att] = uint32(m_attackTimer[att] * deltaAttackSpeed);
    }
}

void Unit::ApplyCastTimePercentMod(float val, bool apply)
{
    if (val > 0)
        ApplyPercentModFloatValue(UNIT_MOD_CAST_SPEED, val, !apply);
    else
        ApplyPercentModFloatValue(UNIT_MOD_CAST_SPEED, -val, apply);
}

void Unit::UpdateAuraForGroup(uint8 slot)
{
    if (GetTypeId() == TYPEID_PLAYER)
    {
        Player* player = (Player*)this;
        if (player->GetGroup())
        {
            player->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_AURAS);
            player->SetAuraUpdateMask(slot);
        }
    }
    else if (GetTypeId() == TYPEID_UNIT && ((Creature*)this)->IsPet())
    {
        Pet* pet = ((Pet*)this);
        if (pet->isControlled())
        {
            Unit* owner = GetOwner();
            if (owner && (owner->GetTypeId() == TYPEID_PLAYER) &&
                ((Player*)owner)->GetGroup())
            {
                ((Player*)owner)
                    ->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_PET_AURAS);
                pet->SetAuraUpdateMask(slot);
            }
        }
    }
}

float Unit::GetAPMultiplier(WeaponAttackType attType, bool normalized)
{
    if (!normalized || GetTypeId() != TYPEID_PLAYER)
        return float(GetAttackTime(attType)) / 1000.0f;

    Item* Weapon = ((Player*)this)->GetWeaponForAttack(attType, true, false);
    if (!Weapon)
        return 2.4f; // fist attack

    switch (Weapon->GetProto()->InventoryType)
    {
    case INVTYPE_2HWEAPON:
        return 3.3f;
    case INVTYPE_RANGED:
    case INVTYPE_RANGEDRIGHT:
    case INVTYPE_THROWN:
        return 2.8f;
    case INVTYPE_WEAPON:
    case INVTYPE_WEAPONMAINHAND:
    case INVTYPE_WEAPONOFFHAND:
    default:
        return Weapon->GetProto()->SubClass == ITEM_SUBCLASS_WEAPON_DAGGER ?
                   1.7f :
                   2.4f;
    }
}

void Unit::SetContestedPvP(Player* attackedPlayer)
{
    Player* player = GetCharmerOrOwnerPlayerOrPlayerItself();

    if (!player ||
        (attackedPlayer &&
            (attackedPlayer == player || player->IsInDuelWith(attackedPlayer))))
        return;

    player->SetContestedPvPTimer(30000);

    if (!player->hasUnitState(UNIT_STAT_ATTACK_PLAYER))
    {
        player->addUnitState(UNIT_STAT_ATTACK_PLAYER);
        player->SetFlag(PLAYER_FLAGS, PLAYER_FLAGS_CONTESTED_PVP);
        // call MoveInLineOfSight for nearby contested guards
        UpdateVisibilityAndView();
    }

    if (!hasUnitState(UNIT_STAT_ATTACK_PLAYER))
    {
        addUnitState(UNIT_STAT_ATTACK_PLAYER);
        // call MoveInLineOfSight for nearby contested guards
        UpdateVisibilityAndView();
    }
}

void Unit::AddPetAura(PetAura const* petSpell)
{
    m_petAuras.insert(petSpell);
    if (Pet* pet = GetPet())
        pet->CastPetAura(petSpell);
}

void Unit::RemovePetAura(PetAura const* petSpell)
{
    m_petAuras.erase(petSpell);
    if (Pet* pet = GetPet())
        pet->remove_auras(petSpell->GetAura(pet->GetEntry()));
}

void Unit::NearTeleportTo(
    float x, float y, float z, float orientation, bool casting /*= false*/)
{
    DisableSpline();

    if (GetTypeId() == TYPEID_PLAYER)
        ((Player*)this)
            ->TeleportTo(GetMapId(), x, y, z, orientation,
                TELE_TO_NOT_LEAVE_TRANSPORT | TELE_TO_NOT_LEAVE_COMBAT |
                    TELE_TO_NOT_UNSUMMON_PET | (casting ? TELE_TO_SPELL : 0));
    else
    {
        Creature* c = (Creature*)this;

        if (!c->movement_gens.empty())
            c->movement_gens.top()->stop();

        GetMap()->relocate((Creature*)this, x, y, z, orientation);

        SendHeartBeat();

        if (!c->movement_gens.empty())
            c->movement_gens.top()->start();
    }
}

struct SetPvPHelper
{
    explicit SetPvPHelper(bool _state) : state(_state) {}
    void operator()(Unit* unit) const { unit->SetPvP(state); }
    bool state;
};

void Unit::SetPvP(bool state)
{
    if (state)
        SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PVP);
    else
        RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PVP);

    CallForAllControlledUnits(
        SetPvPHelper(state), CONTROLLED_PET | CONTROLLED_TOTEMS |
                                 CONTROLLED_GUARDIANS | CONTROLLED_CHARM);
}

struct StopAttackFactionHelper
{
    explicit StopAttackFactionHelper(uint32 _faction_id)
      : faction_id(_faction_id)
    {
    }
    void operator()(Unit* unit) const { unit->StopAttackFaction(faction_id); }
    uint32 faction_id;
};

void Unit::StopAttackFaction(uint32 faction_id)
{
    if (Unit* victim = getVictim())
    {
        if (victim->getFactionTemplateEntry()->faction == faction_id)
        {
            AttackStop();
            if (IsNonMeleeSpellCasted(false))
                InterruptNonMeleeSpells(false);

            // melee and ranged forced attack cancel
            if (GetTypeId() == TYPEID_PLAYER)
                ((Player*)this)->SendAttackSwingCancelAttack();
        }
    }

    AttackerSet const& attackers = getAttackers();
    for (auto itr = attackers.begin(); itr != attackers.end();)
    {
        if ((*itr)->getFactionTemplateEntry()->faction == faction_id)
        {
            (*itr)->AttackStop();
            itr = attackers.begin();
        }
        else
            ++itr;
    }

    getHostileRefManager().deleteReferencesForFaction(faction_id);

    CallForAllControlledUnits(StopAttackFactionHelper(faction_id),
        CONTROLLED_PET | CONTROLLED_GUARDIANS | CONTROLLED_CHARM);
}

bool Unit::CheckAndIncreaseCastCounter()
{
    uint32 maxCasts =
        sWorld::Instance()->getConfig(CONFIG_UINT32_MAX_SPELL_CASTS_IN_CHAIN);

    if (maxCasts && m_castCounter >= maxCasts)
        return false;

    ++m_castCounter;
    return true;
}

bool Unit::IsAllowedDamageInArea(Unit* pVictim) const
{
    // can damage self anywhere
    if (pVictim == this)
        return true;

    // can damage own pet anywhere
    if (pVictim->GetOwnerGuid() == GetObjectGuid())
        return true;

    // non player controlled unit can damage anywhere
    Player const* pOwner = GetCharmerOrOwnerPlayerOrPlayerItself();
    if (!pOwner)
        return true;

    // can damage non player controlled victim anywhere
    Player const* vOwner = pVictim->GetCharmerOrOwnerPlayerOrPlayerItself();
    if (!vOwner)
        return true;

    // can damage opponent in duel
    if (pOwner->IsInDuelWith(vOwner))
        return true;

    // can't damage player controlled unit by player controlled unit in
    // sanctuary
    AreaTableEntry const* area = GetAreaEntryByAreaID(pVictim->GetAreaId());
    if (area && area->flags & AREA_FLAG_SANCTUARY)
        return false;

    return true;
}

namespace
{
template <typename T>
void relocation_worker(Creature* c, T* target)
{
    if (!c->isAlive() || c->hasUnitState(UNIT_STAT_LOST_CONTROL))
        return;

    if (c->AI() && c->AI()->IsVisible(target) && !c->IsInEvadeMode() &&
        c->same_floor(target))
    {
        if (c->aggro_delay() && c->aggro_delay() > WorldTimer::getMSTime())
            return;
        else if (c->aggro_delay())
            c->aggro_delay(0);
        c->AI()->MoveInLineOfSight(target);
    }
}
}

class RelocationNotifyEvent : public BasicEvent
{
public:
    RelocationNotifyEvent(Unit& owner) : BasicEvent(), m_owner(&owner)
    {
        m_owner->_SetAINotifyScheduled(true);
    }

    bool Execute(uint64 /*e_time*/, uint32 /*p_time*/) override
    {
        m_owner->_SetAINotifyScheduled(false);

        float radius = MAX_CREATURE_ATTACK_RADIUS;

        if (m_owner->GetTypeId() == TYPEID_PLAYER)
        {
            if (!m_owner->isAlive() || m_owner->IsTaxiFlying())
                return true;

            maps::visitors::simple<Creature, SpecialVisCreature,
                TemporarySummon, Pet>{}(m_owner, radius, [this](Creature* c)
                {
                    relocation_worker(c, m_owner);
                });
        }
        else // if(m_owner.GetTypeId() == TYPEID_UNIT)
        {
            auto cowner = static_cast<Creature*>(m_owner);
            if (!m_owner->isAlive() || cowner->IsInEvadeMode())
                return true;

            bool cowner_can_aggro =
                !m_owner->hasUnitState(UNIT_STAT_LOST_CONTROL) &&
                cowner->AI() &&
                !(cowner->aggro_delay() &&
                    cowner->aggro_delay() > WorldTimer::getMSTime());

            if (unlikely(cowner_can_aggro && cowner->aggro_delay()))
                cowner->aggro_delay(0);

            // Visit creatures
            maps::visitors::simple<Creature, SpecialVisCreature,
                TemporarySummon, Pet>{}(m_owner, radius,
                [cowner, cowner_can_aggro](auto&& elem)
                {
                    if (cowner_can_aggro && cowner->AI()->IsVisible(elem) &&
                        cowner->same_floor(elem))
                        cowner->AI()->MoveInLineOfSight(elem);
                    relocation_worker(elem, cowner);
                });

            // Visit players
            maps::visitors::simple<Player>{}(m_owner, radius,
                [cowner, cowner_can_aggro](auto&& elem)
                {
                    if (cowner_can_aggro && cowner->AI()->IsVisible(elem) &&
                        cowner->same_floor(elem))
                        cowner->AI()->MoveInLineOfSight(elem);
                });
        }
        return true;
    }

    void Abort(uint64) override { m_owner->_SetAINotifyScheduled(false); }

private:
    Unit* m_owner;
};

void Unit::ScheduleAINotify(uint32 delay)
{
    // Don't trigger AI responses when game masters are moving about
    if (GetTypeId() == TYPEID_PLAYER &&
        static_cast<Player*>(this)->isGameMaster())
    {
        _SetAINotifyScheduled(false);
        return;
    }
    if (!IsAINotifyScheduled())
        m_Events.AddEvent(
            new RelocationNotifyEvent(*this), m_Events.CalculateTime(delay));
}

void Unit::OnRelocated()
{
    // switch to use G3D::Vector3 is good idea, maybe
    float dx = m_last_notified_position.x - GetX();
    float dy = m_last_notified_position.y - GetY();
    float dz = m_last_notified_position.z - GetZ();
    float distsq = dx * dx + dy * dy + dz * dz;
    if (distsq > World::GetRelocationLowerLimitSq())
    {
        m_last_notified_position.x = GetX();
        m_last_notified_position.y = GetY();
        m_last_notified_position.z = GetZ();

        GetViewPoint().Call_UpdateVisibilityForOwner();
        UpdateObjectVisibility();
    }
    ScheduleAINotify(World::GetRelocationAINotifyDelay());
}

void Unit::UpdateSplineMovement(uint32 t_diff)
{
    if (movespline->Finalized())
        return;

    movespline->updateState(t_diff);
    bool arrived = movespline->Finalized();

    if (arrived)
        DisableSpline();

    m_movesplineTimer.Update(t_diff);
    if (m_movesplineTimer.Passed() || arrived)
        UpdateSplinePosition();
}

void Unit::UpdateSplinePosition()
{
    enum
    {
        POSITION_UPDATE_DELAY = 400
    };

    m_movesplineTimer.Reset(POSITION_UPDATE_DELAY);
    movement::Location loc = movespline->ComputePosition();

    if (movespline->transport)
    {
        m_movementInfo.transport.pos.x = loc.x;
        m_movementInfo.transport.pos.y = loc.y;
        m_movementInfo.transport.pos.z = loc.z;
        m_movementInfo.transport.pos.o = loc.orientation;

        if (auto transport = GetTransport())
            transport->CalculatePassengerPosition(
                loc.x, loc.y, loc.z, &loc.orientation);
        else if (GetTypeId() == TYPEID_UNIT &&
                 static_cast<Creature*>(this)->IsPet() && GetOwner())
            GetOwner()->GetPosition(loc.x, loc.y,
                loc.z); // If pet has left transport, adopt owners x,y,z
    }

    if (hasUnitState(UNIT_STAT_CANNOT_ROTATE) &&
        !hasUnitState(UNIT_STAT_FLEEING)) // fear spline can still turn unit
        loc.orientation = GetO();

    // Don't relocate if we haven't moved
    float x, y, z;
    GetPosition(x, y, z);
    if (fabs(x - loc.x) < 0.001 && fabs(y - loc.y) < 0.001 &&
        fabs(z - loc.z) < 0.001 && fabs(GetO() - loc.orientation) < 0.001)
        return;

    if (GetTypeId() == TYPEID_PLAYER)
        static_cast<Player*>(this)->SetPosition(
            loc.x, loc.y, loc.z, loc.orientation);
    else
        GetMap()->relocate(
            static_cast<Creature*>(this), loc.x, loc.y, loc.z, loc.orientation);
}

void Unit::DisableSpline()
{
    m_movementInfo.RemoveMovementFlag(
        MOVEFLAG_SPLINE_ENABLED | MOVEFLAG_FORWARD);
    movespline->_Interrupt();
}

void Unit::update_stealth()
{
    std::vector<ObjectGuid> prev_detected_by(
        stealth_detected_by_.begin(), stealth_detected_by_.end());
    stealth_detected_by_.clear();

    if (!HasAuraType(SPELL_AURA_MOD_STEALTH))
        return;

    auto units = maps::visitors::yield_set<Unit, Player, Creature, Pet,
        SpecialVisCreature, TemporarySummon, Totem>{}(this,
        GetMap()->GetVisibilityDistance(), [this](auto&& elem)
        {
            return this != elem && elem->isAlive();
        });

    for (auto unit : units)
    {
        if (unit->can_stealth_detect(this))
        {
            stealth_detected_by_.push_back(unit->GetObjectGuid());

            if (unit->GetTypeId() == TYPEID_PLAYER &&
                !static_cast<Player*>(unit)->HaveAtClient(this) &&
                can_be_seen_by(unit, this)) // stealth check will pass because
                                            // we add to stealth_detected_by_
                                            // before
                static_cast<Player*>(unit)->AddToClient(this);
        }
        else
        {
            if (unit->GetTypeId() == TYPEID_PLAYER &&
                static_cast<Player*>(unit)->HaveAtClient(this))
                static_cast<Player*>(unit)->RemoveFromClient(this);
        }
    }

    // We need to check if someone that everyone that lost target of us has
    // us
    // properly removed from their client
    for (auto guid : prev_detected_by)
    {
        auto itr = std::find(
            stealth_detected_by_.begin(), stealth_detected_by_.end(), guid);
        if (itr != stealth_detected_by_.end())
            continue;

        if (Unit* u = GetMap()->GetUnit(guid))
        {
            if (!can_stealth_against(u))
            {
                stealth_detected_by_.push_back(guid);
                continue;
            }
            if (u->GetTypeId() == TYPEID_PLAYER &&
                static_cast<Player*>(u)->HaveAtClient(this))
                static_cast<Player*>(u)->RemoveFromClient(this);
        }
    }
}

bool Unit::can_stealth_against(const Unit* target) const
{
    const Player* p1 = GetTypeId() == TYPEID_PLAYER ?
                           static_cast<const Player*>(this) :
                           nullptr;
    const Player* p2 = target->GetTypeId() == TYPEID_PLAYER ?
                           static_cast<const Player*>(target) :
                           nullptr;

    if (p2 && p2->isGameMaster())
        return false;

    // Always detected by target if we're controlled/owned by him
    if (GetCharmerOrOwnerGuid() == target->GetObjectGuid())
        return false;

    // Boss level stealth: not detectable by revealing auras
    auto& stealth_auras = GetAurasByType(SPELL_AURA_MOD_STEALTH);
    for (auto& aura : stealth_auras)
        if (aura->GetModifier()->m_amount > 9000)
            return true;

    // Cannot stealth against someone with SPELL_AURA_DETECT_STEALTH
    if (target->HasAuraType(SPELL_AURA_DETECT_STEALTH))
        return false;

    // Always detected by target if we're in the same raid or group, but not
    // duelling
    if (p1 && p2 && p1->IsInSameRaidWith(p2) &&
        !(p1->duel && p1->duel->startTime > 0 && p1->duel->startTimer == 0 &&
            p1->duel->opponent == p2))
        return false;

    // Stealthing with hunter's mark on you, means you're detected by the hunter
    // and his/hers party
    auto& auras = GetAurasByType(SPELL_AURA_MOD_STALKED);
    for (auto aura : auras)
    {
        Unit* caster = aura->GetCaster();
        if (!caster)
            continue;
        if (caster == target ||
            (caster->GetTypeId() == TYPEID_PLAYER &&
                caster->IsInPartyWith(const_cast<Unit*>(target))))
            return false;
    }

    return true;
}

bool Unit::can_stealth_detect(Unit* target)
{
    // TODO: Should this code use your current viewpoint, or your "body"?
    // For some reason my memory is inclined to believe it's the latter, as
    // strange as it may seem.

    if (!target->can_stealth_against(this))
        return true;

    // If evading, or crowd controlled, NPCs cannot discover stealth
    if (GetTypeId() == TYPEID_UNIT &&
        (static_cast<Creature*>(this)->IsInEvadeMode() ||
            IsAffectedByThreatIgnoringCC()))
        return false;

    // If you're behind the target, and the target is not withint 1.5 yards,
    // you
    // can never see it
    bool in_front = HasInArc(M_PI_F, target);
    if (!in_front && !IsWithinDistInMap(target, 1.5f, true, false))
        return false;

    float detect_dist = stealth_detect_dist(target);

    if (detect_dist == 0.0f)
        return false;

    if (!IsWithinDistInMap(target, detect_dist, true, false))
    {
        // Creatures do a "grunt"-like reaction to nearby stealthed targets if
        // they're close to being seen
        if (GetTypeId() == TYPEID_UNIT &&
            !static_cast<Creature*>(this)->IsPet() &&
            IsWithinDist(target, detect_dist * 1.3f, false) && !isInCombat() &&
            !HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PASSIVE) &&
            IsHostileTo(target))
            static_cast<Creature*>(this)->stealth_reaction(target);
        return false;
    }

    return true;
}

bool Unit::HasDispellableBuff(DispelType type)
{
    uint32 mask = GetDispelMask(type);

    bool found = false;
    loop_auras([&found, mask](AuraHolder* holder)
        {
            const SpellEntry* spell = holder->GetSpellProto();
            if ((1 << spell->Dispel) & mask && holder->IsPositive())
                found = true;
            return !found; // break if found is true
        });

    return found;
}

bool Unit::HasDispellableDebuff(DispelType type)
{
    uint32 mask = GetDispelMask(type);

    bool found = false;
    loop_auras([&found, mask](AuraHolder* holder)
        {
            const SpellEntry* spell = holder->GetSpellProto();
            if ((1 << spell->Dispel) & mask && !holder->IsPositive())
                found = true;
            return !found; // break if found is true
        });

    return found;
}

float Unit::GetInterruptAndSilenceModifier(
    const SpellEntry* info, uint32 eff_mask) const
{
    uint32 mechanics = GetSpellMechanicMask(info, eff_mask);
    if (mechanics &
        ((1 << (MECHANIC_SILENCE - 1)) | (1 << (MECHANIC_INTERRUPT - 1))) == 0)
        return 1.0f;

    bool silence = mechanics & (1 << (MECHANIC_SILENCE - 1));
    float duration = 1.0f;
    float modifier = 1.0f;

    // Improved Conenctration Aura (Rank 1 to 3) : This is the only Area Aura
    // with this effect, so no need for a general check
    if (has_aura(19746,
            SPELL_AURA_MECHANIC_DURATION_MOD_NOT_STACK)) // Normal Concentration
    {
        Unit* pConstAway = const_cast<Unit*>(this);

        // Check if the caster of the aura affecting unitTarget has improved
        // Concenctration aura:
        if (AuraHolder* holder = pConstAway->get_aura(19746))
        {
            if (Unit* caster = holder->GetCaster())
            {
                // Imp. Conc. adds an aura per rank to the player:
                if (caster->has_aura(20254))
                    modifier = 0.9f;
                else if (caster->has_aura(20255))
                    modifier = 0.8f;
                else if (caster->has_aura(20256))
                    modifier = 0.7f;
            }
        }
    }
    if (modifier < duration)
        duration = modifier;

    const Auras& auras =
        GetAurasByType(SPELL_AURA_MECHANIC_DURATION_MOD_NOT_STACK);
    for (const auto& aura : auras)
    {
        const SpellEntry* mod_info = (aura)->GetSpellProto();

        for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
        {
            if (mod_info->EffectApplyAuraName[i] !=
                SPELL_AURA_MECHANIC_DURATION_MOD_NOT_STACK)
                continue;
            if (mod_info->EffectMiscValue[i] !=
                (silence ? MECHANIC_SILENCE : MECHANIC_INTERRUPT))
                continue;

            int32 reduc = mod_info->EffectBasePoints[i];

            if (reduc < 0)
                modifier = (100 + reduc + 1) / 100.0f;
        }

        if (modifier < duration)
            duration = modifier;
    }

    return duration;
}

std::vector<std::pair<AuraHolder*, uint32 /*stack amount*/>>
Unit::get_dispel_buffs(int count, uint32 dispel_mask, bool steal_buff,
    Unit* caster, const Spell* spell, std::vector<uint32>* fail_list,
    bool reflected, bool dry_run) const
{
    // Check cannot be IsFriendlyTo as mobs can be neutral towards one
    // another
    bool friendly_caster = reflected ? false : !IsHostileTo(caster);

    // When under the effect of a mind control, someone treated hostile
    // (your actual teammates) should be able to dispel your mind control
    if (HasAuraType(SPELL_AURA_MOD_POSSESS) && !friendly_caster)
        friendly_caster = true;

    // Build a list of all the potential spells we could dispel
    std::vector<AuraHolder*> potential;
    loop_auras([&potential, dispel_mask, friendly_caster, steal_buff, spell](
        AuraHolder* holder)
        {
            if (((1 << holder->GetSpellProto()->Dispel) & dispel_mask) == 0)
                return true; // continue
            // Friends can only dispel hostile buffs and enemies can only
            // dispel
            // friendly buffs
            // Possess effects (mind control) can also be dispelled by
            // enemies
            // (i.e. non-charmed allies)
            if (holder->IsPositive() == friendly_caster)
                return true; // continue
            if (steal_buff &&
                holder->GetSpellProto()->HasAttribute(
                    SPELL_ATTR_EX4_NOT_STEALABLE))
                return true; // continue
            // Peak dispel resistance; 100% means it's never considered
            auto caster = holder->GetCaster();
            Player* mod_owner;
            if (caster && (mod_owner = caster->GetSpellModOwner()) != nullptr)
            {
                int32 miss_chance = 0;
                mod_owner->ApplySpellMod(holder->GetSpellProto()->Id,
                    SPELLMOD_RESIST_DISPEL_CHANCE, miss_chance, spell, true);
                if (miss_chance >= 100)
                    return true; // continue
            }
            potential.push_back(holder);
            return true; // continue
        });

    std::vector<std::pair<AuraHolder*, uint32 /*stack amount*/>> dispels;

    if (dry_run)
    {
        dispels.resize(potential.size());
        for (size_t i = 0; i < potential.size(); ++i)
            dispels[i].first = potential[i];
        return dispels;
    }

    while (count > 0 && !potential.empty())
    {
        auto itr = potential.begin() + urand(0, potential.size() - 1);
        AuraHolder* holder = *itr;

        // We try to dispel once for each stack there is on this holder, or
        // until we run out of count
        uint32 dispelled_stacks = 0;
        for (int i = 0;
             count > 0 && static_cast<int>(holder->GetStackAmount()) > i;
             ++i, --count)
        {
            Unit* aura_caster;
            Player* mod_owner;
            if ((aura_caster = holder->GetCaster()) != nullptr &&
                (mod_owner = aura_caster->GetSpellModOwner()) != nullptr)
            {
                int32 miss_chance = 0;
                mod_owner->ApplySpellMod(holder->GetSpellProto()->Id,
                    SPELLMOD_RESIST_DISPEL_CHANCE, miss_chance, spell);
                if (miss_chance > 0 && roll_chance_i(miss_chance))
                {
                    if (fail_list)
                        fail_list->push_back(holder->GetSpellProto()->Id);
                    continue;
                }
            }

            // Spell accepted
            ++dispelled_stacks;
        }

        if (dispelled_stacks > 0)
            dispels.push_back(std::make_pair(holder, dispelled_stacks));

        // Remove this spell from potential; we cannot consider it again
        potential.erase(itr);
    }

    return dispels;
}

Unit* Unit::GetCharmerOrOwnerOrTotemOwner() const
{
    if (GetCharmerGuid())
    {
        return GetCharmer();
    }
    else if (GetOwnerGuid())
    {
        Unit* owner = GetOwner();
        if (owner && owner->GetTypeId() == TYPEID_UNIT &&
            static_cast<Creature*>(owner)->IsTotem())
            return owner->GetOwner();
        return owner;
    }
    return nullptr;
}

bool Unit::player_or_pet() const
{
    if (GetTypeId() == TYPEID_PLAYER)
        return true;
    else
    {
        if (static_cast<const Creature*>(this)->IsPet())
            return GetOwnerGuid().IsPlayer();
    }
    return false;
}

void Unit::queue_spell_hit(Spell* spell)
{
    // When a hostile spell is added to the queue, pending
    // SPELL_AURA_MOD_STEALTHs are removed (vanish, e.g.)
    if (IsHostileTo(spell->GetCaster()))
    {
        for (auto itr = spell_queue_.begin(); itr != spell_queue_.end();)
        {
            if (itr->spell->m_spellInfo->HasApplyAuraName(
                    SPELL_AURA_MOD_STEALTH))
            {
                // Remove cooldown for SPELL_ATTR_DISABLED_WHILE_ACTIVE
                if (itr->spell->m_spellInfo->HasAttribute(
                        SPELL_ATTR_DISABLED_WHILE_ACTIVE) &&
                    GetMap()->GetUnit(itr->spell->GetCasterGUID()) &&
                    itr->spell->GetCaster()->GetTypeId() == TYPEID_PLAYER)
                {
                    static_cast<Player*>(itr->spell->GetCaster())
                        ->RemoveSpellCooldown(
                            itr->spell->m_spellInfo->Id, true);
                }
                itr = spell_queue_.erase(itr);
            }
            else
                ++itr;
        }
    }

    spell_queue_.emplace_back(spell->ref_counter(), spell);
}

void Unit::update_spell_queue()
{
    // NOTE: queue_spell_hit can be called while we're updating, make a copy
    // and
    // clear original
    std::vector<spell_ref> copy(spell_queue_.begin(), spell_queue_.end());
    spell_queue_.clear();

    for (auto& ref : copy)
    {
        // We cannot process our hit if the caster is no longer in our map
        // (would be thread-unsafe)
        Unit* caster = GetMap()->GetUnit(ref.spell->GetCasterGUID());
        if (!caster)
            continue;

        ref.spell->DoAllEffectOnTarget(this);
    }
}

void Unit::update_interrupt_mask()
{
    interrupt_mask_ = 0;
    for (auto& aura : interruptible_auras_)
        interrupt_mask_ |= aura->GetSpellProto()->AuraInterruptFlags;
}

void Unit::force_stealth_update_timer(uint32 update_in)
{
    if (update_in >= stealth_update_timer_.GetInterval())
        stealth_update_timer_.SetCurrent(0);
    else
        stealth_update_timer_.SetCurrent(
            stealth_update_timer_.GetInterval() - update_in);
}
