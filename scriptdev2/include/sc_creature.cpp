/* Copyright (C) 2006 - 2012 ScriptDev2 <http://www.scriptdev2.com/>
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#include "BehavioralAI.h"
#include "Item.h"
#include "ObjectMgr.h"
#include "SpecialVisCreature.h"
#include "Totem.h"
#include "TemporarySummon.h"
#include "Spell.h"
#include "WorldPacket.h"
#include "precompiled.h"
#include "maps/checks.h"
#include "maps/visitors.h"

// Spell summary for ScriptedAI::SelectSpell
struct TSpellSummary
{
    uint8 Targets; // set of enum SelectTarget
    uint8 Effects; // set of enum SelectEffect
} * SpellSummary;

ScriptedAI::ScriptedAI(Creature* pCreature)
  : CreatureAI(pCreature), m_bCombatMovement(true),
    m_uiEvadeCheckCooldown(2500), m_uiLastKillSay(0)
{
}

/// This function shows if combat movement is enabled, overwrite for more info
void ScriptedAI::GetAIInformation(ChatHandler& reader)
{
    reader.PSendSysMessage("ScriptedAI, combat movement is %s",
        reader.GetOnOffStr(m_bCombatMovement));
}

/// Return if the creature can "see" pWho
bool ScriptedAI::IsVisible(Unit* pWho) const
{
    if (!pWho)
        return false;

    return m_creature->IsWithinDist(pWho, VISIBLE_RANGE) &&
           pWho->can_be_seen_by(m_creature, m_creature);
}

/**
 * This function triggers the creature attacking pWho, depending on conditions
 * like:
 * - Can the creature start an attack?
 * - Is pWho hostile to the creature?
 * - Can the creature reach pWho?
 * - Is pWho in aggro-range?
 * If the creature can attack pWho, it will if it has no victim.
 * Inside dungeons, the creature will get into combat with pWho, even if it has
 * already a victim
 */
void ScriptedAI::MoveInLineOfSight(Unit* pWho)
{
    if (!m_creature->CanStartAttacking(pWho))
        return;

    if (m_creature->IsWithinAggroDistance(pWho) &&
        m_creature->IsWithinWmoLOSInMap(pWho))
    {
        if (!m_creature->getVictim())
        {
            AttackStart(pWho);
        }
        else if (m_creature->GetMap()->IsDungeon())
        {
            pWho->SetInCombatWith(m_creature);
            m_creature->AddThreat(pWho);
        }
    }
}

/**
 * This function sets the TargetGuid for the creature if required
 * Also it will handle the combat movement (chase movement), depending on
 * SetCombatMovement(bool)
 */
void ScriptedAI::AttackStart(Unit* pWho)
{
    if (pWho && m_creature->Attack(pWho, !IsPacified()))
    {
        m_creature->AddThreat(pWho);
        m_creature->SetInCombatWith(pWho);
        pWho->SetInCombatWith(m_creature);
        if (pWho->GetTypeId() == TYPEID_UNIT)
            pWho->AddThreat(m_creature);

        m_creature->movement_gens.on_event(movement::EVENT_ENTER_COMBAT);

        if (!m_creature->movement_gens.has(movement::gen::chase))
            m_creature->movement_gens.push(
                new movement::ChaseMovementGenerator(),
                movement::EVENT_LEAVE_COMBAT);

        if (!m_creature->movement_gens.has(movement::gen::home))
            m_creature->movement_gens.push(
                new movement::HomeMovementGenerator());

        if (!IsCombatMovement())
        {
            if (!m_creature->movement_gens.has(movement::gen::stopped))
                m_creature->movement_gens.push(
                    new movement::StoppedMovementGenerator(),
                    movement::EVENT_LEAVE_COMBAT);
        }
    }
}

/**
 * This function only calls Aggro, which is to be used for scripting purposes
 */
void ScriptedAI::EnterCombat(Unit* pEnemy)
{
    if (pEnemy)
        Aggro(pEnemy);
}

/**
 * Main update function, by default let the creature behave as expected by a mob
 * (threat management and melee dmg)
 * Always handle here threat-management with m_creature->SelectHostileTarget()
 * Handle (if required) melee attack with DoMeleeAttackIfReady()
 * This is usally overwritten to support timers for ie spells
 */
void ScriptedAI::UpdateAI(const uint32 /*uiDiff*/)
{
    // Check if we have a current target
    if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
        return;

    DoMeleeAttackIfReady();
}

/**
 * This function cleans up the combat state if the creature evades
 */
void ScriptedAI::EnterEvadeMode(bool by_group)
{
    // process creature evade actions
    m_creature->OnEvadeActions(by_group);

    if (!m_creature->isAlive())
        return;

    m_creature->remove_auras_on_evade();
    m_creature->DeleteThreatList();
    m_creature->CombatStop(true);
    m_creature->ResetLootRecipients();

    m_creature->movement_gens.on_event(movement::EVENT_LEAVE_COMBAT);

    Reset();
}

/// This function calls Reset() to reset variables as expected
void ScriptedAI::JustRespawned()
{
    Reset();
}

void ScriptedAI::DoCast(Unit* pTarget, uint32 uiSpellId, bool bTriggered)
{
    if (m_creature->IsNonMeleeSpellCasted(false) && !bTriggered)
        return;

    m_creature->CastSpell(pTarget, uiSpellId, bTriggered);
}

void ScriptedAI::DoCastSpell(
    Unit* pTarget, SpellEntry const* pSpellInfo, bool bTriggered)
{
    if (m_creature->IsNonMeleeSpellCasted(false) && !bTriggered)
        return;

    m_creature->CastSpell(pTarget, pSpellInfo, bTriggered);
}

void ScriptedAI::DoPlaySoundToSet(WorldObject* pSource, uint32 uiSoundId)
{
    if (!pSource)
        return;

    if (!GetSoundEntriesStore()->LookupEntry(uiSoundId))
    {
        logging.error(
            "SD2: Invalid soundId %u used in DoPlaySoundToSet (Source: TypeId "
            "%u, GUID %u)",
            uiSoundId, pSource->GetTypeId(), pSource->GetGUIDLow());
        return;
    }

    pSource->PlayDirectSound(uiSoundId);
}

Creature* ScriptedAI::DoSpawnCreature(uint32 uiId, float fX, float fY, float fZ,
    float fAngle, uint32 uiType, uint32 uiDespawntime)
{
    return m_creature->SummonCreature(uiId, m_creature->GetX() + fX,
        m_creature->GetY() + fY, m_creature->GetZ() + fZ, fAngle,
        (TempSummonType)uiType, uiDespawntime);
}

SpellEntry const* ScriptedAI::SelectSpell(Unit* pTarget, int32 uiSchool,
    int32 iMechanic, SelectTarget selectTargets, uint32 uiPowerCostMin,
    uint32 uiPowerCostMax, float fRangeMin, float fRangeMax,
    SelectEffect selectEffects)
{
    // No target so we can't cast
    if (!pTarget)
        return nullptr;

    // Silenced so we can't cast
    if (m_creature->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SILENCED))
        return nullptr;

    // Using the extended script system we first create a list of viable spells
    SpellEntry const* apSpell[4];
    memset(apSpell, 0, sizeof(SpellEntry*) * 4);

    uint32 uiSpellCount = 0;

    SpellEntry const* pTempSpell;
    SpellRangeEntry const* pTempRange;

    // Check if each spell is viable(set it to null if not)
    for (uint8 i = 0; i < 4; ++i)
    {
        pTempSpell = GetSpellStore()->LookupEntry(m_creature->m_spells[i]);

        // This spell doesn't exist
        if (!pTempSpell)
            continue;

        // Targets and Effects checked first as most used restrictions
        // Check the spell targets if specified
        if (selectTargets &&
            !(SpellSummary[m_creature->m_spells[i]].Targets &
                (1 << (selectTargets - 1))))
            continue;

        // Check the type of spell if we are looking for a specific spell type
        if (selectEffects &&
            !(SpellSummary[m_creature->m_spells[i]].Effects &
                (1 << (selectEffects - 1))))
            continue;

        // Check for school if specified
        if (uiSchool >= 0 && pTempSpell->SchoolMask & uiSchool)
            continue;

        // Check for spell mechanic if specified
        if (iMechanic >= 0 && pTempSpell->Mechanic != (uint32)iMechanic)
            continue;

        // Make sure that the spell uses the requested amount of power
        if (uiPowerCostMin && pTempSpell->manaCost < uiPowerCostMin)
            continue;

        if (uiPowerCostMax && pTempSpell->manaCost > uiPowerCostMax)
            continue;

        // Continue if we don't have the mana to actually cast this spell
        if (pTempSpell->manaCost >
            m_creature->GetPower((Powers)pTempSpell->powerType))
            continue;

        // Get the Range
        pTempRange = GetSpellRangeStore()->LookupEntry(pTempSpell->rangeIndex);

        // Spell has invalid range store so we can't use it
        if (!pTempRange)
            continue;

        // Check if the spell meets our range requirements
        if (fRangeMin && pTempRange->maxRange < fRangeMin)
            continue;

        if (fRangeMax && pTempRange->maxRange > fRangeMax)
            continue;

        // Check if our target is in range
        if (m_creature->IsWithinDistInMap(pTarget, pTempRange->minRange) ||
            !m_creature->IsWithinDistInMap(pTarget, pTempRange->maxRange))
            continue;

        // All good so lets add it to the spell list
        apSpell[uiSpellCount] = pTempSpell;
        ++uiSpellCount;
    }

    // We got our usable spells so now lets randomly pick one
    if (!uiSpellCount)
        return nullptr;

    return apSpell[urand(0, uiSpellCount - 1)];
}

bool ScriptedAI::CanCast(
    Unit* pTarget, SpellEntry const* pSpellEntry, bool bTriggered)
{
    // No target so we can't cast
    if (!pTarget || !pSpellEntry)
        return false;

    // Silenced so we can't cast
    if (!bTriggered &&
        m_creature->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SILENCED))
        return false;

    // Check for power
    if (!bTriggered &&
        m_creature->GetPower((Powers)pSpellEntry->powerType) <
            pSpellEntry->manaCost)
        return false;

    SpellRangeEntry const* pTempRange =
        GetSpellRangeStore()->LookupEntry(pSpellEntry->rangeIndex);

    // Spell has invalid range store so we can't use it
    if (!pTempRange)
        return false;

    // Unit is out of range of this spell
    if (!m_creature->IsInRange(
            pTarget, pTempRange->minRange, pTempRange->maxRange))
        return false;

    return true;
}

void FillSpellSummary()
{
    SpellSummary = new TSpellSummary[GetSpellStore()->GetNumRows()];

    SpellEntry const* pTempSpell;

    for (uint32 i = 0; i < GetSpellStore()->GetNumRows(); ++i)
    {
        SpellSummary[i].Effects = 0;
        SpellSummary[i].Targets = 0;

        pTempSpell = GetSpellStore()->LookupEntry(i);
        // This spell doesn't exist
        if (!pTempSpell)
            continue;

        for (uint8 j = 0; j < 3; ++j)
        {
            // Spell targets self
            if (pTempSpell->EffectImplicitTargetA[j] == TARGET_SELF)
                SpellSummary[i].Targets |= 1 << (SELECT_TARGET_SELF - 1);

            // Spell targets a single enemy
            if (pTempSpell->EffectImplicitTargetA[j] == TARGET_CHAIN_DAMAGE ||
                pTempSpell->EffectImplicitTargetA[j] ==
                    TARGET_CURRENT_ENEMY_COORDINATES)
                SpellSummary[i].Targets |= 1
                                           << (SELECT_TARGET_SINGLE_ENEMY - 1);

            // Spell targets AoE at enemy
            if (pTempSpell->EffectImplicitTargetA[j] ==
                    TARGET_ALL_ENEMY_IN_AREA ||
                pTempSpell->EffectImplicitTargetA[j] ==
                    TARGET_ALL_ENEMY_IN_AREA_INSTANT ||
                pTempSpell->EffectImplicitTargetA[j] ==
                    TARGET_CASTER_COORDINATES ||
                pTempSpell->EffectImplicitTargetA[j] ==
                    TARGET_ALL_ENEMY_IN_AREA_CHANNELED)
                SpellSummary[i].Targets |= 1 << (SELECT_TARGET_AOE_ENEMY - 1);

            // Spell targets an enemy
            if (pTempSpell->EffectImplicitTargetA[j] == TARGET_CHAIN_DAMAGE ||
                pTempSpell->EffectImplicitTargetA[j] ==
                    TARGET_CURRENT_ENEMY_COORDINATES ||
                pTempSpell->EffectImplicitTargetA[j] ==
                    TARGET_ALL_ENEMY_IN_AREA ||
                pTempSpell->EffectImplicitTargetA[j] ==
                    TARGET_ALL_ENEMY_IN_AREA_INSTANT ||
                pTempSpell->EffectImplicitTargetA[j] ==
                    TARGET_CASTER_COORDINATES ||
                pTempSpell->EffectImplicitTargetA[j] ==
                    TARGET_ALL_ENEMY_IN_AREA_CHANNELED)
                SpellSummary[i].Targets |= 1 << (SELECT_TARGET_ANY_ENEMY - 1);

            // Spell targets a single friend(or self)
            if (pTempSpell->EffectImplicitTargetA[j] == TARGET_SELF ||
                pTempSpell->EffectImplicitTargetA[j] == TARGET_SINGLE_FRIEND ||
                pTempSpell->EffectImplicitTargetA[j] == TARGET_SINGLE_PARTY)
                SpellSummary[i].Targets |= 1
                                           << (SELECT_TARGET_SINGLE_FRIEND - 1);

            // Spell targets aoe friends
            if (pTempSpell->EffectImplicitTargetA[j] ==
                    TARGET_ALL_PARTY_AROUND_CASTER ||
                pTempSpell->EffectImplicitTargetA[j] ==
                    TARGET_AREAEFFECT_PARTY ||
                pTempSpell->EffectImplicitTargetA[j] ==
                    TARGET_CASTER_COORDINATES)
                SpellSummary[i].Targets |= 1 << (SELECT_TARGET_AOE_FRIEND - 1);

            // Spell targets any friend(or self)
            if (pTempSpell->EffectImplicitTargetA[j] == TARGET_SELF ||
                pTempSpell->EffectImplicitTargetA[j] == TARGET_SINGLE_FRIEND ||
                pTempSpell->EffectImplicitTargetA[j] == TARGET_SINGLE_PARTY ||
                pTempSpell->EffectImplicitTargetA[j] ==
                    TARGET_ALL_PARTY_AROUND_CASTER ||
                pTempSpell->EffectImplicitTargetA[j] ==
                    TARGET_AREAEFFECT_PARTY ||
                pTempSpell->EffectImplicitTargetA[j] ==
                    TARGET_CASTER_COORDINATES)
                SpellSummary[i].Targets |= 1 << (SELECT_TARGET_ANY_FRIEND - 1);

            // Make sure that this spell includes a damage effect
            if (pTempSpell->Effect[j] == SPELL_EFFECT_SCHOOL_DAMAGE ||
                pTempSpell->Effect[j] == SPELL_EFFECT_INSTAKILL ||
                pTempSpell->Effect[j] == SPELL_EFFECT_ENVIRONMENTAL_DAMAGE ||
                pTempSpell->Effect[j] == SPELL_EFFECT_HEALTH_LEECH)
                SpellSummary[i].Effects |= 1 << (SELECT_EFFECT_DAMAGE - 1);

            // Make sure that this spell includes a healing effect (or an apply
            // aura with a periodic heal)
            if (pTempSpell->Effect[j] == SPELL_EFFECT_HEAL ||
                pTempSpell->Effect[j] == SPELL_EFFECT_HEAL_MAX_HEALTH ||
                pTempSpell->Effect[j] == SPELL_EFFECT_HEAL_MECHANICAL ||
                (pTempSpell->Effect[j] == SPELL_EFFECT_APPLY_AURA &&
                    pTempSpell->EffectApplyAuraName[j] == 8))
                SpellSummary[i].Effects |= 1 << (SELECT_EFFECT_HEALING - 1);

            // Make sure that this spell applies an aura
            if (pTempSpell->Effect[j] == SPELL_EFFECT_APPLY_AURA)
                SpellSummary[i].Effects |= 1 << (SELECT_EFFECT_AURA - 1);
        }
    }
}

void ScriptedAI::DoResetThreat()
{
    if (!m_creature->CanHaveThreatList() ||
        m_creature->getThreatManager().isThreatListEmpty())
    {
        logging.error(
            "SD2: DoResetThreat called for creature that either cannot have "
            "threat list or has empty threat list (m_creature entry = %d)",
            m_creature->GetEntry());
        return;
    }

    ThreatList const& tList = m_creature->getThreatManager().getThreatList();
    for (const auto& elem : tList)
    {
        Unit* pUnit = m_creature->GetMap()->GetUnit((elem)->getUnitGuid());

        if (pUnit && m_creature->getThreatManager().getThreat(pUnit))
            m_creature->getThreatManager().modifyThreatPercent(pUnit, -100);
    }
}

void ScriptedAI::DoTeleportPlayer(
    Unit* pUnit, float fX, float fY, float fZ, float fO)
{
    if (!pUnit)
        return;

    if (pUnit->GetTypeId() != TYPEID_PLAYER)
    {
        logging.error(
            "SD2: %s tried to teleport non-player (%s) to x: %f y:%f z: %f o: "
            "%f. Aborted.",
            m_creature->GetGuidStr().c_str(), pUnit->GetGuidStr().c_str(), fX,
            fY, fZ, fO);
        return;
    }

    ((Player*)pUnit)
        ->TeleportTo(
            pUnit->GetMapId(), fX, fY, fZ, fO, TELE_TO_NOT_LEAVE_COMBAT);
}

Unit* ScriptedAI::DoSelectLowestHpFriendly(float fRange, uint32 uiMinHPDiff)
{
    maps::checks::hurt_friend check{m_creature, false};
    auto target = maps::visitors::yield_best_match<Creature, Creature,
        SpecialVisCreature, TemporarySummon>{}(m_creature, fRange, check);
    if (target && target->GetMaxHealth() - target->GetHealth() < uiMinHPDiff)
        return nullptr;
    return target;
}

std::vector<Creature*> ScriptedAI::DoFindFriendlyCC(float fRange)
{
    return maps::visitors::yield_set<Creature>{}(m_creature, fRange,
        maps::checks::friendly_crowd_controlled{m_creature});
}

std::vector<Creature*> ScriptedAI::DoFindFriendlyMissingBuff(
    float fRange, uint32 uiSpellId)
{
    maps::checks::missing_buff check{m_creature, uiSpellId};
    return maps::visitors::yield_set<Creature>{}(m_creature, fRange, check);
}

Player* ScriptedAI::GetPlayerAtMinimumRange(float fMinimumRange)
{
    return maps::visitors::yield_single<Player>{}(m_creature, fMinimumRange,
        [](Player* p)
        {
            return p->isAlive() && !p->isGameMaster();
        });
}

void ScriptedAI::SetEquipmentSlots(
    bool bLoadDefault, int32 iMainHand, int32 iOffHand, int32 iRanged)
{
    if (bLoadDefault)
    {
        m_creature->LoadEquipment(
            m_creature->GetCreatureInfo()->equipmentId, true);
        return;
    }

    if (iMainHand >= 0)
        m_creature->SetVirtualItem(VIRTUAL_ITEM_SLOT_0, iMainHand);

    if (iOffHand >= 0)
        m_creature->SetVirtualItem(VIRTUAL_ITEM_SLOT_1, iOffHand);

    if (iRanged >= 0)
        m_creature->SetVirtualItem(VIRTUAL_ITEM_SLOT_2, iRanged);
}

void ScriptedAI::SetCombatMovement(bool bCombatMove)
{
    m_bCombatMovement = bCombatMove;

    if (!m_creature->isInCombat())
        return;

    if (bCombatMove)
    {
        m_creature->movement_gens.remove_all(movement::gen::stopped);

        if (!m_creature->movement_gens.has(movement::gen::chase))
            m_creature->movement_gens.push(
                new movement::ChaseMovementGenerator(),
                movement::EVENT_LEAVE_COMBAT);
    }
    else
    {
        m_creature->movement_gens.push(new movement::StoppedMovementGenerator(),
            movement::EVENT_LEAVE_COMBAT);
    }
}

// Hacklike storage used for misc creatures that are expected to evade of
// outside of a certain area.
// It is assumed the information is found elswehere and can be handled by
// mangos. So far no luck finding such information/way to extract it.
enum
{
    NPC_BROODLORD = 12017,
    NPC_VOID_REAVER = 19516,
    NPC_JAN_ALAI = 23578,
    NPC_SARTHARION = 28860,
    NPC_TALON_KING_IKISS = 18473,
    NPC_KARGATH_BLADEFIST = 16808,
};

bool ScriptedAI::EnterEvadeIfOutOfCombatArea(const uint32 uiDiff)
{
    if (m_uiEvadeCheckCooldown < uiDiff)
        m_uiEvadeCheckCooldown = 2500;
    else
    {
        m_uiEvadeCheckCooldown -= uiDiff;
        return false;
    }

    if (m_creature->IsInEvadeMode() || !m_creature->getVictim())
        return false;

    float fX = m_creature->GetX();
    float fY = m_creature->GetY();
    float fZ = m_creature->GetZ();

    switch (m_creature->GetEntry())
    {
    case NPC_BROODLORD: // broodlord (not move down stairs)
        if (fZ > 448.60f)
            return false;
        break;
    case NPC_JAN_ALAI: // jan'alai (calculate by Z)
        if (fZ > 12.0f)
            return false;
        break;
    case NPC_SARTHARION: // sartharion (calculate box)
        if (fX > 3218.86f && fX < 3275.69f && fY < 572.40f && fY > 484.68f)
            return false;
        break;
    case NPC_TALON_KING_IKISS:
    {
        float fX, fY, fZ;
        m_creature->GetRespawnCoord(fX, fY, fZ);
        if (m_creature->GetDistance2d(fX, fY) < 70.0f)
            return false;
        break;
    }
    case NPC_KARGATH_BLADEFIST:
        if (fX < 268.8f && fX > 185.7f)
            return false;
        break;
    default:
        logging.error(
            "SD2: EnterEvadeIfOutOfCombatArea used for creature entry %u, but "
            "does not have any definition.",
            m_creature->GetEntry());
        return false;
    }

    EnterEvadeMode();
    return true;
}

Scripted_BehavioralAI::Scripted_BehavioralAI(Creature* pCreature)
  : ScriptedAI(pCreature)
{
    m_behavioralAI = new BehavioralAI(pCreature);
}

Scripted_BehavioralAI::~Scripted_BehavioralAI()
{
    delete m_behavioralAI;
}

void Scripted_BehavioralAI::GetAIInformation(ChatHandler& reader)
{
    reader.PSendSysMessage("Subclass of Scripted_BehavioralAI");
    reader.PSendSysMessage("BehavioralAI: %s", m_behavioralAI->debug().c_str());
}

void Scripted_BehavioralAI::ToggleBehavioralAI(bool on)
{
    m_behavioralAI->ToggleBehavior(on);
}

void Scripted_BehavioralAI::ChangeBehavior(uint32 behavior)
{
    m_behavioralAI->ChangeBehavior(behavior);
}

void Scripted_BehavioralAI::Reset()
{
    m_behavioralAI->OnReset();
}

void Scripted_BehavioralAI::UpdateInCombatAI(const uint32 uiDiff)
{
    m_behavioralAI->Update(uiDiff);
}

void Scripted_BehavioralAI::AttackStart(Unit* who)
{
    if (m_behavioralAI->TurnedOn())
    {
        if (who && m_creature->Attack(who, !IsPacified()))
        {
            m_creature->AddThreat(who);
            m_creature->SetInCombatWith(who);
            who->SetInCombatWith(m_creature);

            m_creature->movement_gens.on_event(movement::EVENT_ENTER_COMBAT);

            m_behavioralAI->OnAttackStart();
            if (!m_creature->movement_gens.has(movement::gen::home))
                m_creature->movement_gens.push(
                    new movement::HomeMovementGenerator());
        }
    }
    else
    {
        ScriptedAI::AttackStart(who);
    }
}

bool Scripted_BehavioralAI::IgnoreTarget(Unit* target) const
{
    if (m_behavioralAI->TurnedOn())
        return m_behavioralAI->IgnoreTarget(target);
    return CreatureAI::IgnoreTarget(target);
}

void Scripted_BehavioralAI::SetBehavioralPhase(int phase)
{
    m_behavioralAI->SetPhase(phase);
}
