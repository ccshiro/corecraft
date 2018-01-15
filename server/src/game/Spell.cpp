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

#include "Spell.h"
#include "BattleGround.h"
#include "Chat.h"
#include "ConditionMgr.h"
#include "DynamicObject.h"
#include "Group.h"
#include "logging.h"
#include "LootMgr.h"
#include "MapManager.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "PathFinder.h"
#include "Pet.h"
#include "Player.h"
#include "movement/PointMovementGenerator.h"
#include "SharedDefines.h"
#include "SpellMgr.h"
#include "SpecialVisCreature.h"
#include "Totem.h"
#include "TemporarySummon.h"
#include "Unit.h"
#include "UpdateData.h"
#include "UpdateMask.h"
#include "Util.h"
#include "VMapFactory.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "buff_stacking.h"
#include "loot_distributor.h"
#include "pet_behavior.h"
#include "Database/DatabaseEnv.h"
#include "Policies/Singleton.h"
#include "inventory/trade.h"
#include "maps/checks.h"
#include "maps/visitors.h"

extern pEffect SpellEffects[TOTAL_SPELL_EFFECTS];

bool IsQuestTameSpell(uint32 spellId)
{
    SpellEntry const* spellproto = sSpellStore.LookupEntry(spellId);
    if (!spellproto)
        return false;

    return spellproto->Effect[EFFECT_INDEX_0] == SPELL_EFFECT_THREAT &&
           spellproto->Effect[EFFECT_INDEX_1] == SPELL_EFFECT_APPLY_AURA &&
           spellproto->EffectApplyAuraName[EFFECT_INDEX_1] == SPELL_AURA_DUMMY;
}

SpellCastTargets::SpellCastTargets()
{
    m_unitTarget = nullptr;
    m_itemTarget = nullptr;
    m_GOTarget = nullptr;

    m_itemTargetEntry = 0;

    m_srcX = m_srcY = m_srcZ = m_destX = m_destY = m_destZ = 0.0f;
    m_strTarget = "";
    m_targetMask = 0;
}

SpellCastTargets::~SpellCastTargets()
{
}

void SpellCastTargets::setUnitTarget(Unit* target)
{
    if (!target)
        return;

    m_destX = target->GetX();
    m_destY = target->GetY();
    m_destZ = target->GetZ();
    m_unitTarget = target;
    m_unitTargetGUID = target->GetObjectGuid();
    m_targetMask |= TARGET_FLAG_UNIT;
}

void SpellCastTargets::setDestination(float x, float y, float z)
{
    m_destX = x;
    m_destY = y;
    m_destZ = z;
    m_targetMask |= TARGET_FLAG_DEST_LOCATION;
}

void SpellCastTargets::setSource(float x, float y, float z)
{
    m_srcX = x;
    m_srcY = y;
    m_srcZ = z;
    m_targetMask |= TARGET_FLAG_SOURCE_LOCATION;
}

void SpellCastTargets::setScriptTarget(Unit* target)
{
    if (!target)
        return;

    m_destX = target->GetX();
    m_destY = target->GetY();
    m_destZ = target->GetZ();
    m_scriptTargetGUID = target->GetObjectGuid();
    m_targetMask |= TARGET_FLAG_UNIT;
}

void SpellCastTargets::setGOTarget(GameObject* target)
{
    m_GOTarget = target;
    m_GOTargetGUID = target->GetObjectGuid();
    //    m_targetMask |= TARGET_FLAG_OBJECT;
}

void SpellCastTargets::setItemTarget(Item* item)
{
    if (!item)
        return;

    m_itemTarget = item;
    m_itemTargetGUID = item->GetObjectGuid();
    m_itemTargetEntry = item->GetEntry();
    m_targetMask |= TARGET_FLAG_ITEM;
}

void SpellCastTargets::setTradeItemTarget(Player* caster)
{
    m_itemTargetGUID =
        ObjectGuid(); // The correct target will be set in ::Update()
    m_itemTargetEntry = 0;
    m_targetMask |= TARGET_FLAG_TRADE_ITEM;

    Update(caster);
}

void SpellCastTargets::setCorpseTarget(Corpse* corpse)
{
    m_CorpseTargetGUID = corpse->GetObjectGuid();
}

void SpellCastTargets::Update(Unit* caster)
{
    m_GOTarget = m_GOTargetGUID ?
                     caster->GetMap()->GetGameObject(m_GOTargetGUID) :
                     nullptr;
    m_unitTarget =
        m_unitTargetGUID ?
            (m_unitTargetGUID == caster->GetObjectGuid() ?
                    caster :
                    ObjectAccessor::GetUnit(*caster, m_unitTargetGUID)) :
            nullptr;

    m_itemTarget = nullptr;
    if (caster->GetTypeId() == TYPEID_PLAYER)
    {
        Player* player = ((Player*)caster);

        if (m_targetMask & TARGET_FLAG_ITEM)
            m_itemTarget = player->GetItemByGuid(m_itemTargetGUID);
        else if (m_targetMask & TARGET_FLAG_TRADE_ITEM)
        {
            if (player->trade())
            {
                m_itemTarget = player->trade()->spell_target(player);
                // ObjectGuid is not set previously for traded items (in
                // setItemTarget), so we set it here
                if (m_itemTarget)
                    m_itemTargetGUID = m_itemTarget->GetObjectGuid();
            }
        }

        if (m_itemTarget)
            m_itemTargetEntry = m_itemTarget->GetEntry();
    }
}

void SpellCastTargets::read(ByteBuffer& data, Unit* caster)
{
    data >> m_targetMask;

    if (m_targetMask == TARGET_FLAG_SELF)
    {
        m_destX = caster->GetX();
        m_destY = caster->GetY();
        m_destZ = caster->GetZ();
        m_unitTarget = caster;
        m_unitTargetGUID = caster->GetObjectGuid();
        return;
    }

    // TARGET_FLAG_UNK2 is used for non-combat pets, maybe other?
    if (m_targetMask & (TARGET_FLAG_UNIT | TARGET_FLAG_UNK2))
        data >> m_unitTargetGUID.ReadAsPacked();

    if (m_targetMask & (TARGET_FLAG_OBJECT | TARGET_FLAG_OBJECT_UNK))
        data >> m_GOTargetGUID.ReadAsPacked();

    if ((m_targetMask & (TARGET_FLAG_ITEM | TARGET_FLAG_TRADE_ITEM)) &&
        caster->GetTypeId() == TYPEID_PLAYER)
        data >> m_itemTargetGUID.ReadAsPacked();

    if (m_targetMask & TARGET_FLAG_SOURCE_LOCATION)
    {
        data >> m_srcX >> m_srcY >> m_srcZ;
        if (!maps::verify_coords(m_srcX, m_srcY))
            throw ByteBufferException(false, data.rpos(), 0, data.size());
    }

    if (m_targetMask & TARGET_FLAG_DEST_LOCATION)
    {
        data >> m_destX >> m_destY >> m_destZ;
        if (!maps::verify_coords(m_destX, m_destY))
            throw ByteBufferException(false, data.rpos(), 0, data.size());
    }

    if (m_targetMask & TARGET_FLAG_STRING)
        data >> m_strTarget;

    if (m_targetMask & (TARGET_FLAG_CORPSE | TARGET_FLAG_PVP_CORPSE))
        data >> m_CorpseTargetGUID.ReadAsPacked();

    // find real units/GOs
    Update(caster);
}

void SpellCastTargets::write(ByteBuffer& data) const
{
    data << uint32(m_targetMask);

    if (m_targetMask &
        (TARGET_FLAG_UNIT | TARGET_FLAG_PVP_CORPSE | TARGET_FLAG_OBJECT |
            TARGET_FLAG_CORPSE | TARGET_FLAG_UNK2))
    {
        if (m_targetMask & TARGET_FLAG_UNIT)
        {
            if (m_scriptTargetGUID)
                data << m_scriptTargetGUID.WriteAsPacked();
            else if (m_unitTarget)
                data << m_unitTarget->GetPackGUID();
            else
                data << uint8(0);
        }
        else if (m_targetMask & (TARGET_FLAG_OBJECT | TARGET_FLAG_OBJECT_UNK))
        {
            if (m_GOTarget)
                data << m_GOTarget->GetPackGUID();
            else
                data << uint8(0);
        }
        else if (m_targetMask & (TARGET_FLAG_CORPSE | TARGET_FLAG_PVP_CORPSE))
            data << m_CorpseTargetGUID.WriteAsPacked();
        else
            data << uint8(0);
    }

    if (m_targetMask & (TARGET_FLAG_ITEM | TARGET_FLAG_TRADE_ITEM))
    {
        if (m_itemTarget)
            data << m_itemTarget->GetPackGUID();
        else
            data << uint8(0);
    }

    if (m_targetMask & TARGET_FLAG_SOURCE_LOCATION)
        data << m_srcX << m_srcY << m_srcZ;

    if (m_targetMask & TARGET_FLAG_DEST_LOCATION)
        data << m_destX << m_destY << m_destZ;

    if (m_targetMask & TARGET_FLAG_STRING)
        data << m_strTarget;
}

Spell::Spell(Unit* caster, SpellEntry const* info, spell_trigger_type triggered,
    ObjectGuid originalCasterGUID, SpellEntry const* triggeredBy)
{
    assert(caster != nullptr && info != nullptr);
    assert(info == sSpellStore.LookupEntry(info->Id) &&
           "`info` must be pointer to sSpellStore element");

    m_spellInfo = info;
    m_triggeredBySpellInfo = triggeredBy;
    m_caster = caster;
    m_casterGUID = caster->GetObjectGuid();
    m_selfContainer = nullptr;
    m_referencedFromCurrentSpell = false;
    m_executedCurrently = false;
    m_delayStart = 0;
    m_delayAtDamageCount = 0;

    m_applyMultiplierMask = 0;

    // Get data for type of attack
    m_attackType = GetWeaponAttackType(m_spellInfo);

    m_spellSchoolMask = GetSpellSchoolMask(
        info); // Can be override for some spell (wand shoot for example)

    if (IsWandAttack())
    {
        if (Item* item = static_cast<Player*>(m_caster)->GetWeaponForAttack(
                RANGED_ATTACK))
            m_spellSchoolMask =
                SpellSchoolMask(1 << item->GetProto()->Damage[0].DamageType);
    }

    m_originalCasterGUID =
        originalCasterGUID ? originalCasterGUID : m_caster->GetObjectGuid();

    UpdateOriginalCasterPointer();

    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
        m_currentBasePoints[i] =
            m_spellInfo->CalculateSimpleValue(SpellEffectIndex(i));

    m_spellState = SPELL_STATE_PREPARING;

    m_castPositionX = m_castPositionY = m_castPositionZ = 0;
    m_TriggerSpells.clear();
    m_preCastSpells.clear();
    trigger_type_ = triggered;
    // m_AreaAura = false;
    m_CastItem = nullptr;

    unitTarget = nullptr;
    itemTarget = nullptr;
    gameObjTarget = nullptr;
    focusObject = nullptr;
    m_cast_count = 0;
    m_triggeredByAuraSpell = nullptr;

    // Auto Shot & Shoot (wand)
    m_autoRepeat = IsAutoRepeatRangedSpell(m_spellInfo);

    m_stealthedOnCast = false;
    m_scriptTarget = false;

    m_powerCost = 0; // setup to correct value in Spell::prepare, don't must be
                     // used before.
    m_casttime = 0;  // setup to correct value in Spell::prepare, don't must be
                     // used before.
    m_timer = 0;     // will set to cast time in prepare
    m_duration = 0;
    m_durationUnmod = 0;
    m_durationMax = 0;
    m_reagentsIgnoredDueToPrepare = false;

    health_leech_multiplier_ = 0.0f;

    m_needAliveTargetMask = 0;

    m_wasGrounded = false;

    // determine if we can reflect this spell (One extra check happens in
    // Spell::AddUnitTarget() as it's not checkable before we consider targets)
    m_canReflect = IsSpellReflectable(m_spellInfo, trigger_type_);

    m_reflected = false;

    m_instant = false;

    ignore_interrupt_ = false;

    m_canTrigger = false;
    m_procAttacker = 0;
    m_procVictim = 0;

    finish_ok_ = false;

    ElementalMasteryUsed = false;
    DivineFavorUsed = false;

    waiting_for_path = 0;
    path_gen_finished = false;

    send_cast_result_to_pet_owner_ = false;
    pet_cast_ = false;

    casted_timestamp_ = 0;

    CleanupTargetList();
}

Spell::~Spell()
{
}

void Spell::FillTargetMap()
{
    // TODO: ADD the correct target FILLS!!!!!!

    UnitList tmpUnitLists[MAX_EFFECT_INDEX]; // Stores the temporary Target
                                             // Lists for each effect
    uint8 effToIndex[MAX_EFFECT_INDEX] = {0, 1, 2}; // Helper array, to link to
                                                    // another tmpUnitList, if
                                                    // the targets for both
                                                    // effects match
    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        m_scriptTarget = false;

        // not call for empty effect.
        // Also some spells use not used effect targets for store targets for
        // dummy effect in triggered spells
        if (m_spellInfo->Effect[i] == SPELL_EFFECT_NONE)
            continue;

        // targets for TARGET_SCRIPT_COORDINATES (A) and TARGET_SCRIPT
        // for TARGET_FOCUS_OR_SCRIPTED_GAMEOBJECT (A) all is checked in
        // Spell::CheckCast and in Spell::CheckItem
        // filled in Spell::CheckCast call
        if (m_spellInfo->EffectImplicitTargetA[i] ==
                TARGET_SCRIPT_COORDINATES ||
            m_spellInfo->EffectImplicitTargetA[i] == TARGET_SCRIPT ||
            m_spellInfo->EffectImplicitTargetA[i] ==
                TARGET_FOCUS_OR_SCRIPTED_GAMEOBJECT ||
            (m_spellInfo->EffectImplicitTargetB[i] == TARGET_SCRIPT &&
                m_spellInfo->EffectImplicitTargetA[i] != TARGET_SELF))
            continue;

        // TODO: find a way so this is not needed?
        // for area auras always add caster as target (needed for totems for
        // example)
        if (IsAreaAuraEffect(m_spellInfo->Effect[i]))
            AddUnitTarget(m_caster, SpellEffectIndex(i));

        // no double fill for same targets
        for (int j = 0; j < i; ++j)
        {
            // Check if same target, but handle i.e. AreaAuras different
            if (m_spellInfo->EffectImplicitTargetA[i] ==
                    m_spellInfo->EffectImplicitTargetA[j] &&
                m_spellInfo->EffectImplicitTargetB[i] ==
                    m_spellInfo->EffectImplicitTargetB[j] &&
                m_spellInfo->Effect[j] != SPELL_EFFECT_NONE &&
                !IsAreaAuraEffect(m_spellInfo->Effect[i]) &&
                !IsAreaAuraEffect(m_spellInfo->Effect[j]))
            // Add further conditions here if required
            {
                effToIndex[i] =
                    j; // effect i has same targeting list as effect j
                break;
            }
        }

        if (effToIndex[i] == i) // New target combination
        {
            // TargetA/TargetB dependent from each other, we not switch to full
            // support this dependences
            // but need it support in some know cases
            switch (m_spellInfo->EffectImplicitTargetA[i])
            {
            case TARGET_NONE:
                switch (m_spellInfo->EffectImplicitTargetB[i])
                {
                case TARGET_NONE:
                    if (m_caster->GetObjectGuid().IsPet())
                        SetTargetMap(SpellEffectIndex(i), TARGET_SELF,
                            tmpUnitLists[i /*==effToIndex[i]*/]);
                    else
                        SetTargetMap(SpellEffectIndex(i), TARGET_EFFECT_SELECT,
                            tmpUnitLists[i /*==effToIndex[i]*/]);
                    break;
                default:
                    SetTargetMap(SpellEffectIndex(i),
                        m_spellInfo->EffectImplicitTargetB[i],
                        tmpUnitLists[i /*==effToIndex[i]*/]);
                    break;
                }
                break;
            case TARGET_SELF:
                switch (m_spellInfo->EffectImplicitTargetB[i])
                {
                case TARGET_NONE:
                case TARGET_EFFECT_SELECT:
                    SetTargetMap(SpellEffectIndex(i),
                        m_spellInfo->EffectImplicitTargetA[i],
                        tmpUnitLists[i /*==effToIndex[i]*/]);
                    break;
                case TARGET_AREAEFFECT_INSTANT: // use B case that not dependent
                                                // from from A in fact
                    if ((m_targets.m_targetMask & TARGET_FLAG_DEST_LOCATION) ==
                        0)
                        m_targets.setDestination(m_caster->GetX(),
                            m_caster->GetY(), m_caster->GetZ());
                    SetTargetMap(SpellEffectIndex(i),
                        m_spellInfo->EffectImplicitTargetB[i],
                        tmpUnitLists[i /*==effToIndex[i]*/]);
                    break;
                case TARGET_BEHIND_VICTIM: // use B case that not dependent from
                                           // from A in fact
                    SetTargetMap(SpellEffectIndex(i),
                        m_spellInfo->EffectImplicitTargetB[i],
                        tmpUnitLists[i /*==effToIndex[i]*/]);
                    break;
                case TARGET_CURRENT_ENEMY_COORDINATES:
                    SetTargetMap(SpellEffectIndex(i),
                        m_spellInfo->EffectImplicitTargetB[i],
                        tmpUnitLists[i /*==effToIndex[i]*/]);
                    tmpUnitLists[i].clear();
                    tmpUnitLists[i].push_back(m_caster);
                    break;
                default:
                    SetTargetMap(SpellEffectIndex(i),
                        m_spellInfo->EffectImplicitTargetA[i],
                        tmpUnitLists[i /*==effToIndex[i]*/]);
                    SetTargetMap(SpellEffectIndex(i),
                        m_spellInfo->EffectImplicitTargetB[i],
                        tmpUnitLists[i /*==effToIndex[i]*/]);
                    break;
                }
                break;
            case TARGET_EFFECT_SELECT:
                switch (m_spellInfo->EffectImplicitTargetB[i])
                {
                case TARGET_NONE:
                case TARGET_EFFECT_SELECT:
                    SetTargetMap(SpellEffectIndex(i),
                        m_spellInfo->EffectImplicitTargetA[i],
                        tmpUnitLists[i /*==effToIndex[i]*/]);
                    break;
                // dest point setup required
                case TARGET_AREAEFFECT_INSTANT:
                case TARGET_AREAEFFECT_CUSTOM:
                case TARGET_ALL_ENEMY_IN_AREA:
                case TARGET_ALL_ENEMY_IN_AREA_INSTANT:
                case TARGET_ALL_ENEMY_IN_AREA_CHANNELED:
                case TARGET_ALL_FRIENDLY_UNITS_IN_AREA:
                case TARGET_AREAEFFECT_GO_AROUND_DEST:
                case TARGET_RANDOM_NEARBY_DEST:
                    // triggered spells get dest point from default target set,
                    // ignore it
                    if (!(m_targets.m_targetMask & TARGET_FLAG_DEST_LOCATION) ||
                        trigger_type_.triggered())
                        if (WorldObject* castObject = GetCastingObject())
                            m_targets.setDestination(castObject->GetX(),
                                castObject->GetY(), castObject->GetZ());
                    SetTargetMap(SpellEffectIndex(i),
                        m_spellInfo->EffectImplicitTargetB[i],
                        tmpUnitLists[i /*==effToIndex[i]*/]);
                    break;
                // target pre-selection required
                case TARGET_INNKEEPER_COORDINATES:
                case TARGET_TABLE_X_Y_Z_COORDINATES:
                case TARGET_CASTER_COORDINATES:
                case TARGET_SCRIPT_COORDINATES:
                case TARGET_CURRENT_ENEMY_COORDINATES:
                case TARGET_DUELVSPLAYER_COORDINATES:
                case TARGET_DYNAMIC_OBJECT_COORDINATES:
                case TARGET_POINT_AT_NORTH:
                case TARGET_POINT_AT_SOUTH:
                case TARGET_POINT_AT_EAST:
                case TARGET_POINT_AT_WEST:
                case TARGET_POINT_AT_NE:
                case TARGET_POINT_AT_NW:
                case TARGET_POINT_AT_SE:
                case TARGET_POINT_AT_SW:
                    // need some target for processing
                    SetTargetMap(SpellEffectIndex(i),
                        m_spellInfo->EffectImplicitTargetA[i],
                        tmpUnitLists[i /*==effToIndex[i]*/]);
                    SetTargetMap(SpellEffectIndex(i),
                        m_spellInfo->EffectImplicitTargetB[i],
                        tmpUnitLists[i /*==effToIndex[i]*/]);
                    break;
                default:
                    SetTargetMap(SpellEffectIndex(i),
                        m_spellInfo->EffectImplicitTargetB[i],
                        tmpUnitLists[i /*==effToIndex[i]*/]);
                    break;
                }
                break;
            case TARGET_CASTER_COORDINATES:
                switch (m_spellInfo->EffectImplicitTargetB[i])
                {
                case TARGET_ALL_ENEMY_IN_AREA:
                    // Note: this hack with search required until GO casting not
                    // implemented
                    // environment damage spells already have around enemies
                    // targeting but this not help in case nonexistent GO
                    // casting support
                    // currently each enemy selected explicitly and self cast
                    // damage
                    if (m_spellInfo->Effect[i] ==
                        SPELL_EFFECT_ENVIRONMENTAL_DAMAGE)
                    {
                        if (m_targets.getUnitTarget())
                            tmpUnitLists[i /*==effToIndex[i]*/].push_back(
                                m_targets.getUnitTarget());
                    }
                    else
                    {
                        SetTargetMap(SpellEffectIndex(i),
                            m_spellInfo->EffectImplicitTargetA[i],
                            tmpUnitLists[i /*==effToIndex[i]*/]);
                        SetTargetMap(SpellEffectIndex(i),
                            m_spellInfo->EffectImplicitTargetB[i],
                            tmpUnitLists[i /*==effToIndex[i]*/]);
                    }
                    break;
                case TARGET_NONE:
                    SetTargetMap(SpellEffectIndex(i),
                        m_spellInfo->EffectImplicitTargetA[i],
                        tmpUnitLists[i /*==effToIndex[i]*/]);
                    tmpUnitLists[i /*==effToIndex[i]*/].push_back(m_caster);
                    break;
                default:
                    SetTargetMap(SpellEffectIndex(i),
                        m_spellInfo->EffectImplicitTargetA[i],
                        tmpUnitLists[i /*==effToIndex[i]*/]);
                    SetTargetMap(SpellEffectIndex(i),
                        m_spellInfo->EffectImplicitTargetB[i],
                        tmpUnitLists[i /*==effToIndex[i]*/]);
                    break;
                }
                break;
            case TARGET_TABLE_X_Y_Z_COORDINATES:
                switch (m_spellInfo->EffectImplicitTargetB[i])
                {
                case TARGET_NONE:
                    SetTargetMap(SpellEffectIndex(i),
                        m_spellInfo->EffectImplicitTargetA[i],
                        tmpUnitLists[i /*==effToIndex[i]*/]);

                    // need some target for processing
                    SetTargetMap(SpellEffectIndex(i), TARGET_EFFECT_SELECT,
                        tmpUnitLists[i /*==effToIndex[i]*/]);
                    break;
                case TARGET_AREAEFFECT_INSTANT: // All 17/7 pairs used for dest
                                                // teleportation, A processed in
                                                // effect code
                    SetTargetMap(SpellEffectIndex(i),
                        m_spellInfo->EffectImplicitTargetB[i],
                        tmpUnitLists[i /*==effToIndex[i]*/]);
                    break;
                default:
                    SetTargetMap(SpellEffectIndex(i),
                        m_spellInfo->EffectImplicitTargetA[i],
                        tmpUnitLists[i /*==effToIndex[i]*/]);
                    SetTargetMap(SpellEffectIndex(i),
                        m_spellInfo->EffectImplicitTargetB[i],
                        tmpUnitLists[i /*==effToIndex[i]*/]);
                    break;
                }
                break;
            case TARGET_SELF2:
                switch (m_spellInfo->EffectImplicitTargetB[i])
                {
                case TARGET_NONE:
                case TARGET_EFFECT_SELECT:
                    SetTargetMap(SpellEffectIndex(i),
                        m_spellInfo->EffectImplicitTargetA[i],
                        tmpUnitLists[i /*==effToIndex[i]*/]);
                    break;
                // most A/B target pairs is self->negative and not expect adding
                // caster to target list
                default:
                    SetTargetMap(SpellEffectIndex(i),
                        m_spellInfo->EffectImplicitTargetB[i],
                        tmpUnitLists[i /*==effToIndex[i]*/]);
                    break;
                }
                break;
            case TARGET_DUELVSPLAYER_COORDINATES:
                switch (m_spellInfo->EffectImplicitTargetB[i])
                {
                case TARGET_NONE:
                case TARGET_EFFECT_SELECT:
                    SetTargetMap(SpellEffectIndex(i),
                        m_spellInfo->EffectImplicitTargetA[i],
                        tmpUnitLists[i /*==effToIndex[i]*/]);
                    if (Unit* currentTarget = m_targets.getUnitTarget())
                        tmpUnitLists[i /*==effToIndex[i]*/].push_back(
                            currentTarget);
                    break;
                default:
                    SetTargetMap(SpellEffectIndex(i),
                        m_spellInfo->EffectImplicitTargetA[i],
                        tmpUnitLists[i /*==effToIndex[i]*/]);
                    SetTargetMap(SpellEffectIndex(i),
                        m_spellInfo->EffectImplicitTargetB[i],
                        tmpUnitLists[i /*==effToIndex[i]*/]);
                    break;
                }
                break;
            default:
                switch (m_spellInfo->EffectImplicitTargetB[i])
                {
                case TARGET_NONE:
                case TARGET_EFFECT_SELECT:
                    SetTargetMap(SpellEffectIndex(i),
                        m_spellInfo->EffectImplicitTargetA[i],
                        tmpUnitLists[i /*==effToIndex[i]*/]);
                    break;
                case TARGET_SCRIPT_COORDINATES: // B case filled in CheckCast
                                                // but we need fill unit list
                                                // base at A case
                    SetTargetMap(SpellEffectIndex(i),
                        m_spellInfo->EffectImplicitTargetA[i],
                        tmpUnitLists[i /*==effToIndex[i]*/]);
                    break;
                default:
                    SetTargetMap(SpellEffectIndex(i),
                        m_spellInfo->EffectImplicitTargetA[i],
                        tmpUnitLists[i /*==effToIndex[i]*/]);
                    SetTargetMap(SpellEffectIndex(i),
                        m_spellInfo->EffectImplicitTargetB[i],
                        tmpUnitLists[i /*==effToIndex[i]*/]);
                    break;
                }
                break;
            }
        }

        if (m_caster->GetTypeId() == TYPEID_PLAYER)
        {
            Player* me = (Player*)m_caster;
            for (UnitList::const_iterator itr =
                     tmpUnitLists[effToIndex[i]].begin();
                 itr != tmpUnitLists[effToIndex[i]].end(); ++itr)
            {
                Player* targetOwner =
                    (*itr)->GetCharmerOrOwnerPlayerOrPlayerItself();
                if (targetOwner && targetOwner != me && targetOwner->IsPvP() &&
                    !me->IsInDuelWith(targetOwner))
                {
                    me->UpdatePvP(true);
                    me->remove_auras_if([](AuraHolder* h)
                        {
                            return h->GetSpellProto()->AuraInterruptFlags &
                                   AURA_INTERRUPT_FLAG_ENTER_PVP_COMBAT;
                        });
                    break;
                }
            }
        }

        for (auto itr = tmpUnitLists[effToIndex[i]].begin();
             itr != tmpUnitLists[effToIndex[i]].end();)
        {
            if (!CheckTarget(*itr, SpellEffectIndex(i)))
            {
                itr = tmpUnitLists[effToIndex[i]].erase(itr);
                continue;
            }
            else
                ++itr;
        }

        for (UnitList::const_iterator iunit =
                 tmpUnitLists[effToIndex[i]].begin();
             iunit != tmpUnitLists[effToIndex[i]].end(); ++iunit)
            AddUnitTarget((*iunit), SpellEffectIndex(i));
    }

    // Remove all targets but the main target if our chain spell was reflected
    if (m_reflected && IsChainTargetSpell(m_spellInfo))
    {
        ObjectGuid main_target = m_targets.getUnitTargetGuid();
        for (auto itr = m_UniqueTargetInfo.begin();
             itr != m_UniqueTargetInfo.end();)
        {
            if (itr->targetGUID == main_target)
                ++itr;
            else
                itr = m_UniqueTargetInfo.erase(itr);
        }
    }
}

void Spell::prepareDataForTriggerSystem()
{
    //==========================================================================================
    // Now fill data for trigger system, need know:
    // an spell trigger another or not ( m_canTrigger )
    // Create base triggers flags for Attacker and Victim ( m_procAttacker and
    // m_procVictim)
    //==========================================================================================
    // Fill flag can spell trigger or not
    // TODO: possible exist spell attribute for this
    m_canTrigger = false;

    if (m_CastItem)
        m_canTrigger = false; // Do not trigger from item cast spell
    else if (trigger_type_.can_trigger_procs())
        m_canTrigger = true; // Normal cast - can trigger
    else if (m_triggeredBySpellInfo &&
             m_triggeredBySpellInfo->HasEffect(SPELL_EFFECT_TRIGGER_SPELL))
        m_canTrigger =
            true; // Triggered from SPELL_EFFECT_TRIGGER_SPELL - can trigger
    else if (m_spellInfo->HasAttribute(SPELL_ATTR_EX2_AUTOREPEAT_FLAG))
        m_canTrigger =
            true; // Do triggers from auto-repeat spells (auto shot/wand)

    // spell has defined spell proc exceptions? then it can always trigger
    if (!m_canTrigger &&
        sSpellMgr::Instance()->HasSpellProcExceptions(m_spellInfo->Id))
        m_canTrigger = true;

    if (m_spellInfo->HasAttribute(SPELL_ATTR_CUSTOM_CANNOT_TRIGGER_PROCS))
        m_canTrigger = false;

    // Trigger exceptions
    switch (m_spellInfo->SpellFamilyName)
    {
    case SPELLFAMILY_WARRIOR:
        // Execute
        if (m_spellInfo->SpellFamilyFlags & 0x20000000)
            m_canTrigger = true;
        break;
    case SPELLFAMILY_MAGE:
        // Arcane Missiles / Blizzard triggers need do it
        if (m_spellInfo->IsFitToFamilyMask(UI64LIT(0x0000000000200080)))
            m_canTrigger = true;
        break;
    case SPELLFAMILY_WARLOCK:
        // For Hellfire Effect / Rain of Fire / Seed of Corruption triggers need
        // do it
        if (m_spellInfo->IsFitToFamilyMask(UI64LIT(0x0000800000000060)))
            m_canTrigger = true;
        break;
    case SPELLFAMILY_ROGUE:
        // Shiv needs to trigger for the actual spell that does the damage,
        // and not the dummy component, to be able to proc combat potency
        if (m_spellInfo->Id == 5938)
            m_canTrigger = false;
        else if (m_spellInfo->Id == 5940)
            m_canTrigger = true;
        // Instant Poison can trigger (as shown by the fact it consumed
        // Stormstrike stacks)
        if (m_spellInfo->SpellFamilyFlags & 0x2000 &&
            m_spellInfo->Effect[0] == SPELL_EFFECT_SCHOOL_DAMAGE)
            m_canTrigger = true;
        else
            break;
    case SPELLFAMILY_HUNTER:
        // Hunter Explosive Trap Effect/Immolation Trap Effect/Frost Trap
        // Aura/Snake Trap Effect
        if (m_spellInfo->IsFitToFamilyMask(UI64LIT(0x0000200000000014)))
            m_canTrigger = true;
        // Volley
        if (m_spellInfo->SpellFamilyFlags & 0x2000)
            m_canTrigger = true;
        break;
    case SPELLFAMILY_PALADIN:
        // For Holy Shock triggers need do it
        if (m_spellInfo->IsFitToFamilyMask(UI64LIT(0x0001000000200000)))
            m_canTrigger = true;
        // Seal of Command (TODO: There probably exists more general rules)
        // Seal of Blood
        if (m_spellInfo->SpellFamilyFlags & (0x2000000 | 0x40000000000))
            m_canTrigger = true;
        break;
    case SPELLFAMILY_SHAMAN:
        // Windfury Weapon can trigger Flurry
        if (m_spellInfo->IsFitToFamilyMask(UI64LIT(0x0000000000800000)))
            m_canTrigger = true;
        break;
    default:
        break;
    }

    switch (m_spellInfo->Id)
    {
    case 67:
    case 26017:
    case 26018:
        // Vindication can trigger stuff, probably done as an exception by
        // blizzard to not make the talent useless against bosses, whom are
        // immune to it.
        m_canTrigger = true;
        break;
    }

    // Get data for type of attack and fill base info for trigger
    switch (m_spellInfo->DmgClass)
    {
    case SPELL_DAMAGE_CLASS_MELEE:
        m_procAttacker = PROC_FLAG_SUCCESSFUL_MELEE_SPELL_HIT;
        if (m_attackType == OFF_ATTACK)
            m_procAttacker |= PROC_FLAG_SUCCESSFUL_OFFHAND_HIT;
        m_procVictim = PROC_FLAG_TAKEN_MELEE_SPELL_HIT;
        break;
    case SPELL_DAMAGE_CLASS_RANGED:
        // Auto attack
        if (m_spellInfo->HasAttribute(SPELL_ATTR_EX2_AUTOREPEAT_FLAG))
        {
            m_procAttacker = PROC_FLAG_SUCCESSFUL_RANGED_HIT;
            m_procVictim = PROC_FLAG_TAKEN_RANGED_HIT;
        }
        else // Ranged spell attack
        {
            m_procAttacker = PROC_FLAG_SUCCESSFUL_RANGED_SPELL_HIT;
            m_procVictim = PROC_FLAG_TAKEN_RANGED_SPELL_HIT;
        }
        break;
    default:
        if (IsPositiveSpell(m_spellInfo->Id)) // Check for positive spell
        {
            m_procAttacker = PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL;
            m_procVictim = PROC_FLAG_TAKEN_POSITIVE_SPELL;
        }
        else if (m_spellInfo->HasAttribute(
                     SPELL_ATTR_EX2_AUTOREPEAT_FLAG)) // Wands auto attack
        {
            m_procAttacker = PROC_FLAG_SUCCESSFUL_RANGED_HIT;
            m_procVictim = PROC_FLAG_TAKEN_RANGED_HIT;
        }
        else // Negative spell
        {
            m_procAttacker = PROC_FLAG_SUCCESSFUL_NEGATIVE_SPELL_HIT;
            m_procVictim = PROC_FLAG_TAKEN_NEGATIVE_SPELL_HIT;
        }
        break;
    }

    // some negative spells have positive effects to another or same targets
    // avoid triggering negative hit for only positive targets
    m_negativeEffectMask = 0x0;
    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
        if (!IsPositiveEffect(m_spellInfo, SpellEffectIndex(i)))
            m_negativeEffectMask |= (1 << i);

    // Hunter traps spells (for Entrapment trigger)
    // Gives your Immolation Trap, Frost Trap, Explosive Trap, and Snake Trap
    // ....
    if (m_spellInfo->SpellFamilyName == SPELLFAMILY_HUNTER &&
        m_spellInfo->SpellFamilyFlags & UI64LIT(0x000020000000001C))
        m_procAttacker |= PROC_FLAG_ON_TRAP_ACTIVATION;
}

void Spell::CleanupTargetList()
{
    m_UniqueTargetInfo.clear();
    m_UniqueGOTargetInfo.clear();
    m_UniqueItemInfo.clear();
    m_delayMoment = 0;
}

void Spell::AddUnitTarget(Unit* pVictim, SpellEffectIndex effIndex)
{
    if (m_spellInfo->Effect[effIndex] == 0)
        return;

    ObjectGuid targetGUID = pVictim->GetObjectGuid();

    // Lookup target in already in list
    for (auto& elem : m_UniqueTargetInfo)
    {
        if (targetGUID == elem.targetGUID) // Found in list
        {
            elem.effectMask |= 1 << effIndex; // Add only effect mask
            return;
        }
    }

    bool can_reflect = m_canReflect;

    // Last check for m_canReflect. We need to do this one after SetTargetMap()
    // has been called.
    // This one removes reflectability if you're not the main target of a chain
    // spell (e.g. chain lightning)
    if (IsChainTargetSpell(m_spellInfo) &&
        m_targets.getUnitTargetGuid() != pVictim->GetObjectGuid())
        can_reflect = false;

    // This is new target calculate data for him

    Unit* caster = GetAffectiveCaster();
    if (!caster)
        caster = m_caster;

    // Get spell hit result on target
    TargetInfo target;
    target.damage = 0;
    target.targetGUID = targetGUID;    // Store target GUID
    target.effectMask = 1 << effIndex; // Store index of effect
    target.processed = false;          // Effects not apply on target
    target.friendly =
        caster->IsFriendlyTo(pVictim); // Store friendly status at start of cast
    for (int i = 0; i < MAX_EFFECT_INDEX; ++i) // Damage multipliers per effect
        target.damageMultipliers[i] = 1.0f;

    // Remember if target had vanish on him at cast
    target.vanished_on_cast = false;
    auto& auras = pVictim->GetAurasByType(SPELL_AURA_MOD_STEALTH);
    for (auto& aura : auras)
        if (aura->GetSpellProto()->SpellIconID == 252)
            target.vanished_on_cast = true;

    // Partial resistance amount, if full resistance, the spell simply misses on
    // the target
    auto schoolMask = GetSpellSchoolMask(m_spellInfo);
    if (IsPartiallyResistable(m_spellInfo) &&
        (schoolMask & SPELL_SCHOOL_MASK_NORMAL) == 0)
        target.resist =
            pVictim->calculate_partial_resistance(m_caster, schoolMask);
    else
        target.resist = 0.0f; // 100% of damage goes through

    // Calculate hit result
    if (target.resist >= 1.0f)
        target.missCondition = SPELL_MISS_RESIST;
    else
        target.missCondition =
            m_caster->SpellHitResult(pVictim, m_spellInfo, can_reflect, false);

    // HACK: Wrath of the Astromancer should not be fully resistable
    if (m_spellInfo->Id == 33045 && target.missCondition == SPELL_MISS_RESIST)
    {
        target.missCondition = SPELL_MISS_NONE;
        target.resist = 0.75f;
    }

    // Spell have speed - need calculate incoming time
    if (m_spellInfo->speed > 0.0f && pVictim != m_caster)
    {
        // calculate spell incoming interval
        float dist = m_caster->GetDistance(
            pVictim->GetX(), pVictim->GetY(), pVictim->GetZ());
        if (dist < 5.0f)
            dist = 5.0f;
        target.timeDelay = (uint64)floor(dist / m_spellInfo->speed * 1000.0f);

        // Calculate minimum incoming time
        if (m_delayMoment == 0 || m_delayMoment > target.timeDelay)
            m_delayMoment = target.timeDelay;
    }
    else
        target.timeDelay = 0;

    // If target reflect spell back to caster
    if (target.missCondition == SPELL_MISS_REFLECT)
    {
        m_reflected = true;

        // Calculate reflected spell result on caster
        target.reflectResult =
            m_caster->SpellHitResult(m_caster, m_spellInfo, m_canReflect, true);

        if (target.reflectResult == SPELL_MISS_REFLECT) // Impossible reflect
                                                        // again, so simply
                                                        // deflect spell
            target.reflectResult = SPELL_MISS_PARRY;
    }
    else
        target.reflectResult = SPELL_MISS_NONE;

    // Mind vision is a bitch and it requires 500 different exceptions all over
    // the code
    switch (m_spellInfo->Id)
    {
    case 2096:
    case 10909:
        if (m_caster->IsFriendlyTo(pVictim))
            target.missCondition = SPELL_MISS_NONE;
        break;
    }

    // Add target to list
    m_UniqueTargetInfo.push_back(target);
}

void Spell::AddUnitTarget(ObjectGuid unitGuid, SpellEffectIndex effIndex)
{
    if (Unit* unit = m_caster->GetObjectGuid() == unitGuid ?
                         m_caster :
                         ObjectAccessor::GetUnit(*m_caster, unitGuid))
        AddUnitTarget(unit, effIndex);
}

void Spell::AddGOTarget(GameObject* pVictim, SpellEffectIndex effIndex)
{
    if (m_spellInfo->Effect[effIndex] == 0)
        return;

    if (m_spellInfo->MaxAffectedTargets != 0 &&
        m_UniqueGOTargetInfo.size() >= m_spellInfo->MaxAffectedTargets)
        return;

    ObjectGuid targetGUID = pVictim->GetObjectGuid();

    // Lookup target in already in list
    for (auto& elem : m_UniqueGOTargetInfo)
    {
        if (targetGUID == elem.targetGUID) // Found in list
        {
            elem.effectMask |= (1 << effIndex); // Add only effect mask
            return;
        }
    }

    // This is new target calculate data for him

    GOTargetInfo target;
    target.targetGUID = targetGUID;
    target.effectMask = (1 << effIndex);
    target.processed = false; // Effects not apply on target

    // spell fly from visual cast object
    WorldObject* affectiveObject = GetAffectiveCasterObject();

    // Spell have speed - need calculate incoming time
    if (m_spellInfo->speed > 0.0f && affectiveObject &&
        pVictim != affectiveObject)
    {
        // calculate spell incoming interval
        float dist = affectiveObject->GetDistance(
            pVictim->GetX(), pVictim->GetY(), pVictim->GetZ());
        if (dist < 5.0f)
            dist = 5.0f;
        target.timeDelay = (uint64)floor(dist / m_spellInfo->speed * 1000.0f);
        if (m_delayMoment == 0 || m_delayMoment > target.timeDelay)
            m_delayMoment = target.timeDelay;
    }
    else
        target.timeDelay = UI64LIT(0);

    // Add target to list
    m_UniqueGOTargetInfo.push_back(target);
}

void Spell::AddGOTarget(ObjectGuid goGuid, SpellEffectIndex effIndex)
{
    if (GameObject* go = m_caster->GetMap()->GetGameObject(goGuid))
        AddGOTarget(go, effIndex);
}

void Spell::AddItemTarget(Item* pitem, SpellEffectIndex effIndex)
{
    if (m_spellInfo->Effect[effIndex] == 0)
        return;

    // Lookup target in already in list
    for (auto& elem : m_UniqueItemInfo)
    {
        if (pitem == elem.item) // Found in list
        {
            elem.effectMask |= (1 << effIndex); // Add only effect mask
            return;
        }
    }

    // This is new target add data

    ItemTargetInfo target;
    target.item = pitem;
    target.effectMask = (1 << effIndex);
    m_UniqueItemInfo.push_back(target);
}

void Spell::DoAllEffectOnTarget(Unit* target)
{
    for (auto& elem : m_UniqueTargetInfo)
    {
        if (elem.targetGUID == target->GetObjectGuid())
        {
            DoAllEffectOnTarget(&elem);
            break;
        }
    }
}

void Spell::DoUnitEffect(TargetInfo* target)
{
    Unit* unit = m_caster->GetObjectGuid() == target->targetGUID ?
                     m_caster :
                     ObjectAccessor::GetUnit(*m_caster, target->targetGUID);
    if (!unit)
        return;

    // For delayed spells, becoming stealthed during travel time means the spell
    // will just be silently ignored without doing any effect; this is untrue if
    // target was stealthed on cast
    if (m_spellInfo->speed > 0 && target->targetGUID != m_casterGUID &&
        !target->vanished_on_cast &&
        !unit->can_be_hit_by_delayed_spell_stealth_check(m_caster))
    {
        target->missCondition = SPELL_MISS_MISS;
        return;
    }

    if (DoImmunePowerException(target))
        return; // spell has already finished

    // HACK: Divine favor, see note in Spell.h
    // Affects: Holy Light, Flash of Light and Holy Shock
    if (m_spellInfo->SpellFamilyName == SPELLFAMILY_PALADIN &&
        m_spellInfo->Effect[0] != SPELL_EFFECT_DUMMY &&
        (m_spellInfo->SpellFamilyFlags &
            (0x80000000 | 0x40000000 | 0x200000 | 0x1000000000000)))
    {
        if (m_caster->has_aura(20216, SPELL_AURA_DUMMY))
        {
            m_caster->remove_auras(20216);
            DivineFavorUsed = true;
        }
    }

    if (target->missCondition == SPELL_MISS_NONE)
    {
        // Spells that are instant: those on yourself, auto-repeat spells, and
        // spells you cast in stealth that will break the stealth (excl. player
        // targeted spells)
        if (!m_spellInfo->HasAttribute(SPELL_ATTR_CUSTOM_FORCE_SPELL_QUEUE) &&
            (m_spellInfo->HasAttribute(SPELL_ATTR_CUSTOM_NO_SPELL_QUEUE) ||
                unit == m_caster || m_autoRepeat ||
                (unit->GetTypeId() != TYPEID_PLAYER && m_stealthedOnCast &&
                    !m_spellInfo->HasAttribute(
                        SPELL_ATTR_EX_NOT_BREAK_STEALTH)) ||
                m_spellInfo->HasEffect(SPELL_EFFECT_RESURRECT_NEW) ||
                trigger_type_.bypasses_spell_queue() ||
                IsChanneledSpell(m_spellInfo) ||
                m_spellInfo->HasEffect(SPELL_EFFECT_INTERRUPT_CAST)))
        {
            DoAllEffectOnTarget(target);
        }
        else
        {
            unit->queue_spell_hit(this);
        }
    }
    else
    {
        DoImmediateEffectsOnTarget(target);
    }
}

void Spell::DoUnitEffects()
{
    for (auto& elem : m_UniqueTargetInfo)
        DoUnitEffect(&elem);
}

void Spell::DoMissThreat(Unit* target, Unit* real_caster)
{
    if (target == real_caster || target->isDead() || real_caster->isDead())
        return;

    // causes miss threat:
    if (
        // IF:
        // a) negative spell, visible target and no special attributes
        (!m_spellInfo->HasAttribute(SPELL_ATTR_EX3_NO_INITIAL_AGGRO) &&
            !m_spellInfo->HasAttribute(SPELL_ATTR_EX_NO_THREAT) &&
            !m_spellInfo->HasAttribute(SPELL_ATTR_EX3_DONT_AFFECT_COMBAT) &&
            !IsPositiveSpell(m_spellInfo->Id) &&
            real_caster->can_be_seen_by(target, target)) ||
        // OR:
        // Mind Vision fails on NPC
        (!target->player_or_pet() &&
            m_spellInfo->HasApplyAuraName(SPELL_AURA_MOD_DETECT_RANGE)) ||
        // OR:
        // pickpocket fails
        m_spellInfo->HasEffect(SPELL_EFFECT_PICKPOCKET))
    {
        if (!target->isInCombat())
            target->AttackedBy(real_caster);

        target->AddThreat(real_caster);
        target->SetInCombatWith(real_caster);
        real_caster->SetInCombatWith(target);
    }
}

void Spell::DoImmediateEffectsOnTarget(TargetInfo* target)
{
    if (target->processed)
        return;

    Unit* unit = m_caster->GetObjectGuid() == target->targetGUID ?
                     m_caster :
                     ObjectAccessor::GetUnit(*m_caster, target->targetGUID);
    if (!unit)
        return;

    Unit* real_caster = GetAffectiveCaster();
    Unit* caster = m_caster;

    // Handle Spell Reflects
    if (target->missCondition == SPELL_MISS_REFLECT)
    {
        if (m_canReflect)
        {
            // Reflect spells that should be removed seem to all have the
            // PROC_FLAG_TAKEN_NEGATIVE_SPELL_HIT proc flag. Manually remove
            // them here.
            unit->remove_auras(SPELL_AURA_REFLECT_SPELLS, [](AuraHolder* holder)
                {
                    return holder->GetSpellProto()->procFlags &
                           PROC_FLAG_TAKEN_NEGATIVE_SPELL_HIT;
                });

            // Proc reflect (removes charges etc if needed)
            unit->ProcDamageAndSpell(caster, PROC_FLAG_NONE,
                PROC_FLAG_TAKEN_NEGATIVE_SPELL_HIT, PROC_EX_REFLECT,
                proc_amount(), BASE_ATTACK, m_spellInfo);

            m_canReflect = false;

            // If the spell has a travel-time we need to reevalute it once it's
            // returned to the caster
            if (m_spellInfo->speed > 0)
            {
                float dist = caster->GetDistance(unit);
                if (dist < 5.0f)
                    dist = 5.0f;
                target->timeDelay =
                    (uint64)(floor(dist / m_spellInfo->speed * 1000.0f) * 0.6f);
                return;
            }
        }

        DoAllEffectOnTarget(target);
    }
    // Handle spell getting resisted, parried, dodged, etc
    else if (target->missCondition != SPELL_MISS_NONE)
    {
        if (real_caster && real_caster != unit)
        {
            // NOTE: evaded spells cannot cause aggro
            if (target->missCondition != SPELL_MISS_EVADE)
                DoMissThreat(unit, real_caster);

            // Rogue's Setup for resisted spells
            if (target->missCondition == SPELL_MISS_RESIST &&
                unit->GetTypeId() == TYPEID_PLAYER &&
                unit->getClass() == CLASS_ROGUE)
                ((Player*)unit)->HandleRogueSetupTalent(real_caster);
        }
    }

    // Passive and active spell misses (only triggers if proc flags set)
    if ((m_procAttacker || m_procVictim) &&
        target->missCondition != SPELL_MISS_NONE)
    {
        // Do triggers for unit (reflect triggers passed on hit phase for
        // correct drop charge)
        if (m_canTrigger && target->missCondition != SPELL_MISS_REFLECT)
        {
            // Fill base damage struct
            SpellNonMeleeDamage damageInfo(
                caster, unit, m_spellInfo->Id, m_spellSchoolMask);
            uint32 procEx =
                createProcExtendMask(&damageInfo, target->missCondition);
            caster->ProcDamageAndSpell(unit,
                real_caster ? m_procAttacker :
                              static_cast<uint32>(PROC_FLAG_NONE),
                m_procVictim, procEx, proc_amount(), m_attackType, m_spellInfo);
        }
    }

    // Remove grounding totem if spell was groundend
    if (target->missCondition != SPELL_MISS_NONE && m_wasGrounded &&
        unit->GetTypeId() == TYPEID_UNIT && unit->GetEntry() == 5925 &&
        static_cast<Creature*>(unit)->IsTotem())
        ((Totem*)unit)->ForcedDespawn(400);
}

void Spell::DoAllEffectOnTarget(TargetInfo* target)
{
    if (target->processed) // Check target
        return;
    target->processed = true; // Target checked in apply effects procedure

    if (target->missCondition == SPELL_MISS_REFLECT && m_canReflect)
        return;

    Unit* unit = m_caster->GetObjectGuid() == target->targetGUID ?
                     m_caster :
                     ObjectAccessor::GetUnit(*m_caster, target->targetGUID);
    if (!unit)
        return;

    Unit* original_unit = unit; // saved in case of reflect

    // Get original caster (if exist) and calculate damage/healing from him data
    Unit* real_caster = GetAffectiveCaster();
    // FIXME: in case wild GO heal/damage spells will be used target bonuses
    Unit* caster = real_caster ? real_caster : m_caster;

    if (unit->GetTypeId() == TYPEID_UNIT && unit != caster &&
        static_cast<Creature*>(unit)->IsInEvadeMode())
    {
        m_caster->SendSpellMiss(unit, m_spellInfo->Id, SPELL_MISS_EVADE);
        return;
    }

    // caster simply becomes target on reflect
    if (target->missCondition == SPELL_MISS_REFLECT)
        unit = m_caster; // not real caster, but casting object

    // Save if unit is sitting before DoSpellHitOnUnit()
    bool sitting = !unit->IsStandState();

    // If the target died before processing, and the spell is not a spell that
    // works on dead targets, just ignore it
    if (unit->isDead() && !CanBeCastOnDeadTargets(m_spellInfo))
    {
        target->missCondition = SPELL_MISS_MISS;
        return;
    }

    // Ignore some reflected spells
    if (target->missCondition == SPELL_MISS_REFLECT &&
        IsSpellReflectIgnored(m_spellInfo))
        return;

    // Recheck that our target is still hostile (in case of a duel that ended,
    // or mob that changed faction while in travel orspell queue)
    if (caster != unit && (caster->GetTypeId() == TYPEID_PLAYER ||
                              caster->GetCharmerOrOwnerGuid()) &&
        target->friendly !=
            caster->IsFriendlyTo(unit) && // Only do the check if friendly
                                          // status has changed since
                                          // Spell::AddUnitTarget()
        target->missCondition == SPELL_MISS_NONE &&
        !CanSpellHitFriendlyTarget(m_spellInfo))
        return;

    // Remove grounding totem if spell was groundend (do before immunity check)
    if (m_wasGrounded && unit->GetTypeId() == TYPEID_UNIT &&
        unit->GetEntry() == 5925 && static_cast<Creature*>(unit)->IsTotem())
        ((Totem*)unit)->ForcedDespawn(400);

    // Check immunities for all spell effects about to hit us
    target->effectMask =
        unit->SpellImmunityCheck(m_spellInfo, target->effectMask);
    if (target->effectMask == 0)
    {
        target->missCondition = SPELL_MISS_IMMUNE;
        Unit* caster = GetAffectiveCaster();
        if (caster && !unit->isDead())
        {
            caster->SendSpellMiss(unit, m_spellInfo->Id, target->missCondition);
            DoMissThreat(unit, caster);
        }
        // do spell procs for immuning the spell
        if ((m_procAttacker || m_procVictim) && m_canTrigger)
        {
            // Fill base damage struct
            SpellNonMeleeDamage damageInfo(
                caster, unit, m_spellInfo->Id, m_spellSchoolMask);
            uint32 procEx =
                createProcExtendMask(&damageInfo, target->missCondition);
            caster->ProcDamageAndSpell(unit,
                real_caster ? m_procAttacker :
                              static_cast<uint32>(PROC_FLAG_NONE),
                m_procVictim, procEx, proc_amount(), m_attackType, m_spellInfo);
        }
        return;
    }

    // Update alive target mask for channeled spells if effectMask changed but
    // we intend to go through with the spell
    if (m_needAliveTargetMask != 0 &&
        target->effectMask != m_needAliveTargetMask)
        m_needAliveTargetMask = target->effectMask;

    uint32 mask = target->effectMask;
    SpellMissInfo missInfo = target->missCondition;
    unitTarget = unit;

    // Reset damage/healing counter
    ResetEffectDamageAndHeal();

    // Fill base trigger info
    uint32 procAttacker = m_procAttacker;
    uint32 procVictim = m_procVictim;
    uint32 procEx = PROC_EX_NONE;

    // drop proc flags in case target not affected negative effects in negative
    // spell
    // for example caster bonus or animation,
    // except miss case where will assigned PROC_EX_* flags later
    if (((procAttacker | procVictim) & NEGATIVE_TRIGGER_MASK) &&
        !(target->effectMask & m_negativeEffectMask) &&
        missInfo == SPELL_MISS_NONE)
    {
        procAttacker = PROC_FLAG_NONE;
        procVictim = PROC_FLAG_NONE;
    }

    // Damage calc for delayed spells handled in HandleDelayedSpellLaunch.
    // For other spells we do it right here.
    for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (mask & (1 << i) &&
            IsEffectPartOfDamageCalculation(m_spellInfo, SpellEffectIndex(i)))
        {
            if (m_spellInfo->speed == 0)
            {
                HandleEffects(
                    missInfo == SPELL_MISS_REFLECT ? original_unit : unit,
                    nullptr, nullptr, SpellEffectIndex(i));
            }
        }
    }
    m_damage += target->damage;

    // Fill damage structure before we do other effects
    SpellNonMeleeDamage damageInfo(
        caster, unit, m_spellInfo->Id, m_spellSchoolMask);
    bool blocked = false;
    if (m_damage)
    {
        if (m_spellInfo->speed > 0)
        {
            damageInfo.damage = m_damage;
            damageInfo.HitInfo = target->HitInfo;
        }
        else
        {
            caster->CalculateSpellDamage(&damageInfo, m_damage, m_spellInfo,
                m_attackType, m_UniqueTargetInfo.size(),
                (ElementalMasteryUsed || DivineFavorUsed) ? 100.0f : 0.0f,
                this);
            // Some talents that proc on crit do so when the spell is casted
            // Handled in Spell::cast for delayed spells
            if (m_canTrigger && !trigger_type_.triggered() &&
                damageInfo.HitInfo & SPELL_HIT_TYPE_CRIT)
            {
                caster->ProcDamageAndSpell(unit, m_procAttacker, 0,
                    PROC_EX_ON_CAST_CRIT, proc_amount(), BASE_ATTACK,
                    m_spellInfo);
            }
        }

        // Spell block
        damageInfo.blocked = unit->do_spell_block(
            caster, damageInfo.damage, m_spellInfo, BASE_ATTACK);

        if (damageInfo.blocked > 0 && damageInfo.damage == damageInfo.blocked)
            blocked = true;

        damageInfo.damage -= damageInfo.blocked;
        m_damage = damageInfo.damage;
    }
    else
    {
        // Some spells without damage components can still be blocked
        if (m_spellInfo->HasAttribute(SPELL_ATTR_CUSTOM_BLOCKABLE) &&
            unit->IsSpellBlocked(m_caster, m_spellInfo, m_attackType))
        {
            blocked = true;
            damageInfo.damage = 0;
            damageInfo.blocked = 1;
        }
    }

    // If spell was fully blocked, no part should hit
    if (blocked)
    {
        // Do on-block procs
        if (m_canTrigger)
            caster->ProcDamageAndSpell(unit, PROC_FLAG_SUCCESSFUL_RANGED_HIT,
                PROC_FLAG_TAKEN_RANGED_HIT, PROC_EX_BLOCK, proc_amount(),
                m_attackType, m_spellInfo);
        else // Do target procs even if we don't do caster procs
            caster->ProcDamageAndSpell(unit, 0, PROC_FLAG_TAKEN_RANGED_HIT,
                PROC_EX_BLOCK, proc_amount(), m_attackType, m_spellInfo);
        caster->SendSpellNonMeleeDamageLog(&damageInfo);
        return;
    }

    // Chance to Fizzle based on level
    if (m_spellInfo->HasAttribute(SPELL_ATTR_CUSTOM_FIZZLE_OVER_60) &&
        unit->getLevel() > 60)
    {
        if (roll_chance_i(4 * (unit->getLevel() - 60)))
        {
            SendCastResult(SPELL_FAILED_FIZZLE);
            ResetEffectDamageAndHeal();
            return;
        }
    }

    if (missInfo == SPELL_MISS_REFLECT &&
        target->reflectResult != SPELL_MISS_NONE)
    {
        // The actual combat log message is sent from with the intial GoTarget
        // package
        // We do this, however, to get the visual effect over the reflectees
        // head (example: Immune/Resist)
        caster->SendSpellMiss(unit, m_spellInfo->Id, target->reflectResult);
        ResetEffectDamageAndHeal();
        return;
    }

    // use reflect results as new hit result from here on
    if (missInfo == SPELL_MISS_REFLECT)
        missInfo = target->reflectResult;

    if (missInfo == SPELL_MISS_NONE) // In case spell hit target, do all effect
                                     // on that target
    {
        DoSpellHitOnUnit(
            unit, target, mask, target->missCondition == SPELL_MISS_REFLECT);

        // handle SPELL_AURA_ADD_TARGET_TRIGGER auras
        auto& targetTriggers =
            m_caster->GetAurasByType(SPELL_AURA_ADD_TARGET_TRIGGER);
        for (const auto& targetTrigger : targetTriggers)
        {
            if (!(targetTrigger)->isAffectedOnSpell(m_spellInfo))
                continue;

            SpellEntry const* auraSpellInfo = (targetTrigger)->GetSpellProto();
            SpellEffectIndex auraSpellIdx = (targetTrigger)->GetEffIndex();
            // Calculate chance at that moment (can be depend for example from
            // combo points)
            int32 auraBasePoints = (targetTrigger)->GetBasePoints();
            int32 chance = m_caster->CalculateSpellDamage(
                unit, auraSpellInfo, auraSpellIdx, &auraBasePoints);
            if (roll_chance_i(chance))
                m_caster->CastSpell(unit,
                    auraSpellInfo->EffectTriggerSpell[auraSpellIdx], true,
                    nullptr, (targetTrigger));
        }
    }

    // All calculated do it!
    // Do healing and triggers
    if (m_healing)
    {
        bool crit =
            real_caster &&
            real_caster->IsSpellCrit(unit, m_spellInfo, m_spellSchoolMask,
                BASE_ATTACK,
                (ElementalMasteryUsed || DivineFavorUsed) ? 100.0f : 0.0f,
                this);
        uint32 addhealth = m_healing;
        if (crit)
        {
            procEx |= PROC_EX_CRITICAL_HIT;
            addhealth = caster->SpellCriticalHealingBonus(
                m_spellInfo, addhealth, nullptr);
            // On cast crit procs
            if (m_canTrigger && !trigger_type_.triggered())
            {
                caster->ProcDamageAndSpell(unit, m_procAttacker, 0,
                    PROC_EX_ON_CAST_CRIT, proc_amount(), BASE_ATTACK,
                    m_spellInfo);
            }
        }
        else
            procEx |= PROC_EX_NORMAL_HIT;

        proc_amount procamnt(false, addhealth, unit);

        int32 gain = caster->DealHeal(unit, addhealth, m_spellInfo, crit);

        // Do triggers for unit
        if (m_canTrigger && missInfo != SPELL_MISS_REFLECT)
            caster->ProcDamageAndSpell(unit,
                real_caster ? procAttacker :
                              static_cast<uint32>(PROC_FLAG_NONE),
                procVictim, procEx, procamnt, m_attackType, m_spellInfo);

        // Threat is not applied to the real caster, but the caster who
        // triggered the effect
        unit->getHostileRefManager().threatAssist(m_caster,
            float(gain) * 0.5f *
                sSpellMgr::Instance()->GetSpellThreatMultiplier(m_spellInfo),
            m_spellInfo);
    }
    // Do damage and triggers
    else if (m_damage)
    {
        // m_damage can be updated by multipliers, etc
        damageInfo.damage = m_damage;
        uint32 school_mask = GetSpellSchoolMask(m_spellInfo);

        // Apply damage reduction (must happen after
        // Spell::DoSpellHitOnUnit, as
        // an effect might remove absorbs (like Arcane Shot), or scale
        // damage)
        // Partial resistance
        if (target->resist > 0)
        {
            damageInfo.resist =
                damageInfo.damage - (damageInfo.damage * target->resist);
            damageInfo.damage -= damageInfo.resist;
        }
        // Absorb
        auto absorb = damageInfo.target->do_absorb(
            caster, damageInfo.damage, school_mask, true);
        damageInfo.damage -= absorb;
        // Damage sharing
        auto share = damageInfo.target->do_damage_sharing(
            caster, damageInfo.damage, school_mask);
        damageInfo.damage -= share;
        // Death prevention
        auto prevented =
            damageInfo.target->do_death_prevention(damageInfo.damage);
        damageInfo.damage -= prevented;
        // Absorb, sharing and prevention all show up as absorb in the log
        damageInfo.absorb = absorb + share + prevented;

        caster->DealDamageMods(
            damageInfo.target, damageInfo.damage, &damageInfo.absorb);

        // Send log damage message to client
        caster->SendSpellNonMeleeDamageLog(&damageInfo);

        procEx = createProcExtendMask(&damageInfo, missInfo);
        procVictim |= PROC_FLAG_TAKEN_ANY_DAMAGE;

        // save duel state before we deal the damage (used by SW:D later down to
        // avoid rebuke damage on duel end)
        bool in_duel = m_caster->GetTypeId() == TYPEID_PLAYER ?
                           (static_cast<Player*>(m_caster)->duel &&
                               static_cast<Player*>(m_caster)->duel->opponent ==
                                   original_unit) :
                           false;

        proc_amount procamnt(true, damageInfo.damage, unit, damageInfo.absorb);

        caster->DealSpellDamage(&damageInfo, true);

        // Health Leech
        if (damageInfo.damage > 0 && health_leech_multiplier_ > 0 &&
            caster->isAlive())
        {
            uint32 heal = uint32(damageInfo.damage * health_leech_multiplier_);
            heal =
                caster->SpellHealingBonusTaken(caster, m_spellInfo, heal, HEAL);
            caster->DealHeal(caster, heal, m_spellInfo);
        }

        // Do triggers for unit
        // NOTE: for damage spells we always do victim procs, only caster procs
        // are blocked if m_canTrigger is false
        // Crits are considered normal hits (even though they deal critical
        // damage) for procs if we sit down
        uint32 proc_ex = procEx;
        if (sitting && proc_ex & PROC_EX_CRITICAL_HIT)
        {
            // Switch to a normal hit
            proc_ex &= ~PROC_EX_CRITICAL_HIT;
            proc_ex |= PROC_EX_NORMAL_HIT;
        }
        if (m_canTrigger) // See NOTE above
            caster->ProcDamageAndSpell(unit,
                real_caster ? procAttacker :
                              static_cast<uint32>(PROC_FLAG_NONE),
                procVictim, proc_ex, procamnt, m_attackType, m_spellInfo);
        else
            caster->ProcDamageAndSpell(unit, 0, procVictim, proc_ex, procamnt,
                m_attackType, m_spellInfo);
        // Note: we don't want to use the changed proc_ex here
        if (procEx & PROC_EX_NORMAL_HIT && unit->GetTypeId() == TYPEID_PLAYER)
            static_cast<Player*>(unit)->DoResilienceCritProc(m_caster,
                damageInfo.damage + damageInfo.absorb, m_attackType,
                m_spellInfo);

        // trigger weapon enchants for weapon based spells; exclude spells that
        // stop attack, because may break CC
        if (m_caster->GetTypeId() == TYPEID_PLAYER &&
            m_spellInfo->EquippedItemClass == ITEM_CLASS_WEAPON &&
            !m_spellInfo->HasAttribute(SPELL_ATTR_STOP_ATTACK_TARGET))
            ((Player*)m_caster)
                ->CastItemCombatSpell(
                    unit, m_attackType, EXTRA_ATTACK_NONE, 0, m_spellInfo);

        // Shadow Word: Death - deals damage equal to damage done by caster if
        // the target wasn't killed
        // (don't deal the damage if SW:D ended a duel against the target)
        if (m_spellInfo->SpellFamilyName == SPELLFAMILY_PRIEST &&
            m_spellInfo->SpellFamilyFlags & 0x0000000200000000LL &&
            unit->isAlive() &&
            !(in_duel && !static_cast<Player*>(m_caster)->duel)) // if in_duel
                                                                 // is true
                                                                 // m_caster is
                                                                 // guaranteed
                                                                 // to be a
                                                                 // player
        {
            int32 damagePoint = damageInfo.absorb;
            damagePoint = damagePoint + damageInfo.damage;
            m_caster->CastCustomSpell(
                m_caster, 32409, &damagePoint, nullptr, nullptr, true);
        }

        if (m_spellInfo->SpellFamilyName == SPELLFAMILY_PALADIN)
        {
            // Judgement of Blood
            if (m_spellInfo->SpellFamilyFlags & UI64LIT(0x0000000800000000) &&
                m_spellInfo->SpellIconID == 153)
            {
                int32 damagePoint = damageInfo.damage * 33 / 100;

                auto caster = m_caster;
                caster->queue_action(800, [damagePoint, in_duel, caster]
                    {
                        if (caster->GetTypeId() == TYPEID_PLAYER && in_duel &&
                            !static_cast<Player*>(caster)->duel)
                            return;
                        caster->CastCustomSpell(caster, 32220, &damagePoint,
                            nullptr, nullptr, true);
                    });
            }
            // Seal of Blood backfire damage
            else if (m_spellInfo->Id == 31893)
            {
                int32 damagePoint =
                    (damageInfo.damage + damageInfo.absorb) * 10 / 100;

                auto caster = m_caster;
                caster->queue_action(800, [damagePoint, in_duel, caster]
                    {
                        if (caster->GetTypeId() == TYPEID_PLAYER && in_duel &&
                            !static_cast<Player*>(caster)->duel)
                            return;
                        caster->CastCustomSpell(caster, 32221, &damagePoint,
                            nullptr, nullptr, true);
                    });
            }
        }
        // Bloodthirst
        else if (m_spellInfo->SpellFamilyName == SPELLFAMILY_WARRIOR &&
                 m_spellInfo->SpellFamilyFlags & UI64LIT(0x40000000000))
        {
            uint32 BTAura = 0;
            switch (m_spellInfo->Id)
            {
            case 23881:
                BTAura = 23885;
                break;
            case 23892:
                BTAura = 23886;
                break;
            case 23893:
                BTAura = 23887;
                break;
            case 23894:
                BTAura = 23888;
                break;
            case 25251:
                BTAura = 25252;
                break;
            case 30335:
                BTAura = 30339;
                break;
            default:
                logging.error(
                    "Spell::EffectSchoolDMG: Spell %u not handled in BTAura",
                    m_spellInfo->Id);
                break;
            }
            if (BTAura)
                m_caster->CastSpell(m_caster, BTAura, true);
        }
    }
    // Passive spell hits (only triggers if proc flags set)
    else if (procAttacker || procVictim)
    {
        // Do triggers for unit
        // Fill base damage struct
        SpellNonMeleeDamage damageInfo(
            caster, unit, m_spellInfo->Id, m_spellSchoolMask);
        procEx = createProcExtendMask(&damageInfo, missInfo);
        if (m_canTrigger)
        {
            // HACK: Force Vindication to be active
            proc_amount amnt(m_spellInfo->SpellIconID == 1822);
            caster->ProcDamageAndSpell(unit,
                real_caster ? procAttacker :
                              static_cast<uint32>(PROC_FLAG_NONE),
                procVictim, procEx, amnt, m_attackType, m_spellInfo);
        }
        else // Do target procs even if we don't do caster procs
        {
            caster->ProcDamageAndSpell(unit, 0, procVictim, procEx,
                proc_amount(), m_attackType, m_spellInfo);
        }
    }

    // Make target of hostile spells stand up. This used to be in
    // Spell::DoSpellHitOnUnit)
    // but we need the sit to still be in effect when the crit chance is
    // calculated
    if (missInfo == SPELL_MISS_NONE && !unit->IsStandState() &&
        !unit->hasUnitState(UNIT_STAT_STUNNED))
    {
        Unit* real = GetAffectiveCaster();
        if (real && real != unit && !IsPositiveSpell(m_spellInfo) &&
            !real->IsFriendlyTo(unit))
            unit->SetStandState(UNIT_STAND_STATE_STAND);
    }

    // Call scripted function for AI if this spell is casted upon a creature
    if (unit->GetTypeId() == TYPEID_UNIT)
    {
        // cast at creature (or GO) quest objectives update at successful cast
        // finished (+channel finished)
        // ignore pets or autorepeat/melee casts for speed (not exist quest for
        // spells (hm... )
        if (real_caster && !((Creature*)unit)->IsPet() && !IsAutoRepeat() &&
            !IsNextMeleeSwingSpell() && !IsChannelActive())
            if (Player* p =
                    real_caster->GetCharmerOrOwnerPlayerOrPlayerItself())
                p->RewardPlayerAndGroupAtCast(unit, m_spellInfo->Id);

        if (((Creature*)unit)->AI())
            ((Creature*)unit)->AI()->SpellHit(m_caster, m_spellInfo);
    }

    // Call scripted function for AI if this spell is casted by a creature
    if (m_caster->GetTypeId() == TYPEID_UNIT && ((Creature*)m_caster)->AI())
        ((Creature*)m_caster)->AI()->SpellHitTarget(unit, m_spellInfo);
    if (real_caster && real_caster != m_caster &&
        real_caster->GetTypeId() == TYPEID_UNIT &&
        ((Creature*)real_caster)->AI())
        ((Creature*)real_caster)->AI()->SpellHitTarget(unit, m_spellInfo);
}

void Spell::DoFinishPhase()
{
    // code below are only for spells that finished successfully
    if (!finish_ok_)
    {
        if (m_CastItem)
            m_CastItem->remove_referencing_spell(this);
        return;
    }

    TakeCastItem();

    // remove spell mods for successful spells (every other case handled in
    // Spell::finish)
    if (Player* modOwner = m_caster->GetSpellModOwner())
        modOwner->RemoveSpellMods(this);

    DropComboPointsIfNeeded();

    // call triggered spell only at successful cast (after clear combo points ->
    // for add some if need)
    if (!m_TriggerSpells.empty())
        CastTriggerSpells();
}

void Spell::DoSpellHitOnUnit(
    Unit* unit, TargetInfo* targetInfo, uint32 effectMask, bool isReflected)
{
    if (!unit)
        return;

    // Save facing before combat starts
    float facing = unit->GetO();

    // The caster the spell originates from: he gets threat, is put in combat,
    // etc. Not necessarily the same as the real caster.
    Unit* originating_caster = GetAffectiveCaster();

    // If spell originates from an aura proc
    if (originating_caster != m_caster && trigger_type_.triggered() &&
        m_triggeredByAuraSpell)
        originating_caster = m_caster;

    if (originating_caster && originating_caster != unit)
    {
        // Recheck UNIT_FLAG_NON_ATTACKABLE for delayed spells
        if (m_spellInfo->speed > 0.0f &&
            (unit->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE) ||
                (unit->HasFlag(
                     UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_PLAYER_ATTACKABLE) &&
                    m_caster->player_controlled())) &&
            unit->GetCharmerOrOwnerGuid() != m_caster->GetObjectGuid())
        {
            originating_caster->SendSpellMiss(
                unit, m_spellInfo->Id, SPELL_MISS_EVADE);
            ResetEffectDamageAndHeal();
            return;
        }

        if (!originating_caster->IsFriendlyTo(unit))
        {
            if (!IsPositiveSpell(m_spellInfo) ||
                m_spellInfo->HasEffect(SPELL_EFFECT_DISPEL))
            {
                // Make target stand on negative spell-hit
                if (!unit->IsStandState())
                    unit->SetStandState(UNIT_STAND_STATE_STAND);

                // Break auras that break on hit by spell
                if (!m_spellInfo->HasAttribute(
                        SPELL_ATTR_CUSTOM_NO_INITIAL_BLAST))
                {
                    unit->remove_auras_on_event(AURA_INTERRUPT_FLAG_HITBYSPELL);

                    // Stealth breakage uses custom rules
                    if (!m_spellInfo->HasAttribute(
                            SPELL_ATTR_CUSTOM_DONT_BREAK_TARGET_STEALTH))
                    {
                        unit->remove_auras(SPELL_AURA_MOD_STEALTH,
                            [](AuraHolder* holder)
                            {
                                auto info = holder->GetSpellProto();
                                return info->procFlags &
                                       PROC_FLAG_TAKEN_NEGATIVE_SPELL_HIT;
                            });
                    }
                }
            }

            // Toggle combat state on negative spell hit
            if (!m_spellInfo->HasAttribute(SPELL_ATTR_EX3_NO_INITIAL_AGGRO) &&
                !m_spellInfo->HasAttribute(SPELL_ATTR_EX_NO_THREAT) &&
                !m_spellInfo->HasAttribute(SPELL_ATTR_EX3_DONT_AFFECT_COMBAT) &&
                (!IsPositiveSpell(m_spellInfo->Id) ||
                    (IsDispelSpell(m_spellInfo) &&
                        m_caster->IsHostileTo(unit))) &&
                m_caster->can_be_seen_by(unit, unit))
            {
                // caster can be detected but have stealth aura
                if (!m_spellInfo->HasAttribute(SPELL_ATTR_EX_NOT_BREAK_STEALTH))
                    m_caster->remove_auras(SPELL_AURA_MOD_STEALTH);

                if (!(unit->GetTypeId() == TYPEID_UNIT &&
                        m_caster->HasFlag(
                            UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE)))
                {
                    if (!m_spellInfo->HasApplyAuraName(SPELL_AURA_MOD_POSSESS))
                    {
                        if (!unit->isInCombat())
                            unit->AttackedBy(originating_caster);
                        unit->AddThreat(originating_caster);
                    }

                    unit->SetInCombatWith(originating_caster);
                    originating_caster->SetInCombatWith(unit);

                    if (Player* attackedPlayer =
                            unit->GetCharmerOrOwnerPlayerOrPlayerItself())
                        originating_caster->SetContestedPvP(attackedPlayer);

                    if (!trigger_type_.triggered() || IsAutoRepeat())
                    {
                        if (unit->GetTypeId() == TYPEID_UNIT &&
                            m_caster->GetTypeId() == TYPEID_PLAYER)
                            static_cast<Creature*>(unit)->ResetKitingLeashPos();
                        // FIXME: Hack for Warsong Gulch's & AB's & Eye's food
                        // buff (restoration)
                        if (m_caster->GetMapId() == 489 ||
                            m_caster->GetMapId() == 529 ||
                            m_caster->GetMapId() == 566)
                            m_caster->remove_auras(
                                SPELL_AURA_PERIODIC_TRIGGER_SPELL,
                                [](AuraHolder* holder)
                                {
                                    return holder->GetId() == 24379;
                                });
                    }
                }
            }
        }
        else if (originating_caster->isTargetableForAttack())
        {
            // assisting case, healing and resurrection
            if (unit->hasUnitState(UNIT_STAT_ATTACK_PLAYER))
                originating_caster->SetContestedPvP();

            if (unit->isInCombat() &&
                !m_spellInfo->HasAttribute(SPELL_ATTR_EX3_NO_INITIAL_AGGRO) &&
                !m_spellInfo->HasAttribute(SPELL_ATTR_EX_NO_THREAT) &&
                !m_spellInfo->HasAttribute(SPELL_ATTR_EX3_DONT_AFFECT_COMBAT))
            {
                originating_caster->SetInCombatState(unit->InPvPCombat());
                unit->getHostileRefManager().threatAssist(
                    originating_caster, 0.0f, m_spellInfo);
            }
        }
    }

    Unit* realCaster = GetAffectiveCaster();

    // Get Data Needed for Diminishing Returns, some effects may have multiple
    // auras, so this must be done on spell hit, not aura add
    bool uncontrolled_proc =
        m_triggeredByAuraSpell || (m_CastItem && trigger_type_.triggered());
    m_diminishGroup =
        GetDiminishingReturnsGroupForSpell(m_spellInfo, uncontrolled_proc);
    m_diminishLevel = unit->GetDiminishing(m_diminishGroup);

    if (IsSpellAppliesAura(m_spellInfo, effectMask))
    {
        m_spellAuraHolder = CreateAuraHolder(
            m_spellInfo, unit, realCaster, m_CastItem, m_triggeredBySpellInfo);
        // HACK: Target move gen can make unit turn before stun is applied; we
        // need to save and restore facing.
        m_spellAuraHolder->SetBeforeStunFacing(facing);
    }
    else
        m_spellAuraHolder = nullptr;

    for (int effectNumber = 0; effectNumber < MAX_EFFECT_INDEX; ++effectNumber)
    {
        if (effectMask & (1 << effectNumber))
        {
            if (!IsEffectPartOfDamageCalculation(
                    m_spellInfo, SpellEffectIndex(effectNumber)))
            {
                HandleEffects(
                    unit, nullptr, nullptr, SpellEffectIndex(effectNumber));
            }
            if (m_applyMultiplierMask & (1 << effectNumber))
            {
                // Apply multiplier for this target, set in
                // Spell::InitializeDamageMultipliers()
                m_healing *= targetInfo->damageMultipliers[effectNumber];
                m_damage *= targetInfo->damageMultipliers[effectNumber];
            }
        }
    }

    // now apply all created auras
    if (m_spellAuraHolder)
    {
        // normally shouldn't happen
        if (m_spellAuraHolder->IsEmptyHolder())
        {
            delete m_spellAuraHolder;
            m_spellAuraHolder = nullptr;
            return;
        }

        if (!IsPassiveSpell(m_spellInfo))
        {
            int32 originalDuration = m_spellAuraHolder->GetAuraMaxDuration();

            HandleDurationDR(unit, isReflected, effectMask);

            if (m_duration == 0)
            {
                if (unit != m_caster)
                    m_caster->SendSpellMiss(
                        unit, m_spellInfo->Id, SPELL_MISS_IMMUNE);
                delete m_spellAuraHolder;
                m_spellAuraHolder = nullptr;
                return;
            }

            if (m_duration != originalDuration)
            {
                if (originalDuration < m_duration)
                    m_duration = originalDuration;

                m_spellAuraHolder->SetAuraMaxDuration(m_duration);
                m_spellAuraHolder->SetAuraDuration(m_duration);
            }
        }

        m_spellAuraHolder->SetDiminishingGroup(m_diminishGroup);
        if (unit->AddAuraHolder(m_spellAuraHolder))
        {
            // Increase Diminishing Returns on target if the aura was added
            // successfully
            Unit* owner = unit->GetCharmerOrOwner();
            if (owner)
            {
                if (Unit* iowner = owner->GetCharmerOrOwner())
                    owner = iowner;
            }
            else
                owner = unit;
            Unit* caster =
                GetAffectiveCaster() ? GetAffectiveCaster() : m_caster;
            bool subject_to_player_dr = owner->GetTypeId() == TYPEID_PLAYER;
            if (((GetDiminishingReturnsGroupType(m_diminishGroup) ==
                         DRTYPE_PLAYER &&
                     subject_to_player_dr) ||
                    GetDiminishingReturnsGroupType(m_diminishGroup) ==
                        DRTYPE_ALL) &&
                (m_caster != unit || isReflected) &&
                caster->player_controlled())
                unit->IncrDiminishing(m_diminishGroup);
        }
    }

    // Apply additional spell effects to target
    CastPreCastSpells(unit);
}

void Spell::DoAllEffectOnTarget(GOTargetInfo* target)
{
    if (target->processed) // Check target
        return;
    target->processed = true; // Target checked in apply effects procedure

    uint32 effectMask = target->effectMask;
    if (!effectMask)
        return;

    GameObject* go = m_caster->GetMap()->GetGameObject(target->targetGUID);
    if (!go)
        return;

    for (int effectNumber = 0; effectNumber < MAX_EFFECT_INDEX; ++effectNumber)
        if (effectMask & (1 << effectNumber))
            HandleEffects(nullptr, nullptr, go, SpellEffectIndex(effectNumber));

    // cast at creature (or GO) quest objectives update at successful cast
    // finished (+channel finished)
    // ignore autorepeat/melee casts for speed (not exist quest for spells
    // (hm... )
    if (!IsAutoRepeat() && !IsNextMeleeSwingSpell() && !IsChannelActive())
    {
        if (Player* p = m_caster->GetCharmerOrOwnerPlayerOrPlayerItself())
            p->RewardPlayerAndGroupAtCast(go, m_spellInfo->Id);
    }
}

void Spell::DoAllEffectOnTarget(ItemTargetInfo* target)
{
    uint32 effectMask = target->effectMask;
    if (!target->item || !effectMask)
        return;

    for (int effectNumber = 0; effectNumber < MAX_EFFECT_INDEX; ++effectNumber)
        if (effectMask & (1 << effectNumber))
            HandleEffects(
                nullptr, target->item, nullptr, SpellEffectIndex(effectNumber));
}

void Spell::HandleDelayedSpellLaunch(TargetInfo* target)
{
    // Get mask of effects for target
    uint32 mask = target->effectMask;

    Unit* unit = m_caster->GetObjectGuid() == target->targetGUID ?
                     m_caster :
                     ObjectAccessor::GetUnit(*m_caster, target->targetGUID);
    if (!unit)
        return;

    // Get original caster (if exist) and calculate damage/healing from him data
    Unit* real_caster = GetAffectiveCaster();
    // FIXME: in case wild GO heal/damage spells will be used target bonuses
    Unit* caster = real_caster ? real_caster : m_caster;

    SpellMissInfo missInfo = target->missCondition;
    unitTarget = unit;

    // Reset damage/healing counter
    m_damage = 0;
    m_healing = 0; // healing maybe not needed at this point

    // Fill base damage struct
    SpellNonMeleeDamage damageInfo(
        caster, unit, m_spellInfo->Id, m_spellSchoolMask);

    // keep damage amount for reflected spells
    if (missInfo == SPELL_MISS_NONE ||
        (missInfo == SPELL_MISS_REFLECT &&
            target->reflectResult == SPELL_MISS_NONE))
    {
        for (int32 effectNumber = 0; effectNumber < MAX_EFFECT_INDEX;
             ++effectNumber)
        {
            if (mask & (1 << effectNumber) &&
                IsEffectPartOfDamageCalculation(
                    m_spellInfo, SpellEffectIndex(effectNumber)))
            {
                HandleEffects(
                    unit, nullptr, nullptr, SpellEffectIndex(effectNumber));
                // NOTE: multiplier for target applied in
                // Spell::DoSpellHitOnUnit()
            }
        }

        if (m_damage > 0)
            caster->CalculateSpellDamage(&damageInfo, m_damage, m_spellInfo,
                m_attackType, 0, 0.0f, this);
    }

    target->damage = damageInfo.damage;
    target->HitInfo = damageInfo.HitInfo;

    // Drop combo points for travel-time spells
    DropComboPointsIfNeeded(false);
}

void Spell::InitializeDamageMultipliers()
{
    Unit* real_caster = GetAffectiveCaster();

    for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (m_spellInfo->Effect[i] == 0)
            continue;

        uint32 EffectChainTarget = m_spellInfo->EffectChainTarget[i];
        if (Unit* realCaster = GetAffectiveCaster())
            if (Player* modOwner = realCaster->GetSpellModOwner())
                modOwner->ApplySpellMod(m_spellInfo->Id, SPELLMOD_JUMP_TARGETS,
                    EffectChainTarget, this);

        if ((m_spellInfo->EffectImplicitTargetA[i] == TARGET_CHAIN_DAMAGE ||
                m_spellInfo->EffectImplicitTargetA[i] == TARGET_CHAIN_HEAL) &&
            (EffectChainTarget > 1))
            m_applyMultiplierMask |= (1 << i);
        else
            continue; // don't calculate multiplier

        if (m_UniqueTargetInfo.empty())
            continue; // dont calculate multipliers if no targets

        // Calculate multiplier for all targets
        float multiplier = 1.0f;

        auto itr = m_UniqueTargetInfo.begin();
        for (++itr; itr != m_UniqueTargetInfo.end(); ++itr)
        {
            float step = m_spellInfo->DmgMultiplier[i];
            if (real_caster)
                if (Player* mod_owner = real_caster->GetSpellModOwner())
                    mod_owner->ApplySpellMod(m_spellInfo->Id,
                        SPELLMOD_EFFECT_PAST_FIRST, step, this);
            multiplier *= step;
            itr->damageMultipliers[i] = multiplier;
        }
    }
}

bool Spell::IsAliveUnitPresentInTargetList()
{
    // Not need check return true
    if (m_needAliveTargetMask == 0)
        return true;

    uint8 needAliveTargetMask = m_needAliveTargetMask;

    for (TargetList::const_iterator ihit = m_UniqueTargetInfo.begin();
         ihit != m_UniqueTargetInfo.end(); ++ihit)
    {
        if (ihit->missCondition == SPELL_MISS_NONE &&
            (needAliveTargetMask & ihit->effectMask))
        {
            Unit* unit =
                m_caster->GetObjectGuid() == ihit->targetGUID ?
                    m_caster :
                    ObjectAccessor::GetUnit(*m_caster, ihit->targetGUID);

            // either unit is alive and normal spell, or unit dead and
            // deathonly-spell
            if (unit && (unit->isAlive() != IsDeathOnlySpell(m_spellInfo)))
                needAliveTargetMask &=
                    ~ihit->effectMask; // remove from need alive mask effect
                                       // that have alive target
        }
    }

    // is all effects from m_needAliveTargetMask have alive targets
    return needAliveTargetMask == 0;
}

// Helper for Chain Healing
// Spell target first
// Raidmates then descending by injury suffered (MaxHealth - Health)
// Other players/mobs then descending by injury suffered (MaxHealth - Health)
struct ChainHealingOrder
    : public std::binary_function<const Unit*, const Unit*, bool>
{
    const Unit* MainTarget;
    ChainHealingOrder(Unit const* Target) : MainTarget(Target){};
    // functor for operator ">"
    bool operator()(Unit const* _Left, Unit const* _Right) const
    {
        return (ChainHealingHash(_Left) < ChainHealingHash(_Right));
    }
    int32 ChainHealingHash(Unit const* Target) const
    {
        if (Target == MainTarget)
            return 0;
        else if (Target->GetTypeId() == TYPEID_PLAYER &&
                 MainTarget->GetTypeId() == TYPEID_PLAYER &&
                 ((Player const*)Target)
                     ->IsInSameRaidWith((Player const*)MainTarget))
        {
            if (Target->GetHealth() == Target->GetMaxHealth())
                return 40000;
            else
                return 20000 - Target->GetMaxHealth() + Target->GetHealth();
        }
        else
            return 40000 - Target->GetMaxHealth() + Target->GetHealth();
    }
};

class ChainHealingFullHealth : std::unary_function<const Unit*, bool>
{
public:
    const Unit* MainTarget;
    ChainHealingFullHealth(const Unit* Target) : MainTarget(Target){};

    bool operator()(const Unit* Target)
    {
        return (Target != MainTarget &&
                Target->GetHealth() == Target->GetMaxHealth());
    }
};

// Helper for targets nearest to the spell target
// The spell target is always first unless there is a target at _completely_ the
// same position (unbelievable case)
struct TargetDistanceOrderNear
    : public std::binary_function<const Unit, const Unit, bool>
{
    const Unit* MainTarget;
    TargetDistanceOrderNear(const Unit* Target) : MainTarget(Target){};
    // functor for operator ">"
    bool operator()(const Unit* _Left, const Unit* _Right) const
    {
        return MainTarget->GetDistanceOrder(_Left, _Right);
    }
};

// Helper for targets furthest away to the spell target
// The spell target is always first unless there is a target at _completely_ the
// same position (unbelievable case)
struct TargetDistanceOrderFarAway
    : public std::binary_function<const Unit, const Unit, bool>
{
    const Unit* MainTarget;
    TargetDistanceOrderFarAway(const Unit* Target) : MainTarget(Target){};
    // functor for operator "<"
    bool operator()(const Unit* _Left, const Unit* _Right) const
    {
        return !MainTarget->GetDistanceOrder(_Left, _Right);
    }
};

void Spell::SetTargetMap(
    SpellEffectIndex effIndex, uint32 targetMode, UnitList& targetUnitMap)
{
    float radius;
    if (m_spellInfo->EffectRadiusIndex[effIndex])
        radius = GetSpellRadius(sSpellRadiusStore.LookupEntry(
            m_spellInfo->EffectRadiusIndex[effIndex]));
    else
        radius = GetSpellMaxRange(
            sSpellRangeStore.LookupEntry(m_spellInfo->rangeIndex));

    m_scriptTarget = false;

    // If spell is a spell that spreads over time, we need to recalculate the
    // radius:
    float spread = 0.0f;
    if (GetSpreadingRadius(radius, spread))
        radius = spread;

    uint32 EffectChainTarget = m_spellInfo->EffectChainTarget[effIndex];

    if (Unit* realCaster = GetAffectiveCaster())
    {
        if (Player* modOwner = realCaster->GetSpellModOwner())
        {
            modOwner->ApplySpellMod(
                m_spellInfo->Id, SPELLMOD_RADIUS, radius, this);
            modOwner->ApplySpellMod(m_spellInfo->Id, SPELLMOD_JUMP_TARGETS,
                EffectChainTarget, this);
        }
    }

    // Get spell max affected targets
    uint32 unMaxTargets = m_spellInfo->MaxAffectedTargets;

    // custom target amount cases
    switch (m_spellInfo->SpellFamilyName)
    {
    case SPELLFAMILY_GENERIC:
    {
        switch (m_spellInfo->Id)
        {
        case 23138: // Gate of Shazzrah
        case 28560: // Summon Blizzard
        case 29883: // Blink (Karazhan, Arcane Anomaly)
        case 31347: // Doom TODO: exclude top threat target from target
                    // selection
        case 33711: // Murmur's Touch
        case 34094: // Power of Arrazius
        case 36717: // Energy Discharge (Arcatraz N)
        case 38573: // Spore Drop Effect
        case 38794: // Murmur's Touch (h)
        case 38829: // Energy Discharge (Arcatraz H)
        case 39042: // Rampant Infection (SSC)
        case 44869: // Spectral Blast
        case 45976: // Open Portal
            unMaxTargets = 1;
            break;
        case 28542: // Life Drain
            unMaxTargets = 2;
            break;
        case 31298: // Sleep
        case 37790: // Spread Shot
        case 38650: // Rancid Mushroom Primer (SSC)
        case 39992: // Needle Spine Targeting (BT, Warlord Najentus)
            unMaxTargets = 3;
            break;
        case 30843: // Enfeeble TODO: exclude top threat target from target
                    // selection
        case 33332: // Suppression Blast (Shadow Labyrinth)
        case 37676: // Insidious Whisper
        case 42005: // Bloodboil TODO: need to be 5 targets(players) furthest
                    // away from caster
        case 45641: // Fire Bloom (SWP, Kil'jaeden)
            unMaxTargets = 5;
            break;
        case 28796: // Poison Bolt Volley
            unMaxTargets = 10;
            break;
        case 46771: // Flame Sear (SWP, Grand Warlock Alythess)
            unMaxTargets = urand(3, 5);
            break;
        case 30284:        // Change Facing (Karazhan, Chess)
            radius = 3.5f; // Changing radius here too
            unMaxTargets = 1;
            break;
        }
        break;
    }
    case SPELLFAMILY_MAGE:
    {
        if (m_spellInfo->Id == 38194) // Blink
            unMaxTargets = 1;
        break;
    }
    default:
        break;
    }

    // custom radius cases
    switch (m_spellInfo->SpellFamilyName)
    {
    case SPELLFAMILY_GENERIC:
    {
        switch (m_spellInfo->Id)
        {
        case 24811: // Draw Spirit (Lethon)
        {
            if (effIndex == EFFECT_INDEX_0) // Copy range from EFF_1 to 0
                radius = GetSpellRadius(sSpellRadiusStore.LookupEntry(
                    m_spellInfo->EffectRadiusIndex[EFFECT_INDEX_1]));
            break;
        }
        case 39384: // Burning Flames, Fury of Medivh (Karazhan, Chess)
            radius = 1.5f;
            break;
        default:
            break;
        }
        break;
    }
    default:
        break;
    }

    switch (targetMode)
    {
    case TARGET_RANDOM_NEARBY_LOC:
        // Get a random point in circle. Use sqrt(rand) to correct distribution
        // when converting polar to Cartesian coordinates.
        radius *= sqrtf(rand_norm_f());
    // no 'break' expected since we use code in case
    // TARGET_RANDOM_CIRCUMFERENCE_POINT!!!
    case TARGET_RANDOM_CIRCUMFERENCE_POINT:
    {
        // Get a random point AT the circumference
        float angle = 2.0f * M_PI_F * rand_norm_f();
        auto pos = m_caster->GetPoint(radius, angle);

        switch (m_spellInfo->Id)
        {
        case 38724: // Magic Sucker Device (Success Visual)
            pos.z += radius;
            break;
        default:
            break;
        }

        m_targets.setDestination(pos.x, pos.y, pos.z);

        targetUnitMap.push_back(m_caster);
        break;
    }
    case TARGET_DEST_TARGET_RANDOM:
    case TARGET_RANDOM_NEARBY_DEST:
    {
        // Get a random point IN the CIRCEL around current M_TARGETS
        // COORDINATES(!).
        if (radius > 0.0f)
        {
            // Use sqrt(rand) to correct distribution when converting polar to
            // Cartesian coordinates.
            radius *= sqrtf(rand_norm_f());
            float angle = 2.0f * M_PI_F * rand_norm_f();
            float dest_x = m_targets.m_destX + cos(angle) * radius;
            float dest_y = m_targets.m_destY + sin(angle) * radius;
            float dest_z = m_caster->GetZ();
            m_caster->UpdateGroundPositionZ(dest_x, dest_y, dest_z);
            m_targets.setDestination(dest_x, dest_y, dest_z);
        }

        // This targetMode is often used as 'last' implicitTarget for positive
        // spells, that just require coordinates
        // and no unitTarget (e.g. summon effects). As MaNGOS always needs a
        // unitTarget we add just the caster here.
        if (IsPositiveSpell(m_spellInfo))
            targetUnitMap.push_back(m_caster);

        break;
    }
    case TARGET_TOTEM_EARTH:
    case TARGET_TOTEM_WATER:
    case TARGET_TOTEM_AIR:
    case TARGET_TOTEM_FIRE:
    case TARGET_SELF:
    case TARGET_SELF2:
        targetUnitMap.push_back(m_caster);
        break;
    case TARGET_RANDOM_ENEMY_CHAIN_IN_AREA:
    {
        m_targets.m_targetMask = 0;
        unMaxTargets = EffectChainTarget;
        float max_range = radius + unMaxTargets * m_spellInfo->bounce_radius;

        UnitList tempTargetUnitMap;

        {
            bool targetForPlayer = m_caster->IsControlledByPlayer();
            auto tmp = maps::visitors::yield_set<Unit, Player, Creature, Pet,
                SpecialVisCreature, TemporarySummon>{}(m_caster, max_range,
                [targetForPlayer, this](Unit* u)
                {
                    // Check contains checks for: live, non-selectable,
                    // non-attackable
                    // flags, flight check and GM check, ignore totems
                    if (!u->isTargetableForAttack())
                        return false;

                    if (u->GetTypeId() == TYPEID_UNIT &&
                        ((Creature*)u)->IsTotem())
                        return false;

                    if ((targetForPlayer ? !m_caster->IsFriendlyTo(u) :
                                           m_caster->IsHostileTo(u)))
                        return true;

                    return false;
                });
            tempTargetUnitMap.assign(tmp.begin(), tmp.end());
        }

        if (tempTargetUnitMap.empty())
            break;

        tempTargetUnitMap.sort(TargetDistanceOrderNear(m_caster));

        // Now to get us a random target that's in the initial range of the
        // spell
        uint32 t = 0;
        auto itr = tempTargetUnitMap.begin();
        while (itr != tempTargetUnitMap.end() &&
               (*itr)->IsWithinDist(m_caster, radius))
            ++t, ++itr;

        if (!t)
            break;

        itr = tempTargetUnitMap.begin();
        std::advance(itr, urand(0, t - 1));
        Unit* pUnitTarget = *itr;
        targetUnitMap.push_back(pUnitTarget);

        tempTargetUnitMap.erase(itr);

        tempTargetUnitMap.sort(TargetDistanceOrderNear(pUnitTarget));

        t = unMaxTargets - 1;
        Unit* prev = pUnitTarget;
        auto next = tempTargetUnitMap.begin();

        while (t && next != tempTargetUnitMap.end())
        {
            if (!prev->IsWithinDist(*next, m_spellInfo->bounce_radius))
                break;

            if (!prev->IsWithinWmoLOSInMap(*next))
            {
                ++next;
                continue;
            }

            prev = *next;
            targetUnitMap.push_back(prev);
            tempTargetUnitMap.erase(next);
            tempTargetUnitMap.sort(TargetDistanceOrderNear(prev));
            next = tempTargetUnitMap.begin();

            --t;
        }
        break;
    }
    case TARGET_RANDOM_FRIEND_CHAIN_IN_AREA:
    case TARGET_RANDOM_FRIEND_CHAIN_IN_AREA2:
    {
        m_targets.m_targetMask = 0;
        unMaxTargets = EffectChainTarget;
        float max_range = radius + unMaxTargets * m_spellInfo->bounce_radius;
        UnitList tempTargetUnitMap;
        {
            auto tmp = maps::visitors::yield_set<Unit, Player, Creature, Pet,
                SpecialVisCreature, TemporarySummon>{}(m_caster, max_range,
                maps::checks::friendly_status{
                    m_caster, maps::checks::friendly_status::friendly});
            tempTargetUnitMap.assign(tmp.begin(), tmp.end());
        }

        if (tempTargetUnitMap.empty())
            break;

        // Exclude caster for TARGET_RANDOM_FRIEND_CHAIN_IN_AREA2
        if (targetMode == TARGET_RANDOM_FRIEND_CHAIN_IN_AREA2)
            tempTargetUnitMap.remove(m_caster);

        tempTargetUnitMap.sort(TargetDistanceOrderNear(m_caster));

        // Now to get us a random target that's in the initial range of the
        // spell
        uint32 t = 0;
        auto itr = tempTargetUnitMap.begin();
        while (itr != tempTargetUnitMap.end() &&
               (*itr)->IsWithinDist(m_caster, radius))
            ++t, ++itr;

        if (!t)
            break;

        itr = tempTargetUnitMap.begin();
        std::advance(itr, urand(0, t - 1));
        Unit* pUnitTarget = *itr;
        targetUnitMap.push_back(pUnitTarget);

        tempTargetUnitMap.erase(itr);

        tempTargetUnitMap.sort(TargetDistanceOrderNear(pUnitTarget));

        t = unMaxTargets - 1;
        Unit* prev = pUnitTarget;
        auto next = tempTargetUnitMap.begin();

        while (t && next != tempTargetUnitMap.end())
        {
            if (!prev->IsWithinDist(*next, m_spellInfo->bounce_radius))
                break;

            if (!prev->IsWithinWmoLOSInMap(*next))
            {
                ++next;
                continue;
            }
            prev = *next;
            targetUnitMap.push_back(prev);
            tempTargetUnitMap.erase(next);
            tempTargetUnitMap.sort(TargetDistanceOrderNear(prev));
            next = tempTargetUnitMap.begin();
            --t;
        }
        break;
    }
    case TARGET_PET:
    {
        Unit* tmp = m_caster->GetPet();
        if (!tmp)
            tmp = m_caster->GetCharm();
        if (!tmp)
            break;
        targetUnitMap.push_back(tmp);
        break;
    }
    case TARGET_CHAIN_DAMAGE:
    {
        if (EffectChainTarget <= 1)
        {
            if (Unit* pUnitTarget = m_caster->SelectMagnetTarget(
                    m_targets.getUnitTarget(), this, effIndex))
            {
                m_targets.setUnitTarget(pUnitTarget);
                targetUnitMap.push_back(pUnitTarget);
            }
        }
        else
        {
            Unit* pUnitTarget = m_targets.getUnitTarget();
            WorldObject* originalCaster = GetAffectiveCasterObject();
            if (!pUnitTarget || !originalCaster)
                break;

            unMaxTargets = EffectChainTarget;

            bool cleave = m_spellInfo->DmgClass == SPELL_DAMAGE_CLASS_MELEE;
            float max_range;
            max_range =
                unMaxTargets * (cleave ? 10.0f : m_spellInfo->bounce_radius);

            UnitList tempTargetUnitMap;
            bool targetForUnit = originalCaster->isType(TYPEMASK_UNIT);
            bool targetForPlayer =
                (originalCaster->GetTypeId() == TYPEID_PLAYER);
            auto tmp = maps::visitors::yield_set<Unit, Player, Creature, Pet,
                SpecialVisCreature, TemporarySummon>{}(
                pUnitTarget, max_range,
                [this, targetForUnit, targetForPlayer, pUnitTarget,
                    originalCaster](Unit* u)
                {
                    // Check contains checks for: live, non-selectable,
                    // non-attackable flags, flight check and GM check, ignore
                    // totems
                    if (!u->isTargetableForAttack())
                        return false;

                    // check visibility only for unit-like original casters
                    if (targetForUnit && m_spellInfo->rangeIndex != 13 &&
                        !u->can_be_seen_by(
                            (Unit const*)originalCaster, originalCaster))
                        return false;

                    if ((targetForPlayer ? !originalCaster->IsFriendlyTo(u) :
                                           originalCaster->IsHostileTo(u)))
                        return true;

                    return false;
                });
            tempTargetUnitMap.assign(tmp.begin(), tmp.end());

            if (tempTargetUnitMap.empty())
                break;

            tempTargetUnitMap.sort(TargetDistanceOrderNear(pUnitTarget));

            if (*tempTargetUnitMap.begin() == pUnitTarget)
                tempTargetUnitMap.erase(tempTargetUnitMap.begin());

            targetUnitMap.push_back(pUnitTarget);
            uint32 t = unMaxTargets - 1;
            Unit* prev = pUnitTarget;
            auto next = tempTargetUnitMap.begin();

            while (t && next != tempTargetUnitMap.end())
            {
                // Cleave bounce: scale melee reach with caster's bounding box
                if (cleave &&
                    !prev->IsWithinDist(
                        *next, m_caster->GetMeleeReach(*next), true, false))
                    break;
                // Otherwise: use bounce radius in spell_dbc
                else if (!cleave &&
                         !prev->IsWithinDist(*next, m_spellInfo->bounce_radius))
                    break;

                if (!prev->IsWithinWmoLOSInMap(*next))
                {
                    ++next;
                    continue;
                }

                // Patch 2.1 behavior:
                // Prior to patch 2.1 NPC cleaves could hit behind the caster.
                // Every other spell can only hit targets in front of caster.
                if (!m_spellInfo->HasAttribute(SPELL_ATTR_CUSTOM_360_BOUNCE) &&
                    (m_caster->GetTypeId() != TYPEID_UNIT || !cleave))
                {
                    // 180 degree angle
                    if (!m_caster->HasInArc(M_PI, *next))
                    {
                        ++next;
                        continue;
                    }
                }

                prev = *next;
                targetUnitMap.push_back(prev);
                tempTargetUnitMap.erase(next);
                tempTargetUnitMap.sort(TargetDistanceOrderNear(prev));
                next = tempTargetUnitMap.begin();

                --t;
            }
        }
        break;
    }
    case TARGET_ALL_ENEMY_IN_AREA:
    {
        // Crone's Cyclone knockup (Karazhan)
        // Carbdis' Cyclone knockup (SSC)
        // TODO: Find general functionality for this. Is it possible AoE jut
        // don't care about Z axis? Only LoS in that direction.
        if (m_spellInfo->Id == 29538 || m_spellInfo->Id == 38517)
        {
            // Increase radius in Z-axis only
            UnitList tempUL;
            FillAreaTargets(
                tempUL, 30, PUSH_DEST_CENTER, SPELL_TARGETS_AOE_DAMAGE);
            for (auto& elem : tempUL)
            {
                if (m_caster->GetDistance2d(elem) <= radius)
                    targetUnitMap.push_back(elem);
            }
            break;
        }

        FillAreaTargets(
            targetUnitMap, radius, PUSH_DEST_CENTER, SPELL_TARGETS_AOE_DAMAGE);

        switch (m_spellInfo->Id)
        {
        case 5246: // Intimidating shout (AoE Fear Effect does not hit main
                   // target)
            if (Unit* mainTarget = m_targets.getUnitTarget())
                targetUnitMap.remove(mainTarget);
            break;
        case 23002: // Alarm-o-bot. Only hits stealthed or invisible targets
            for (auto itr = targetUnitMap.begin(); itr != targetUnitMap.end();)
            {
                if (!(*itr)->HasAuraType(SPELL_AURA_MOD_STEALTH) &&
                    !(*itr)->HasAuraType(SPELL_AURA_MOD_INVISIBILITY))
                    itr = targetUnitMap.erase(itr);
                else
                    ++itr;
            }
            break;
        // Doesn't hit CHAIN DAMAGE TARGET
        case 36797: // Mind Control
        case 37676: // Insidious Whisper
            if (m_caster->getVictim())
                targetUnitMap.remove(m_caster->getVictim());
            break;
        // Sort by closest targets
        case 26052: // Poison Bolt
        case 26180: // Wyvern Sting
        {
            if (targetUnitMap.size() > unMaxTargets)
            {
                targetUnitMap.sort(TargetDistanceOrderNear(m_caster));
                targetUnitMap.resize(unMaxTargets);
            }
            break;
        }
        // Sort by targets furthest away
        case 42005: // Bloodboil
        {
            if (targetUnitMap.size() > unMaxTargets)
            {
                targetUnitMap.sort(TargetDistanceOrderFarAway(m_caster));
                targetUnitMap.resize(unMaxTargets);
            }
            break;
        }
        default:
            break;
        }
    }
    break;
    case TARGET_AREAEFFECT_INSTANT:
    {
        SpellTargets targetB = SPELL_TARGETS_AOE_DAMAGE;

        // Select friendly targets for positive effect
        if (IsPositiveEffect(m_spellInfo, effIndex))
            targetB = SPELL_TARGETS_FRIENDLY;

        UnitList tempTargetUnitMap;
        SpellScriptTargetBounds bounds =
            sSpellMgr::Instance()->GetSpellScriptTargetBounds(m_spellInfo->Id);

        // fill real target list if no spell script target defined
        FillAreaTargets(
            bounds.first != bounds.second ? tempTargetUnitMap : targetUnitMap,
            radius, PUSH_DEST_CENTER,
            bounds.first != bounds.second ? SPELL_TARGETS_ALL : targetB);

        if (!tempTargetUnitMap.empty())
        {
            m_scriptTarget = true;
            for (UnitList::const_iterator iter = tempTargetUnitMap.begin();
                 iter != tempTargetUnitMap.end(); ++iter)
            {
                if ((*iter)->GetTypeId() != TYPEID_UNIT)
                    continue;

                for (auto i_spellST = bounds.first; i_spellST != bounds.second;
                     ++i_spellST)
                {
                    // only creature entries supported for this target type
                    if (i_spellST->second.type == SPELL_TARGET_TYPE_GAMEOBJECT)
                        continue;

                    if ((*iter)->GetEntry() == i_spellST->second.targetEntry)
                    {
                        if (i_spellST->second.type == SPELL_TARGET_TYPE_DEAD &&
                            ((Creature*)(*iter))->IsCorpse())
                        {
                            targetUnitMap.push_back((*iter));
                        }
                        else if (i_spellST->second.type ==
                                     SPELL_TARGET_TYPE_CREATURE &&
                                 (*iter)->isAlive())
                        {
                            targetUnitMap.push_back((*iter));
                        }

                        break;
                    }
                }
            }
        }

        switch (m_spellInfo->Id)
        {
        case 34946: // Golem Repair
            // Remove any target that doesn't have Powered Down
            for (auto itr = targetUnitMap.begin(); itr != targetUnitMap.end();)
                if (!(*itr)->has_aura(34937))
                    itr = targetUnitMap.erase(itr);
                else
                    ++itr;
            break;
        default:
            break;
        }

        // exclude caster
        targetUnitMap.remove(m_caster);
        break;
    }
    case TARGET_AREAEFFECT_CUSTOM:
    {
        if (m_spellInfo->Effect[effIndex] == SPELL_EFFECT_PERSISTENT_AREA_AURA)
            break;
        else if (m_spellInfo->Effect[effIndex] == SPELL_EFFECT_SUMMON)
        {
            targetUnitMap.push_back(m_caster);
            break;
        }

        UnitList tempTargetUnitMap;
        SpellScriptTargetBounds bounds =
            sSpellMgr::Instance()->GetSpellScriptTargetBounds(m_spellInfo->Id);
        // fill real target list if no spell script target defined
        FillAreaTargets(
            bounds.first != bounds.second ? tempTargetUnitMap : targetUnitMap,
            radius, PUSH_DEST_CENTER, SPELL_TARGETS_ALL);

        if (!tempTargetUnitMap.empty())
        {
            for (UnitList::const_iterator iter = tempTargetUnitMap.begin();
                 iter != tempTargetUnitMap.end(); ++iter)
            {
                if ((*iter)->GetTypeId() != TYPEID_UNIT)
                    continue;

                for (auto i_spellST = bounds.first; i_spellST != bounds.second;
                     ++i_spellST)
                {
                    // only creature entries supported for this target type
                    if (i_spellST->second.type == SPELL_TARGET_TYPE_GAMEOBJECT)
                        continue;

                    if ((*iter)->GetEntry() == i_spellST->second.targetEntry)
                    {
                        if (i_spellST->second.type == SPELL_TARGET_TYPE_DEAD &&
                            ((Creature*)(*iter))->IsCorpse())
                        {
                            targetUnitMap.push_back((*iter));
                        }
                        else if (i_spellST->second.type ==
                                     SPELL_TARGET_TYPE_CREATURE &&
                                 (*iter)->isAlive())
                        {
                            targetUnitMap.push_back((*iter));
                        }

                        break;
                    }
                }
            }
        }
        else
        {
            // remove not targetable units if spell has no script targets
            for (auto itr = targetUnitMap.begin(); itr != targetUnitMap.end();)
            {
                if (!(*itr)->isTargetableForAttack(
                        m_spellInfo->HasAttribute(SPELL_ATTR_EX3_CAST_ON_DEAD)))
                    targetUnitMap.erase(itr++);
                else
                    ++itr;
            }
        }
        break;
    }
    case TARGET_AREAEFFECT_GO_AROUND_DEST:
    {
        // It may be possible to fill targets for some spell effects
        // automatically (SPELL_EFFECT_WMO_REPAIR(88) for example) but
        // for some/most spells we clearly need/want to limit with
        // spell_target_script

        // Some spells untested, for affected GO type 33. May need further
        // adjustments for spells related.

        SpellScriptTargetBounds bounds =
            sSpellMgr::Instance()->GetSpellScriptTargetBounds(m_spellInfo->Id);

        std::vector<GameObject*> tempTargetGOList;

        for (auto i_spellST = bounds.first; i_spellST != bounds.second;
             ++i_spellST)
        {
            if (i_spellST->second.type == SPELL_TARGET_TYPE_GAMEOBJECT)
            {
                tempTargetGOList = maps::visitors::yield_set<GameObject>{}(
                    m_caster, radius,
                    maps::checks::entry_guid{i_spellST->second.targetEntry, 0});
            }
        }

        for (auto& elem : tempTargetGOList)
            AddGOTarget(elem, effIndex);

        break;
    }
    case TARGET_ALL_ENEMY_IN_AREA_INSTANT:
    {
        // targets the ground, not the units in the area
        switch (m_spellInfo->Effect[effIndex])
        {
        case SPELL_EFFECT_PERSISTENT_AREA_AURA:
            break;
        case SPELL_EFFECT_SUMMON:
            targetUnitMap.push_back(m_caster);
            break;
        default:
            FillAreaTargets(targetUnitMap, radius, PUSH_DEST_CENTER,
                SPELL_TARGETS_AOE_DAMAGE);
            break;
        }
        break;
    }
    case TARGET_DUELVSPLAYER_COORDINATES:
    {
        if (Unit* currentTarget = m_targets.getUnitTarget())
            m_targets.setDestination(currentTarget->GetX(),
                currentTarget->GetY(), currentTarget->GetZ());
        break;
    }
    case TARGET_ALL_PARTY_AROUND_CASTER:
    case TARGET_ALL_PARTY_AROUND_CASTER_2:
    case TARGET_ALL_PARTY:
    {
        // Player or player-controlled NPC:
        if ((m_caster->GetTypeId() == TYPEID_PLAYER &&
                !m_caster->GetCharmerOrOwnerGuid().IsUnit()) ||
            m_caster->GetCharmerOrOwnerGuid()
                .IsPlayer()) // Player or player owned unit
        {
            FillRaidOrPartyTargets(
                targetUnitMap, m_caster, radius, false, true, true);
        }
        // NPC or NPC-controlled Player:
        else
        {
            // For NPCs this means other in the same CreatureGroup. It is used
            // for long-range AoE heals and stuff, and by using the same group
            // it avoids hitting other NPCs that might be out of combat.
            auto npc = m_caster->GetCharmerOrOwnerOrSelf();
            if (npc && npc->GetTypeId() == TYPEID_UNIT)
            {
                // Add all group members
                if (auto group = static_cast<Creature*>(npc)->GetGroup())
                {
                    for (auto member : group->GetMembers())
                    {
                        if (member->isAlive())
                            targetUnitMap.push_back(member);
                    }
                }
                // Add self if no group exists
                else
                {
                    targetUnitMap.push_back(npc);
                }
                // Make sure caster is always targeted
                if (npc != m_caster &&
                    std::find(targetUnitMap.begin(), targetUnitMap.end(),
                        m_caster) == targetUnitMap.end())
                {
                    targetUnitMap.push_back(m_caster);
                }
            }
        }
        break;
    }
    case TARGET_ALL_RAID_AROUND_CASTER:
    {
        FillRaidOrPartyTargets(targetUnitMap, m_caster, radius, true, true,
            IsPositiveSpell(m_spellInfo->Id));
        break;
    }
    case TARGET_SINGLE_FRIEND:
    case TARGET_SINGLE_FRIEND_2:
        if (m_targets.getUnitTarget())
            targetUnitMap.push_back(m_targets.getUnitTarget());
        break;
    case TARGET_NONCOMBAT_PET:
        if (Unit* target = m_targets.getUnitTarget())
            if (target->GetTypeId() == TYPEID_UNIT &&
                ((Creature*)target)->IsPet() &&
                ((Pet*)target)->getPetType() == MINI_PET)
                targetUnitMap.push_back(target);
        break;
    case TARGET_CASTER_COORDINATES:
    {
        if (WorldObject* caster = GetCastingObject())
        {
            m_targets.setSource(caster->GetX(), caster->GetY(), caster->GetZ());
            if (!(m_targets.m_targetMask & TARGET_FLAG_DEST_LOCATION))
                m_targets.setDestination(
                    caster->GetX(), caster->GetY(), caster->GetZ());
        }
        break;
    }
    case TARGET_ALL_HOSTILE_UNITS_AROUND_CASTER:
        FillAreaTargets(
            targetUnitMap, radius, PUSH_SELF_CENTER, SPELL_TARGETS_HOSTILE);
        break;
    case TARGET_ALL_FRIENDLY_UNITS_AROUND_CASTER:
        // selected friendly units (for casting objects) around casting object
        FillAreaTargets(targetUnitMap, radius, PUSH_SELF_CENTER,
            SPELL_TARGETS_FRIENDLY, GetCastingObject());

        switch (m_spellInfo->Id)
        {
        case 33048: // Wrath of the Astromancer
        {
            // Targets closest player
            Unit* target = nullptr;
            float dist = 50000.0f;
            for (auto& elem : targetUnitMap)
            {
                if (elem == m_caster)
                    continue;
                float d = (elem)->GetDistance(m_caster);
                if (d < dist)
                {
                    dist = d;
                    target = elem;
                }
            }
            if (target)
            {
                targetUnitMap.clear();
                targetUnitMap.push_back(target);
            }
            break;
        }
        case 39090: // Mechanar Charges
        case 39093:
        {
            // Positve & Negative charge in Mechanar only targets those friendly
            // units that have the opposite charge on them
            bool positive = (m_spellInfo->Id == 39090) ? true : false;
            for (auto itr = targetUnitMap.begin(); itr != targetUnitMap.end();)
            {
                if ((*itr)->has_aura(positive ? 39091 : 39088))
                {
                    ++itr;
                    continue;
                }

                itr = targetUnitMap.erase(itr);
            }
            break;
        }
        default:
            break;
        }
        break;

    case TARGET_ALL_FRIENDLY_UNITS_IN_AREA:
        FillAreaTargets(
            targetUnitMap, radius, PUSH_DEST_CENTER, SPELL_TARGETS_FRIENDLY);
        break;
    // TARGET_SINGLE_PARTY means that the spells can only be casted on a party
    // member and not on the caster (some seals, fire shield from imp, etc..)
    case TARGET_SINGLE_PARTY:
    {
        Unit* target = m_targets.getUnitTarget();
        // Those spells apparently can't be casted on the caster.
        if (target && target != m_caster)
        {
            // Can only be casted on group's members or its pets
            Group* pGroup = nullptr;

            Unit* owner = m_caster->GetCharmerOrOwner();
            Unit* targetOwner = target->GetCharmerOrOwner();
            if (owner)
            {
                if (owner->GetTypeId() == TYPEID_PLAYER)
                {
                    if (target == owner)
                    {
                        targetUnitMap.push_back(target);
                        break;
                    }
                    pGroup = ((Player*)owner)->GetGroup();
                }
            }
            else if (m_caster->GetTypeId() == TYPEID_PLAYER)
            {
                if (targetOwner == m_caster &&
                    target->GetTypeId() == TYPEID_UNIT &&
                    ((Creature*)target)->IsPet())
                {
                    targetUnitMap.push_back(target);
                    break;
                }
                pGroup = ((Player*)m_caster)->GetGroup();
            }

            if (pGroup)
            {
                // Our target can also be a player's pet who's grouped with us
                // or our pet. But can't be controlled player
                if (targetOwner)
                {
                    if (targetOwner->GetTypeId() == TYPEID_PLAYER &&
                        target->GetTypeId() == TYPEID_UNIT &&
                        (((Creature*)target)->IsPet()) &&
                        target->GetOwnerGuid() ==
                            targetOwner->GetObjectGuid() &&
                        pGroup->IsMember(
                            ((Player*)targetOwner)->GetObjectGuid()))
                    {
                        targetUnitMap.push_back(target);
                    }
                }
                // 1Our target can be a player who is on our group
                else if (target->GetTypeId() == TYPEID_PLAYER &&
                         pGroup->IsMember(((Player*)target)->GetObjectGuid()))
                {
                    targetUnitMap.push_back(target);
                }
            }
        }
        break;
    }
    case TARGET_GAMEOBJECT:
        if (m_targets.getGOTarget())
            AddGOTarget(m_targets.getGOTarget(), effIndex);
        break;
    case TARGET_IN_FRONT_OF_CASTER:
    {
        bool in_front =
            !m_spellInfo->HasAttribute(SPELL_ATTR_CUSTOM_REVERSE_DIRECTION);
        FillAreaTargets(targetUnitMap, radius,
            in_front ? PUSH_IN_FRONT : PUSH_IN_BACK, SPELL_TARGETS_AOE_DAMAGE);
        break;
    }
    case TARGET_LARGE_FRONTAL_CONE:
    {
        bool in_front =
            !m_spellInfo->HasAttribute(SPELL_ATTR_CUSTOM_REVERSE_DIRECTION);
        FillAreaTargets(targetUnitMap, radius,
            in_front ? PUSH_IN_FRONT_90 : PUSH_IN_BACK_90,
            SPELL_TARGETS_AOE_DAMAGE);
        break;
    }
    case TARGET_NARROW_FRONTAL_CONE:
    {
        bool in_front =
            !m_spellInfo->HasAttribute(SPELL_ATTR_CUSTOM_REVERSE_DIRECTION);
        FillAreaTargets(targetUnitMap, radius,
            in_front ? PUSH_IN_FRONT_15 : PUSH_IN_BACK_15,
            SPELL_TARGETS_AOE_DAMAGE);
        break;
    }
    case TARGET_DUELVSPLAYER:
    {
        Unit* target = m_targets.getUnitTarget();
        if (target)
        {
            if (m_caster->IsFriendlyTo(target))
            {
                targetUnitMap.push_back(target);
            }
            else
            {
                if (Unit* pUnitTarget = m_caster->SelectMagnetTarget(
                        m_targets.getUnitTarget(), this, effIndex))
                {
                    m_targets.setUnitTarget(pUnitTarget);
                    targetUnitMap.push_back(pUnitTarget);
                }
            }
        }
        break;
    }
    case TARGET_GAMEOBJECT_ITEM:
        if (m_targets.getGOTargetGuid())
            AddGOTarget(m_targets.getGOTarget(), effIndex);
        else if (m_targets.getItemTarget())
            AddItemTarget(m_targets.getItemTarget(), effIndex);
        break;
    case TARGET_MASTER:
        if (Unit* owner = m_caster->GetCharmerOrOwner())
            targetUnitMap.push_back(owner);
        break;
    case TARGET_ALL_ENEMY_IN_AREA_CHANNELED:
        // targets the ground, not the units in the area
        if (m_spellInfo->Effect[effIndex] != SPELL_EFFECT_PERSISTENT_AREA_AURA)
            FillAreaTargets(targetUnitMap, radius, PUSH_DEST_CENTER,
                SPELL_TARGETS_AOE_DAMAGE);
        break;
    case TARGET_MINION:
        if (m_spellInfo->Effect[effIndex] != SPELL_EFFECT_DUEL)
            targetUnitMap.push_back(m_caster);
        break;
    case TARGET_SINGLE_ENEMY:
    {
        if (Unit* pUnitTarget = m_caster->SelectMagnetTarget(
                m_targets.getUnitTarget(), this, effIndex))
        {
            m_targets.setUnitTarget(pUnitTarget);
            targetUnitMap.push_back(pUnitTarget);
        }
        break;
    }
    case TARGET_AREAEFFECT_PARTY:
    {
        Unit* owner = m_caster->GetCharmerOrOwner();
        Player* pTarget = nullptr;

        if (owner)
        {
            targetUnitMap.push_back(m_caster);
            if (owner->GetTypeId() == TYPEID_PLAYER)
                pTarget = (Player*)owner;
        }
        else if (m_caster->GetTypeId() == TYPEID_PLAYER)
        {
            if (Unit* target = m_targets.getUnitTarget())
            {
                if (target->GetTypeId() != TYPEID_PLAYER)
                {
                    if (((Creature*)target)->IsPet())
                    {
                        Unit* targetOwner = target->GetOwner();
                        if (targetOwner->GetTypeId() == TYPEID_PLAYER)
                            pTarget = (Player*)targetOwner;
                    }
                }
                else
                    pTarget = (Player*)target;
            }
        }

        Group* group =
            pTarget && pTarget->duel == nullptr ? pTarget->GetGroup() : nullptr;

        if (group)
        {
            uint8 subgroup = pTarget->GetSubGroup();

            for (auto member : group->members(true))
            {
                // IsHostileTo check duel and controlled by enemy
                if (member->GetSubGroup() == subgroup &&
                    !m_caster->IsHostileTo(member))
                {
                    if (member->isDead() &&
                        !m_spellInfo->HasAttribute(
                            SPELL_ATTR_EX3_DEATH_PERSISTENT))
                        continue;

                    if (member->duel != nullptr)
                        continue;

                    if (pTarget->IsWithinDistInMap(member, radius))
                        targetUnitMap.push_back(member);

                    if (Pet* pet = member->GetPet())
                        if (pTarget->IsWithinDistInMap(pet, radius))
                            targetUnitMap.push_back(pet);
                }
            }
        }
        else if (owner)
        {
            if (m_caster->IsWithinDistInMap(owner, radius))
                targetUnitMap.push_back(owner);
        }
        else if (pTarget)
        {
            targetUnitMap.push_back(pTarget);

            if (Pet* pet = pTarget->GetPet())
                if (m_caster->IsWithinDistInMap(pet, radius))
                    targetUnitMap.push_back(pet);
        }
        break;
    }
    case TARGET_SCRIPT:
    {
        if (m_targets.getUnitTarget())
        {
            m_scriptTarget = true;
            targetUnitMap.push_back(m_targets.getUnitTarget());
        }
        if (m_targets.getItemTarget())
            AddItemTarget(m_targets.getItemTarget(), effIndex);
        break;
    }
    case TARGET_SELF_FISHING:
        targetUnitMap.push_back(m_caster);
        break;
    case TARGET_CHAIN_HEAL:
    {
        Unit* pUnitTarget = m_targets.getUnitTarget();
        if (!pUnitTarget)
            break;

        if (EffectChainTarget <= 1)
            targetUnitMap.push_back(pUnitTarget);
        else
        {
            unMaxTargets = EffectChainTarget;
            float max_range =
                radius + unMaxTargets * m_spellInfo->bounce_radius;

            UnitList tempTargetUnitMap;

            FillAreaTargets(tempTargetUnitMap, max_range, PUSH_SELF_CENTER,
                SPELL_TARGETS_FRIENDLY);

            if (m_caster != pUnitTarget &&
                std::find(tempTargetUnitMap.begin(), tempTargetUnitMap.end(),
                    m_caster) == tempTargetUnitMap.end())
                tempTargetUnitMap.push_front(m_caster);

            tempTargetUnitMap.sort(TargetDistanceOrderNear(pUnitTarget));

            if (tempTargetUnitMap.empty())
                break;

            if (*tempTargetUnitMap.begin() == pUnitTarget)
                tempTargetUnitMap.erase(tempTargetUnitMap.begin());

            targetUnitMap.push_back(pUnitTarget);
            uint32 t = unMaxTargets - 1;
            Unit* prev = pUnitTarget;
            auto next = tempTargetUnitMap.begin();

            while (t && next != tempTargetUnitMap.end())
            {
                // Check so target we're bouncing to is a valid target, from a
                // duelling PoV
                Player* owner =
                    (*next)->GetCharmerOrOwnerPlayerOrPlayerItself();
                if (owner && owner->duel && owner != m_caster)
                {
                    next = targetUnitMap.erase(next);
                    continue;
                }

                if (!prev->IsWithinDist(*next, m_spellInfo->bounce_radius))
                    break;

                if (!prev->IsWithinWmoLOSInMap(*next))
                {
                    ++next;
                    continue;
                }

                if ((*next)->GetHealth() == (*next)->GetMaxHealth())
                {
                    next = tempTargetUnitMap.erase(next);
                    continue;
                }

                prev = *next;
                targetUnitMap.push_back(prev);
                tempTargetUnitMap.erase(next);
                tempTargetUnitMap.sort(TargetDistanceOrderNear(prev));
                next = tempTargetUnitMap.begin();

                --t;
            }
        }
        break;
    }
    case TARGET_CURRENT_ENEMY_COORDINATES:
    {
        Unit* currentTarget = m_targets.getUnitTarget();
        if (currentTarget)
        {
            targetUnitMap.push_back(currentTarget);
            m_targets.setDestination(currentTarget->GetX(),
                currentTarget->GetY(), currentTarget->GetZ());
        }
        break;
    }
    case TARGET_AREAEFFECT_PARTY_AND_CLASS:
    {
        Player* targetPlayer =
            m_targets.getUnitTarget() &&
                    m_targets.getUnitTarget()->GetTypeId() == TYPEID_PLAYER ?
                (Player*)m_targets.getUnitTarget() :
                nullptr;

        Group* group = targetPlayer ? targetPlayer->GetGroup() : nullptr;
        if (group)
        {
            for (auto member : group->members(true))
            {
                // IsHostileTo check duel and controlled by enemy
                if (targetPlayer->IsWithinDistInMap(member, radius) &&
                    targetPlayer->getClass() == member->getClass() &&
                    !m_caster->IsHostileTo(member))
                {
                    targetUnitMap.push_back(member);
                }
            }
        }
        else if (m_targets.getUnitTarget())
            targetUnitMap.push_back(m_targets.getUnitTarget());
        break;
    }
    case TARGET_TABLE_X_Y_Z_COORDINATES:
    {
        SpellTargetPosition const* st =
            sSpellMgr::Instance()->GetSpellTargetPosition(m_spellInfo->Id);
        if (st)
        {
            // teleport spells are handled in another way
            if (m_spellInfo->Effect[effIndex] == SPELL_EFFECT_TELEPORT_UNITS)
                break;
            if (st->target_mapId == m_caster->GetMapId())
                m_targets.setDestination(
                    st->target_X, st->target_Y, st->target_Z);
            else
                logging.error(
                    "SPELL: wrong map (%u instead %u) target coordinates for "
                    "spell ID %u",
                    st->target_mapId, m_caster->GetMapId(), m_spellInfo->Id);
        }
        else
            logging.error("SPELL: unknown target coordinates for spell ID %u",
                m_spellInfo->Id);
        break;
    }
    case TARGET_INFRONT_OF_VICTIM:
    case TARGET_BEHIND_VICTIM:
    case TARGET_RIGHT_FROM_VICTIM:
    case TARGET_LEFT_FROM_VICTIM:
    {
        Unit* pTarget = nullptr;

        // explicit cast data from client or server-side cast
        // some spell at client send caster
        if (m_targets.getUnitTarget() && m_targets.getUnitTarget() != m_caster)
            pTarget = m_targets.getUnitTarget();
        else if (m_caster->getVictim())
            pTarget = m_caster->getVictim();
        else if (m_caster->GetTypeId() == TYPEID_PLAYER)
            pTarget = ObjectAccessor::GetUnit(
                *m_caster, ((Player*)m_caster)->GetSelectionGuid());

        if (pTarget)
        {
            float angle = 0.0f;
            float dist = radius ? radius : CONTACT_DISTANCE;
            dist += pTarget->GetObjectBoundingRadius() +
                    m_caster->GetObjectBoundingRadius();

            switch (targetMode)
            {
            case TARGET_INFRONT_OF_VICTIM:
                break;
            case TARGET_BEHIND_VICTIM:
                angle = M_PI_F;
                break;
            case TARGET_RIGHT_FROM_VICTIM:
                angle = -M_PI_F / 2;
                break;
            case TARGET_LEFT_FROM_VICTIM:
                angle = M_PI_F / 2;
                break;
            }

            G3D::Vector3 pos;

            if (m_spellInfo->Id == 36563 &&
                m_caster->GetTypeId() == TYPEID_PLAYER)
                pos =
                    static_cast<Player*>(m_caster)->GetShadowstepPoint(pTarget);
            else
                pos = pTarget->GetPoint(
                    angle, dist, m_caster->GetTypeId() == TYPEID_PLAYER);
            targetUnitMap.push_back(m_caster);
            m_targets.setDestination(pos.x, pos.y, pos.z);
        }
        break;
    }
    case TARGET_DYNAMIC_OBJECT_COORDINATES:
        // if parent spell create dynamic object extract area from it
        if (DynamicObject* dynObj = m_caster->GetDynObject(
                m_triggeredByAuraSpell ? m_triggeredByAuraSpell->Id :
                                         m_spellInfo->Id))
            m_targets.setDestination(
                dynObj->GetX(), dynObj->GetY(), dynObj->GetZ());
        break;

    case TARGET_DYNAMIC_OBJECT_FRONT:
    case TARGET_DYNAMIC_OBJECT_BEHIND:
    case TARGET_DYNAMIC_OBJECT_LEFT_SIDE:
    case TARGET_DYNAMIC_OBJECT_RIGHT_SIDE:
    {
        if (!(m_targets.m_targetMask & TARGET_FLAG_DEST_LOCATION))
        {
            // General override, we don't want to use max spell range here.
            // Note: 0.0 radius is also for index 36. It is possible that 36
            // must be defined as
            // "at the base of", in difference to 0 which appear to be "directly
            // in front of".
            // TODO: some summoned will make caster be half inside summoned
            // object. Need to fix
            // that in the below code (nearpoint vs closepoint, etc).
            if (m_spellInfo->EffectRadiusIndex[effIndex] == 0)
                radius = 0.0f;

            float angle;
            switch (targetMode)
            {
            case TARGET_DYNAMIC_OBJECT_FRONT:
                angle = 0.0f;
                break;
            case TARGET_DYNAMIC_OBJECT_BEHIND:
                angle = M_PI_F;
                break;
            case TARGET_DYNAMIC_OBJECT_LEFT_SIDE:
                angle = M_PI_F / 2;
                break;
            case TARGET_DYNAMIC_OBJECT_RIGHT_SIDE:
                angle = (2 * M_PI_F) - (M_PI_F / 2);
                break;
            }

            auto pos = m_caster->GetPoint(angle, radius, true);
            m_targets.setDestination(pos.x, pos.y, pos.z);
        }

        targetUnitMap.push_back(m_caster);
        break;
    }
    case TARGET_POINT_AT_NORTH:
    case TARGET_POINT_AT_SOUTH:
    case TARGET_POINT_AT_EAST:
    case TARGET_POINT_AT_WEST:
    case TARGET_POINT_AT_NE:
    case TARGET_POINT_AT_NW:
    case TARGET_POINT_AT_SE:
    case TARGET_POINT_AT_SW:
    {
        if (!(m_targets.m_targetMask & TARGET_FLAG_DEST_LOCATION))
        {
            Unit* currentTarget = m_targets.getUnitTarget() ?
                                      m_targets.getUnitTarget() :
                                      m_caster;
            float angle = currentTarget != m_caster ?
                              currentTarget->GetAngle(m_caster) :
                              m_caster->GetO();

            switch (targetMode)
            {
            case TARGET_POINT_AT_NORTH:
                break;
            case TARGET_POINT_AT_SOUTH:
                angle += M_PI_F;
                break;
            case TARGET_POINT_AT_EAST:
                angle -= M_PI_F / 2;
                break;
            case TARGET_POINT_AT_WEST:
                angle += M_PI_F / 2;
                break;
            case TARGET_POINT_AT_NE:
                angle -= M_PI_F / 4;
                break;
            case TARGET_POINT_AT_NW:
                angle += M_PI_F / 4;
                break;
            case TARGET_POINT_AT_SE:
                angle -= 3 * M_PI_F / 4;
                break;
            case TARGET_POINT_AT_SW:
                angle += 3 * M_PI_F / 4;
                break;
            }

            auto pos = currentTarget->GetPoint(angle, radius);
            m_targets.setDestination(pos.x, pos.y, pos.z);
        }
        break;
    }
    case TARGET_EFFECT_SELECT:
    {
        // add here custom effects that need default target.
        // FOR EVERY TARGET TYPE THERE IS A DIFFERENT FILL!!
        switch (m_spellInfo->Effect[effIndex])
        {
        case SPELL_EFFECT_DUMMY:
        {
            switch (m_spellInfo->Id)
            {
            case 20577: // Cannibalize
            {
                SpellRangeEntry const* srange =
                    sSpellRangeStore.LookupEntry(m_spellInfo->rangeIndex);
                float max_range = GetSpellMaxRange(srange);

                // Check for nearby Player Corpse
                auto result =
                    maps::visitors::yield_single<WorldObject, Corpse>{}(
                        m_caster, max_range, [this](Corpse* c)
                        {
                            Player* owner =
                                ObjectAccessor::FindPlayer(c->GetOwnerGuid());

                            if (!owner || m_caster->IsFriendlyTo(owner))
                                return false;

                            return true;
                        });

                // Check for nearby dead Player
                if (!result)
                {
                    result =
                        maps::visitors::yield_single<WorldObject, Player>{}(
                            m_caster, max_range, [this](Player* p)
                            {
                                return p->isDead() &&
                                       !m_caster->IsFriendlyTo(p);
                            });
                }

                // Check for nearby dead humanoid or undead Creatures
                if (!result)
                {
                    result = maps::visitors::yield_single<WorldObject, Creature,
                        TemporarySummon, SpecialVisCreature>{}(m_caster,
                        max_range, [this](Creature* c)
                        {
                            return c->isDead() && !m_caster->IsFriendlyTo(c) &&
                                   (c->GetCreatureTypeMask() &
                                       CREATURE_TYPEMASK_HUMANOID_OR_UNDEAD) !=
                                       0;
                        });
                }

                if (result)
                {
                    switch (result->GetTypeId())
                    {
                    case TYPEID_UNIT:
                    case TYPEID_PLAYER:
                        targetUnitMap.push_back((Unit*)result);
                        break;
                    case TYPEID_CORPSE:
                        m_targets.setCorpseTarget((Corpse*)result);
                        if (Player* owner = ObjectAccessor::FindPlayer(
                                ((Corpse*)result)->GetOwnerGuid()))
                            targetUnitMap.push_back(owner);
                        break;
                    }
                }
                else
                {
                    // clear cooldown at fail
                    if (m_caster->GetTypeId() == TYPEID_PLAYER)
                        ((Player*)m_caster)
                            ->RemoveSpellCooldown(m_spellInfo->Id, true);
                    SendCastResult(SPELL_FAILED_NO_EDIBLE_CORPSES);
                    finish(false);
                }
                break;
            }
            default:
                if (m_targets.getUnitTarget())
                    targetUnitMap.push_back(m_targets.getUnitTarget());
                break;
            }
            // Add AoE target-mask to self, if no target-dest provided already
            if ((m_targets.m_targetMask & TARGET_FLAG_DEST_LOCATION) == 0)
                m_targets.setDestination(
                    m_caster->GetX(), m_caster->GetY(), m_caster->GetZ());
            break;
        }
        case SPELL_EFFECT_BIND:
        case SPELL_EFFECT_RESURRECT:
        case SPELL_EFFECT_PARRY:
        case SPELL_EFFECT_BLOCK:
        case SPELL_EFFECT_CREATE_ITEM:
        case SPELL_EFFECT_WEAPON:
        case SPELL_EFFECT_TRIGGER_SPELL:
        case SPELL_EFFECT_TRIGGER_MISSILE:
        case SPELL_EFFECT_LEARN_SPELL:
        case SPELL_EFFECT_SKILL_STEP:
        case SPELL_EFFECT_PROFICIENCY:
        case SPELL_EFFECT_SUMMON_OBJECT_WILD:
        case SPELL_EFFECT_SELF_RESURRECT:
        case SPELL_EFFECT_REPUTATION:
        case SPELL_EFFECT_SEND_TAXI:
            if (m_targets.getUnitTarget())
                targetUnitMap.push_back(m_targets.getUnitTarget());
            // Triggered spells have additional spell targets - cast them even
            // if no explicit unit target is given (required for spell 50516 for
            // example)
            else if (m_spellInfo->Effect[effIndex] ==
                     SPELL_EFFECT_TRIGGER_SPELL)
                targetUnitMap.push_back(m_caster);
            break;
        case SPELL_EFFECT_SUMMON_PLAYER:
        {
            if (m_targets.getUnitTargetGuid() == m_caster->GetObjectGuid())
            {
                if (m_caster->GetTypeId() == TYPEID_PLAYER &&
                    ((Player*)m_caster)->GetSelectionGuid())
                    if (Player* target = sObjectMgr::Instance()->GetPlayer(
                            ((Player*)m_caster)->GetSelectionGuid()))
                        targetUnitMap.push_back(target);
            }
            else
            {
                if (Player* player =
                        dynamic_cast<Player*>(m_targets.getUnitTarget()))
                    targetUnitMap.push_back(player);
            }
            break;
        }
        case SPELL_EFFECT_RESURRECT_NEW:
            if (m_targets.getUnitTarget())
                targetUnitMap.push_back(m_targets.getUnitTarget());
            if (m_targets.getCorpseTargetGuid())
            {
                if (Corpse* corpse = m_caster->GetMap()->GetCorpse(
                        m_targets.getCorpseTargetGuid()))
                    if (Player* owner =
                            ObjectAccessor::FindPlayer(corpse->GetOwnerGuid()))
                        targetUnitMap.push_back(owner);
            }
            break;
        case SPELL_EFFECT_TELEPORT_UNITS:
        case SPELL_EFFECT_SUMMON:
        case SPELL_EFFECT_SUMMON_CHANGE_ITEM:
        case SPELL_EFFECT_TRANS_DOOR:
        case SPELL_EFFECT_ADD_FARSIGHT:
        case SPELL_EFFECT_STUCK:
        case SPELL_EFFECT_DESTROY_ALL_TOTEMS:
        case SPELL_EFFECT_SKILL:
            targetUnitMap.push_back(m_caster);
            break;
        case SPELL_EFFECT_PERSISTENT_AREA_AURA:
            if (Unit* currentTarget = m_targets.getUnitTarget())
                m_targets.setDestination(currentTarget->GetX(),
                    currentTarget->GetY(), currentTarget->GetZ());
            break;
        case SPELL_EFFECT_LEARN_PET_SPELL:
            if (Pet* pet = m_caster->GetPet())
                targetUnitMap.push_back(pet);
            break;
        case SPELL_EFFECT_ENCHANT_ITEM:
        case SPELL_EFFECT_ENCHANT_ITEM_TEMPORARY:
        case SPELL_EFFECT_DISENCHANT:
        case SPELL_EFFECT_FEED_PET:
        case SPELL_EFFECT_PROSPECTING:
            if (m_targets.getItemTarget())
                AddItemTarget(m_targets.getItemTarget(), effIndex);
            break;
        case SPELL_EFFECT_APPLY_AURA:
            switch (m_spellInfo->EffectApplyAuraName[effIndex])
            {
            case SPELL_AURA_ADD_FLAT_MODIFIER: // some spell mods auras have 0
                                               // target modes instead expected
                                               // TARGET_SELF(1) (and present
                                               // for other ranks for same spell
                                               // for example)
            case SPELL_AURA_ADD_PCT_MODIFIER:
                targetUnitMap.push_back(m_caster);
                break;
            default: // apply to target in other case
                if (m_targets.getUnitTarget())
                    targetUnitMap.push_back(m_targets.getUnitTarget());
                break;
            }
            break;
        case SPELL_EFFECT_APPLY_AREA_AURA_PARTY:
            // AreaAura
            if ((m_spellInfo->Attributes ==
                    (SPELL_ATTR_NOT_SHAPESHIFT | SPELL_ATTR_UNK18 |
                        SPELL_ATTR_CASTABLE_WHILE_MOUNTED |
                        SPELL_ATTR_CASTABLE_WHILE_SITTING)) ||
                (m_spellInfo->Attributes == SPELL_ATTR_NOT_SHAPESHIFT))
                SetTargetMap(effIndex, TARGET_AREAEFFECT_PARTY, targetUnitMap);
            break;
        case SPELL_EFFECT_SKIN_PLAYER_CORPSE:
            if (m_targets.getUnitTarget())
                targetUnitMap.push_back(m_targets.getUnitTarget());
            else if (m_targets.getCorpseTargetGuid())
            {
                if (Corpse* corpse = m_caster->GetMap()->GetCorpse(
                        m_targets.getCorpseTargetGuid()))
                    if (Player* owner =
                            ObjectAccessor::FindPlayer(corpse->GetOwnerGuid()))
                        targetUnitMap.push_back(owner);
            }
            break;
        default:
            break;
        }
        break;
    }
    default:
        // logging.error( "SPELL: Unknown implicit target (%u) for
        // spell ID %u", targetMode, m_spellInfo->Id );
        break;
    }

    if (m_spellInfo->HasAttribute(SPELL_ATTR_EX_CANT_TARGET_SELF))
        targetUnitMap.remove(m_caster);

    // TODO: Make these two into tidier, easier to read, code
    if (m_spellInfo->HasAttribute(SPELL_ATTR_EX3_ONLY_TARGET_PLAYERS))
    {
        for (auto itr = targetUnitMap.begin(); itr != targetUnitMap.end();)
        {
            if ((*itr)->GetTypeId() != TYPEID_PLAYER)
                itr = targetUnitMap.erase(itr);
            else
                ++itr;
        }
    }
    if (m_spellInfo->HasAttribute(SPELL_ATTR_EX6_CANT_TARGET_CROWD_CONTROLLED))
    {
        auto itr = targetUnitMap.begin();
        std::advance(itr, 1); // Skip main target
        while (itr != targetUnitMap.end())
        {
            // TODO: Make this less fugly
            bool erase = false;
            for (const auto& elem :
                (*itr)->GetAurasByType(SPELL_AURA_MOD_CONFUSE))
                if ((elem)->GetSpellProto()->AuraInterruptFlags &
                    AURA_INTERRUPT_FLAG_DAMAGE)
                {
                    erase = true;
                    break;
                }
            for (const auto& elem : (*itr)->GetAurasByType(SPELL_AURA_MOD_STUN))
                if ((elem)->GetSpellProto()->AuraInterruptFlags &
                    AURA_INTERRUPT_FLAG_DAMAGE)
                {
                    erase = true;
                    break;
                }

            if (erase)
                itr = targetUnitMap.erase(itr);
            else
                ++itr;
        }
    }

    if (unMaxTargets && targetUnitMap.size() > unMaxTargets)
    {
        // make sure one unit is always removed per iteration
        uint32 removed_utarget = 0;
        bool prio_utarget = !IsAreaOfEffectSpell(m_spellInfo);
        if (prio_utarget)
        {
            for (UnitList::iterator itr = targetUnitMap.begin(), next;
                 itr != targetUnitMap.end(); itr = next)
            {
                next = itr;
                ++next;
                if (!*itr)
                    continue;
                if ((*itr) == m_targets.getUnitTarget())
                {
                    targetUnitMap.erase(itr);
                    removed_utarget = 1;
                    break;
                }
            }
        }
        // remove random units from the map
        while (targetUnitMap.size() > unMaxTargets - removed_utarget)
        {
            uint32 poz = urand(0, targetUnitMap.size() - 1);
            for (auto itr = targetUnitMap.begin(); itr != targetUnitMap.end();
                 ++itr, --poz)
            {
                if (!*itr)
                    continue;

                if (!poz)
                {
                    targetUnitMap.erase(itr);
                    break;
                }
            }
        }
        // the player's target will always be added to the map
        if (removed_utarget && m_targets.getUnitTarget())
            targetUnitMap.push_back(m_targets.getUnitTarget());
    }
}

static Unit* correct_incomming_pet_target(
    Creature* pet, Unit* current, const SpellEntry* info)
{
    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
        if (info->Effect[i] != SPELL_EFFECT_NONE &&
            info->EffectImplicitTargetA[i] != TARGET_SELF &&
            info->EffectImplicitTargetA[i] != TARGET_MASTER)
            return current;
    return pet;
}

bool Spell::attempt_pet_cast(Creature* pet, const SpellEntry* info,
    SpellCastTargets targets, bool owner_cast)
{
    if (!pet->HasSpell(info->Id) || IsPassiveSpell(info->Id))
        return false;

    if (pet->GetCharmInfo() &&
        pet->GetCharmInfo()->GetGlobalCooldownMgr().HasGlobalCooldown(info))
        return false;

    Unit* target = targets.getUnitTarget(); // can be nullptr

    if (target && owner_cast)
    {
        target = correct_incomming_pet_target(pet, target, info);
        targets.setUnitTarget(target);
    }

    Player* owner = nullptr;
    bool attack_start =
        owner_cast && target &&
        (owner = pet->GetCharmerOrOwnerPlayerOrPlayerItself()) != nullptr &&
        (IsDamagingSpell(info) || TriggersDamagingSpell(info)) &&
        !owner->IsFriendlyTo(target) &&
        !pet->HasAuraType(SPELL_AURA_MOD_POSSESS);

    // If Out of Range/Sight: queue the spell if pet_behavior agrees with that
    if (owner_cast && pet->behavior() && target &&
        !pet->HasSpellCooldown(info->Id) &&
        ((pet->GetDistance(target) >
                GetSpellMaxRange(
                    sSpellRangeStore.LookupEntry(info->rangeIndex)) ||
            !pet->IsWithinLOSInMap(target))))
    {
        if (pet->behavior()->attempt_queue_spell(info, target, attack_start))
            return true;
    }

    // auto turn to target for spell that requires in front, unless possessed
    if (target && info->FacingCasterFlags & SPELL_FACING_FLAG_INFRONT &&
        !pet->HasInArc(M_PI_F, target) &&
        !pet->HasAuraType(SPELL_AURA_MOD_POSSESS))
    {
        pet->SetInFront(target);
    }

    auto spell = new Spell(pet, info, false);

    spell->send_cast_result_to_pet_owner_ = owner_cast;
    spell->pet_cast_ = true;

    spell->prepare(&targets);
    if (spell->m_spellState == SPELL_STATE_FINISHED && !spell->finish_ok_)
        return false;

    if (attack_start)
    {
        if (static_cast<Creature*>(pet)->behavior())
            static_cast<Creature*>(pet)->behavior()->try_attack(target);
        else if (static_cast<Creature*>(pet)->AI())
            static_cast<Creature*>(pet)->AI()->AttackStart(target);

        if (pet->IsPet() && owner_cast &&
            static_cast<Pet*>(pet)->getPetType() == SUMMON_PET &&
            roll_chance_i(10))
        {
            pet->SendPetTalk(PET_TALK_ORDERED_SPELL_CAST);
        }
    }

    if (GetSpellCastTime(spell->m_spellInfo) > 0 ||
        IsChanneledSpell(spell->m_spellInfo))
    {
        pet->clearUnitState(UNIT_STAT_MOVING);
        pet->StopMoving();
    }

    // Reset cast positions, so having moved on StopMoving() won't cancel the
    // spell during next update
    spell->m_castPositionX = pet->GetX();
    spell->m_castPositionY = pet->GetY();
    spell->m_castPositionZ = pet->GetZ();
    spell->m_castOrientation = pet->GetO();

    return true;
}

void Spell::prepare(SpellCastTargets const* targets, Aura* triggeredByAura)
{
    // Is prepare a recall because path-generation has now finished?
    if (m_spellState == SPELL_STATE_CHARGE_SPECIAL)
    {
        if (!path_gen_finished)
            return;
        m_spellState = SPELL_STATE_PREPARING;
        goto charge_special_continue;
    }

    m_targets = *targets;

    m_spellState = SPELL_STATE_PREPARING;

    m_castPositionX = m_caster->GetTransport() ?
                          m_caster->m_movementInfo.transport.pos.x :
                          m_caster->GetX();
    m_castPositionY = m_caster->GetTransport() ?
                          m_caster->m_movementInfo.transport.pos.y :
                          m_caster->GetY();
    m_castPositionZ = m_caster->GetTransport() ?
                          m_caster->m_movementInfo.transport.pos.z :
                          m_caster->GetZ();
    m_castOrientation = m_caster->GetTransport() ?
                            m_caster->m_movementInfo.transport.pos.o :
                            m_caster->GetO();

    if (triggeredByAura)
        m_triggeredByAuraSpell = triggeredByAura->GetSpellProto();

    // create and add update event for this spell
    {
        auto Event = new SpellEvent(this);
        m_caster->m_Events.AddEvent(Event, m_caster->m_Events.CalculateTime(1));
    }

    // Cannot replace a spell that's currently waiting for a path-gen
    if (Spell* spell = m_caster->GetCurrentSpell(GetCurrentContainer()))
    {
        if (spell->waiting_for_path != 0)
        {
            // Don't send an error
            finish(false);
            return;
        }
    }

    // Prevent casting at cast another spell (ServerSide check)
    if (m_caster->IsNonMeleeSpellCasted(false, true, true) && m_cast_count)
    {
        SendCastResult(SPELL_FAILED_SPELL_IN_PROGRESS);
        finish(false);
        return;
    }

    // Fill cost data
    m_powerCost = CalculatePowerCost(m_spellInfo, m_caster, this, m_CastItem);

charge_special_continue:
    SpellCastResult result =
        !pet_cast_ ? CheckCast(true) : CheckPetCast(nullptr);

    // If charge needed time to generate path, we need to wait for the path
    // generation to finish
    if (result == SPELL_CAST_SERVERSIDE_SPECIAL_CHARGE)
    {
        m_spellState = SPELL_STATE_CHARGE_SPECIAL;
        m_caster->SetCurrentCastedSpell(this);
        return;
    }

    if (result != SPELL_CAST_OK &&
        !IsAutoRepeat()) // always cast autorepeat dummy for triggering
    {
        // XXX: Before the code was inclusive, as in all triggered spell errors
        // were stated except those not included
        // I think it's safe to assume that most triggered spells should not
        // report an error, so exclude by default
        // Leaving this change as an XXX for now, in case my assumption is
        // incorrect
        SendCastResult(
            trigger_type_.triggered() ? SPELL_FAILED_DONT_REPORT : result);

        // For disabled while active spells we need to tell the client to drop
        // the cooldown, assuming the spell has no cooldown server-side.
        if (m_caster->GetTypeId() == TYPEID_PLAYER &&
            m_spellInfo->HasAttribute(SPELL_ATTR_DISABLED_WHILE_ACTIVE) &&
            !static_cast<Player*>(m_caster)->HasSpellCooldown(m_spellInfo->Id))
            static_cast<Player*>(m_caster)->SendClearCooldown(
                m_spellInfo->Id, m_caster);

        finish(false);
        return;
    }

    // If auto shot is currently in its prepare fire stage, we need to delay
    // steady shot, but still consume GCD
    if (!trigger_type_.ignore_gcd() && !m_caster->AutoRepeatFirstCast() &&
        m_spellInfo->Id == 34120 &&
        m_caster->getAttackTimer(RANGED_ATTACK) <= 500 &&
        m_caster->GetTypeId() == TYPEID_PLAYER)
    {
        // Prepare a steady shot that will be executed without GCD when auto
        // shot finishes
        // Also consume the GCD now.
        static_cast<Player*>(m_caster)->pending_steady_shot = true;
        TriggerGlobalCooldown();
        finish(false);
        SendCastResult(SPELL_FAILED_DONT_REPORT);
        return;
    }

    // Prepare data for triggers
    prepareDataForTriggerSystem();

    // calculate cast time (calculated after first CheckCast check to prevent
    // charge counting for first CheckCast fail)
    if (trigger_type_.ignore_cast_time() ||
        (trigger_type_.triggered() &&
            ((m_targets.m_targetMask & TARGET_FLAG_TRADE_ITEM) != 0)))
    {
        // FIXME: We should use the trigger type flag for the trade case as well
        m_casttime = 0;
    }
    else
    {
        m_casttime = GetSpellCastTime(m_spellInfo, this);
    }
    m_duration = CalculateSpellDuration(m_spellInfo, m_caster);
    m_durationUnmod = m_duration;
    m_durationMax = m_duration;

    // set timer base at cast time
    ReSetTimer();

    m_stealthedOnCast = m_caster->HasAuraType(SPELL_AURA_MOD_STEALTH);

    // stealth must be removed at cast starting (at show channel bar)
    // skip triggered spell (item equip spell casting and other not explicit
    // character casts/item uses)
    if (!trigger_type_.triggered() && isSpellBreakStealth(m_spellInfo))
        m_caster->remove_auras_on_event(AURA_INTERRUPT_FLAG_CAST);

    // Stop Attack for some spells
    if (m_spellInfo->HasAttribute(SPELL_ATTR_STOP_ATTACK_TARGET))
    {
        bool res = m_caster->AttackStop();
        // If attack stop had no victim we need to manually send the stop swing
        // opcode (in case the player is auto-shooting atm)
        if (!res && m_caster->GetTypeId() == TYPEID_PLAYER)
            static_cast<Player*>(m_caster)
                ->SendAttackSwingCancelAttack(); // tell the client to stop
                                                 // reactivating melee and
                                                 // ranged attacks
    }

    // add non-triggered (with cast time and without)
    if (!trigger_type_.triggered())
    {
        if (m_caster->GetTypeId() == TYPEID_PLAYER && GetCastTime() == 0 &&
            GetCurrentContainer() == CURRENT_GENERIC_SPELL)
            m_instant =
                true; // Only allow generic spells to be processed instantly

        // add to cast type slot
        m_caster->SetCurrentCastedSpell(this);

        // will show cast bar
        SendSpellStart();

        if (!trigger_type_.ignore_gcd())
            TriggerGlobalCooldown();

        // Creatures should keep this target for the entire duration of the
        // spell (Only turn for certain target types)
        if (targets->getUnitTarget() && targets->getUnitTarget() != m_caster &&
            m_caster->GetTypeId() == TYPEID_UNIT &&
            !((Creature*)m_caster)->IsTotem() &&
            (m_spellInfo->HasTargetType(TARGET_CHAIN_DAMAGE) ||
                m_spellInfo->HasTargetType(TARGET_SINGLE_FRIEND) ||
                m_spellInfo->HasTargetType(TARGET_DUELVSPLAYER) ||
                m_spellInfo->HasTargetType(TARGET_CURRENT_ENEMY_COORDINATES)))
            ((Creature*)m_caster)
                ->SetFocusSpellTarget(targets->getUnitTarget(), m_spellInfo);

        // With instant spells we don't save them in a current spells slot,
        // instead we execute them right away
        if (m_instant)
            cast(true);
    }
    // execute triggered without cast time explicitly in call point
    else if (m_timer == 0)
    {
        // Triggered channeled spells need to be added to current spells for
        // proper interrupt handling
        // Exclude triggered spells with no duration (incorrectly marked by
        // blizzard? seems like an oxymoron to have a no-duration channeled
        // spell)
        if (GetCurrentContainer() == CURRENT_CHANNELED_SPELL &&
            m_spellInfo->DurationIndex > 1)
            m_caster->SetCurrentCastedSpell(this);

        cast(true);
    }
    // else triggered with cast time will execute execute at next tick or later
    // without adding to cast type slot
    // will not show cast bar but will show effects at casting time etc
}

void Spell::cancel(Spell* replacedBy /* = NULL */)
{
    if (m_spellState == SPELL_STATE_FINISHED)
        return;

    // channeled spells don't display interrupted message even if they are
    // interrupted, possible other cases with no "Interrupted" message
    bool sendInterrupt =
        IsChanneledSpell(m_spellInfo) || IsAutoRepeat() ? false : true;
    if (ignore_interrupt_) // Are we forced to ignore interrupt by some external
                           // source?
        sendInterrupt = false;

    m_autoRepeat = false;
    switch (m_spellState)
    {
    case SPELL_STATE_PREPARING:
        CancelGlobalCooldown(true);

        // The client is written by morons, and it activates a completely
        // self-made up
        // infinite cooldown when you begin casting Ritual of Doom, and then it
        // expects
        // the server to remvoe it. Whomever designed this is a bad person,
        // surely.
        if (m_spellInfo->Id == 18540 && m_caster->GetTypeId() == TYPEID_PLAYER)
            static_cast<Player*>(m_caster)->RemoveSpellCooldown(
                18540, true); // Will not remove CD (there is none), but will
                              // send packet anyway

    //(no break)
    case SPELL_STATE_DELAYED:
    {
        SendInterrupted(0);

        if (sendInterrupt)
            SendCastResult(SPELL_FAILED_INTERRUPTED);
    }
    break;

    case SPELL_STATE_CASTING:
    {
        m_caster->remove_auras(m_spellInfo->Id, [this](AuraHolder* holder)
            {
                return holder->GetCasterGuid() == m_caster->GetObjectGuid();
            });

        for (TargetList::const_iterator ihit = m_UniqueTargetInfo.begin();
             ihit != m_UniqueTargetInfo.end(); ++ihit)
        {
            if (ihit->missCondition == SPELL_MISS_NONE)
            {
                Unit* unit =
                    m_caster->GetObjectGuid() == (*ihit).targetGUID ?
                        m_caster :
                        ObjectAccessor::GetUnit(*m_caster, ihit->targetGUID);
                if (unit && unit->isAlive())
                    unit->remove_auras(m_spellInfo->Id,
                        [this](AuraHolder* holder)
                        {
                            return holder->GetCasterGuid() ==
                                   m_caster->GetObjectGuid();
                        });
            }
        }

        // Do not send interrupt and update if we're replaced by another
        // channeled spell
        if (!(replacedBy && IsChanneledSpell(replacedBy->m_spellInfo)))
        {
            SendChannelUpdate(0);
            SendInterrupted(0);
        }

        if (sendInterrupt)
            SendCastResult(SPELL_FAILED_INTERRUPTED);
    }
    break;

    default:
    {
    }
    break;
    }

    // Interrupt pending steady shot if auto shot was cancelled
    if (m_spellInfo->Id == 75 && m_caster->GetTypeId() == TYPEID_PLAYER &&
        static_cast<Player*>(m_caster)->pending_steady_shot)
        static_cast<Player*>(m_caster)->pending_steady_shot = false;

    finish(false);
    m_caster->RemoveDynObject(m_spellInfo->Id);
    m_caster->RemoveGameObject(m_spellInfo->Id, true);
}

void Spell::cast(bool skipCheck)
{
    SetExecutedCurrently(true);

    if (!m_caster->CheckAndIncreaseCastCounter())
    {
        if (m_triggeredByAuraSpell)
            logging.error(
                "Spell %u triggered by aura spell %u too deep in cast chain "
                "for cast. Cast not allowed for prevent overflow stack crash.",
                m_spellInfo->Id, m_triggeredByAuraSpell->Id);
        else
            logging.error(
                "Spell %u too deep in cast chain for cast. Cast not allowed "
                "for prevent overflow stack crash.",
                m_spellInfo->Id);

        SendCastResult(SPELL_FAILED_ERROR);
        finish(false);
        SetExecutedCurrently(false);
        return;
    }

    // update pointers base at GUIDs to prevent access to already nonexistent
    // object
    UpdatePointers();

    // cancel at lost main target unit
    if (!m_targets.getUnitTarget() && m_targets.getUnitTargetGuid() &&
        m_targets.getUnitTargetGuid() != m_caster->GetObjectGuid())
    {
        cancel();
        m_caster->DecreaseCastCounter();
        SetExecutedCurrently(false);
        return;
    }

    if (m_caster->GetTypeId() != TYPEID_PLAYER && m_targets.getUnitTarget() &&
        m_targets.getUnitTarget() != m_caster)
        m_caster->SetInFront(m_targets.getUnitTarget());

    // We need to recaculate the cost on release of the spell (in case a
    // modifier has been applied in the meantime)
    m_powerCost = CalculatePowerCost(m_spellInfo, m_caster, this, m_CastItem);

    SpellCastResult castResult = CheckPower();
    if (castResult != SPELL_CAST_OK)
    {
        SendChannelUpdate(0);
        SendInterrupted(0);
        SendCastResult(castResult);
        finish(false);
        m_caster->DecreaseCastCounter();
        SetExecutedCurrently(false);
        return;
    }

    // triggered cast called from Spell::prepare where it was already checked
    if (!skipCheck)
    {
        castResult = CheckCast(false);
        if (castResult != SPELL_CAST_OK)
        {
            SendChannelUpdate(0);
            SendInterrupted(0);
            SendCastResult(castResult);
            finish(false);
            m_caster->DecreaseCastCounter();
            SetExecutedCurrently(false);
            return;
        }
    }

    // different triggered (for caster) and pre-cast (casted before apply effect
    // to each target) cases
    switch (m_spellInfo->SpellFamilyName)
    {
    case SPELLFAMILY_GENERIC:
    {
        // Bandages
        if (m_caster->GetTypeId() == TYPEID_PLAYER &&
            m_spellInfo->Mechanic == MECHANIC_BANDAGE)
            AddPrecastSpell(11196); // Recently Bandaged
        // Blood Fury (Racial)
        else if (m_spellInfo->SpellIconID == 1662 &&
                 m_spellInfo->AttributesEx & 0x20)
            AddPrecastSpell(23230); // Blood Fury - Healing Reduction
        // The Mortar: Reloaded (NOTE: Taking of casting item must happen BELOW
        // this point)
        else if (m_spellInfo->Id == 13240)
        {
            // 10% chance to backfire
            if (urand(0, 9) < 1)
            {
                // Cast Reload Explode
                m_caster->CastSpell(m_caster, 13239, true);
                return;
            }
        }
        break;
    }
    case SPELLFAMILY_MAGE:
    {
        // Ice Block
        if (m_spellInfo->CasterAuraStateNot == AURA_STATE_HYPOTHERMIA)
            AddPrecastSpell(41425); // Hypothermia
        // Arcane Blast cast speed & mana cost
        else if (m_spellInfo->SpellFamilyFlags & UI64LIT(0x20000000))
            m_caster->CastSpell(m_caster, 36032, true);
        break;
    }
    case SPELLFAMILY_WARRIOR:
        break;
    case SPELLFAMILY_PRIEST:
    {
        // Power Word: Shield
        if (m_spellInfo->CasterAuraStateNot == AURA_STATE_WEAKENED_SOUL ||
            m_spellInfo->TargetAuraStateNot == AURA_STATE_WEAKENED_SOUL)
            AddPrecastSpell(6788); // Weakened Soul

        switch (m_spellInfo->Id)
        {
        case 15237:
            AddTriggeredSpell(23455);
            break; // Holy Nova, rank 1
        case 15430:
            AddTriggeredSpell(23458);
            break; // Holy Nova, rank 2
        case 15431:
            AddTriggeredSpell(23459);
            break; // Holy Nova, rank 3
        case 27799:
            AddTriggeredSpell(27803);
            break; // Holy Nova, rank 4
        case 27800:
            AddTriggeredSpell(27804);
            break; // Holy Nova, rank 5
        case 27801:
            AddTriggeredSpell(27805);
            break; // Holy Nova, rank 6
        case 25331:
            AddTriggeredSpell(25329);
            break; // Holy Nova, rank 7
        default:
            break;
        }
        break;
    }
    case SPELLFAMILY_HUNTER:
    {
        // Aimed Shot
        if (m_spellInfo->SpellFamilyFlags & UI64LIT(0x0000000000020000))
            m_caster->resetAttackTimer(RANGED_ATTACK);
        // Kill Command
        else if (m_spellInfo->Id == 34026)
        {
            if (m_caster->has_aura(
                    37483)) // Improved Kill Command - Item set bonus
                m_caster->CastSpell(
                    m_caster, 37482, true); // Exploited Weakness
        }
        // Bestial Wrath - The Beast Within
        else if (m_spellInfo->Id == 19574)
        {
            if (m_caster->has_aura(34692))
                AddTriggeredSpell(34471);
        }
        break;
    }
    case SPELLFAMILY_PALADIN:
    {
        // Divine Shield, Divine Protection, Blessing of Protection or Avenging
        // Wrath
        if (m_spellInfo->CasterAuraStateNot == AURA_STATE_FORBEARANCE ||
            m_spellInfo->TargetAuraStateNot == AURA_STATE_FORBEARANCE)
            AddPrecastSpell(25771); // Forbearance
        break;
    }
    default:
        break;
    }

    // traded items have trade slot instead of guid in m_itemTargetGUID
    // set to real guid to be sent later to the client
    m_targets.updateTradeSlotItem();

    FillTargetMap();

    casted_timestamp_ = WorldTimer::getMSTime();

    if (m_spellState == SPELL_STATE_FINISHED) // stop cast if spell marked as
                                              // finish somewhere in
                                              // FillTargetMap
    {
        m_caster->DecreaseCastCounter();
        SetExecutedCurrently(false);
        return;
    }

    // CAST SPELL
    SendSpellCooldown();

    if (!trigger_type_.triggered() && isSpellBreakStealth(m_spellInfo))
        m_caster->remove_auras_on_event(AURA_INTERRUPT_FLAG_CAST_FINISH);

    // Throw needs to reset ranged attack timer
    if (!IsAutoRepeat() &&
        m_spellInfo->HasAttribute(SPELL_ATTR_EX3_RANGED_ATT_TIMER))
        m_caster->resetAttackTimer(RANGED_ATTACK);

    TakePower();
    // Do not take reagents if spell is "preparation proof" and we're currently
    // preparing
    if (!(m_spellInfo->HasAttribute(SPELL_ATTR_EX5_NO_REAGENT_WHILE_PREP) &&
            m_caster->HasAuraType(SPELL_AURA_ARENA_PREPARATION)))
        TakeReagents(); // we must remove reagents before HandleEffects to allow
                        // place crafted item in same slot
    else
        m_reagentsIgnoredDueToPrepare = true;

    SendCastResult(castResult);
    SendSpellGo(); // we must send smsg_spell_go packet before m_castItem delete
                   // in TakeCastItem()...

    // NOTE: Must call after FillTargetMap()
    InitializeDamageMultipliers();

    auto on_cast_procs = [this]()
    {
        Unit* caster = GetAffectiveCaster();
        if (m_canTrigger && !trigger_type_.triggered() && caster)
        {
            Unit* main_target = m_targets.getUnitTarget();
            if (main_target)
            {
                caster->ProcSpellsOnCast(this, main_target, m_procAttacker,
                    m_procVictim, PROC_EX_NONE, proc_amount(), BASE_ATTACK,
                    m_spellInfo, EXTRA_ATTACK_NONE, 0, m_casttime);
            }
        }
    };

    // Okay, everything is prepared. Now we need to distinguish between
    // immediate and evented delayed spells
    if (m_spellInfo->speed > 0.0f)
    {
        // Remove used for cast item if need (it can be already NULL after
        // TakeReagents call
        // in case delayed spell remove item at cast delay start
        TakeCastItem();

        // fill initial spell damage from caster for delayed casted spells
        for (auto& elem : m_UniqueTargetInfo)
            HandleDelayedSpellLaunch(&(elem));

        // Okay, maps created, now prepare flags
        m_immediateHandled = false;
        m_spellState = SPELL_STATE_DELAYED;
        SetDelayStart(0);

        // Do on cast procs after handle delayed
        on_cast_procs();
        // Some talents that proc on crit do so when the spell is casted
        // Handled in Spell::DoAllEffectOnTarget for non-delayed spells
        Unit* caster = GetAffectiveCaster();
        if (m_canTrigger && !trigger_type_.triggered() && caster)
        {
            for (auto& elem : m_UniqueTargetInfo)
            {
                if ((elem.HitInfo & SPELL_HIT_TYPE_CRIT) == 0)
                    continue;
                if (auto unit = caster->GetMap()->GetUnit(elem.targetGUID))
                {
                    caster->ProcDamageAndSpell(unit, m_procAttacker, 0,
                        PROC_EX_ON_CAST_CRIT, proc_amount(), BASE_ATTACK,
                        m_spellInfo);
                }
            }
        }
    }
    else
    {
        // Do on cast procs before handle immediate
        on_cast_procs();

        // Immediate spell, no big deal
        handle_immediate();
    }

    // Caster ends up in combat as soon as the spell leaves his hands (not the
    // target, though)
    if (!trigger_type_.triggered() && m_caster->GetTypeId() == TYPEID_PLAYER &&
        m_targets.getUnitTarget() &&
        m_targets.getUnitTarget()->player_or_pet() &&
        m_caster->IsHostileTo(m_targets.getUnitTarget()) &&
        !m_spellInfo->HasAttribute(SPELL_ATTR_EX3_NO_INITIAL_AGGRO) &&
        !m_spellInfo->HasAttribute(SPELL_ATTR_EX_NO_THREAT) &&
        !m_spellInfo->HasAttribute(SPELL_ATTR_EX3_DONT_AFFECT_COMBAT))
    {
        m_caster->SetInCombatState(true);
    }

    m_caster->DecreaseCastCounter();
    SetExecutedCurrently(false);

    if (IsMeleeAttackResetSpell())
    {
        m_caster->resetAttackTimer(BASE_ATTACK);
        if (m_caster->haveOffhandWeapon())
            m_caster->resetAttackTimer(OFF_ATTACK);
        m_caster->ClearQueuedWhiteAttacks();
    }

    // Auto shot: We need to start steady shot if it was started within 500 ms
    // (the "load gun" phase) of auto shot
    if (m_spellInfo->Id == 75 && m_caster->GetTypeId() == TYPEID_PLAYER &&
        static_cast<Player*>(m_caster)->pending_steady_shot)
    {
        if (m_targets.getUnitTarget() && m_targets.getUnitTarget()->isAlive())
            m_caster->CastSpell(m_targets.getUnitTarget(), 34120,
                spell_trigger_type((uint32)TRIGGER_TYPE_IGNORE_GCD));
        static_cast<Player*>(m_caster)->pending_steady_shot = false;
    }

    // HACK: Goblin Rocket Launcher (no main spell, such as 44137 for the MgT
    // boss, exists for the Goblin Rocket Launcher, so a clean fix is not
    // possible)
    if (m_spellInfo->Id == 46567)
        m_caster->CastSpell(m_caster, 13360, true);

    // Deadly Throw's pvp gloves interrupt is instant (at cast, rather than
    // hitting the target)
    if (m_spellInfo->Id == 26679 && m_caster->has_aura(32748))
    {
        // Find the target off the damaging component, he's the one we intend to
        // silence
        for (auto& elem : m_UniqueTargetInfo)
        {
            if (elem.effectMask & 0x1)
            {
                if (Unit* target = m_caster->GetMap()->GetUnit(elem.targetGUID))
                    m_caster->CastSpell(target, 32747, true);
                break;
            }
        }
    }
}

void Spell::handle_immediate()
{
    // process immediate effects (items, ground, etc.) also initialize some
    // variables
    _handle_immediate_phase();

    DoUnitEffects();

    for (auto& elem : m_UniqueGOTargetInfo)
        DoAllEffectOnTarget(&(elem));

    // start channeling if applicable (after _handle_immediate_phase for get
    // persistent effect dynamic object for channel target
    if (IsChanneledSpell(m_spellInfo) && (m_duration > 0 || m_duration == -1))
    {
        m_spellState = SPELL_STATE_CASTING;
        SendChannelStart(m_duration);
        // Channeled spells take cast item right away
        TakeCastItem();
    }

    // spell is finished, perform some last features of the spell here
    _handle_finish_phase();

    if (m_spellState != SPELL_STATE_CASTING)
        finish(true); // successfully finish spell cast (not last in case
                      // autorepeat or channel spell)
}

uint64 Spell::handle_delayed(uint64 t_offset)
{
    uint64 next_time = 0;

    for (auto& elem : m_UniqueTargetInfo)
    {
        if (!elem.processed)
        {
            if (elem.timeDelay <= t_offset)
            {
                uint64 prevTimeDelay = elem.timeDelay;
                elem.timeDelay = 0;

                DoUnitEffect(&elem);

                // Still not processed properly? Needs reprocesssing
                if (!elem.processed && elem.timeDelay > 0)
                    next_time = prevTimeDelay + elem.timeDelay;
            }
            else if (next_time == 0 || elem.timeDelay < next_time)
                next_time = elem.timeDelay;
        }
    }

    // now recheck gameobject targeting correctness
    for (auto& elem : m_UniqueGOTargetInfo)
    {
        if (!elem.processed)
        {
            if (elem.timeDelay <= t_offset)
                DoAllEffectOnTarget(&(elem));
            else if (next_time == 0 || elem.timeDelay < next_time)
                next_time = elem.timeDelay;
        }
    }
    // All targets passed - need finish phase
    if (next_time == 0)
    {
        // Only handle immediate once the spell has finished its travel time
        if (!m_immediateHandled)
        {
            _handle_immediate_phase();
            m_immediateHandled = true;
        }

        // spell is finished, perform some last features of the spell here
        _handle_finish_phase();

        finish(true); // successfully finish spell cast

        // return zero, spell is finished now
        return 0;
    }
    else
    {
        // spell is unfinished, return next execution time
        return next_time;
    }
}

void Spell::_handle_immediate_phase()
{
    HandleDurationDR();

    // handle some immediate features of the spell here
    HandleThreatSpells();

    m_needSpellLog = IsNeedSendToClient();
    for (int j = 0; j < MAX_EFFECT_INDEX; ++j)
    {
        if (m_spellInfo->Effect[j] == 0)
            continue;

        // apply Send Event effect to ground in case empty target lists
        if (m_spellInfo->Effect[j] == SPELL_EFFECT_SEND_EVENT &&
            !HaveTargetsForEffect(SpellEffectIndex(j)))
        {
            HandleEffects(nullptr, nullptr, nullptr, SpellEffectIndex(j));
            continue;
        }

        // Don't do spell log, if is school damage spell
        if (m_spellInfo->Effect[j] == SPELL_EFFECT_SCHOOL_DAMAGE ||
            m_spellInfo->Effect[j] == 0)
            m_needSpellLog = false;
    }

    // initialize Diminishing Returns Data
    m_diminishLevel = DIMINISHING_LEVEL_1;
    m_diminishGroup = DIMINISHING_NONE;

    // process items
    for (auto& elem : m_UniqueItemInfo)
        DoAllEffectOnTarget(&(elem));

    // process ground
    for (int j = 0; j < MAX_EFFECT_INDEX; ++j)
    {
        // persistent area auras and some object summoning effects target only
        // the ground
        if (m_spellInfo->Effect[j] == SPELL_EFFECT_PERSISTENT_AREA_AURA ||
            m_spellInfo->EffectImplicitTargetA[j] ==
                TARGET_AREAEFFECT_GO_AROUND_DEST)
        {
            HandleEffects(nullptr, nullptr, nullptr, SpellEffectIndex(j));
        }
    }
}

void Spell::_handle_finish_phase()
{
    // spell log
    if (m_needSpellLog)
        SendLogExecute();
}

void Spell::SendSpellCooldown()
{
    if (m_caster->GetTypeId() == TYPEID_PLAYER)
    {
        Player* _player = (Player*)m_caster;

        // (1) have infinity cooldown but set at aura apply, (2) passive
        // cooldown at triggering
        if ((m_spellInfo->HasAttribute(SPELL_ATTR_DISABLED_WHILE_ACTIVE) &&
                m_spellInfo->HasEffect(SPELL_EFFECT_APPLY_AURA)) ||
            m_spellInfo->HasAttribute(SPELL_ATTR_PASSIVE))
            return;

        _player->AddSpellAndCategoryCooldowns(
            m_spellInfo, m_CastItem ? m_CastItem->GetEntry() : 0, this);
    }
    else if ((m_caster->hasUnitState(UNIT_STAT_CONTROLLED) ||
                 m_caster->HasAuraType(SPELL_AURA_MOD_CHARM)) &&
             m_caster->GetCharmer() &&
             m_caster->GetCharmer()->GetTypeId() == TYPEID_PLAYER)
    {
        // If spell has a cooldown in DBC, add that
        if (m_spellInfo->CategoryRecoveryTime != 0 ||
            m_spellInfo->RecoveryTime != 0)
        {
            static_cast<Creature*>(m_caster)->AddCreatureSpellCooldown(
                m_spellInfo->Id);
        }
        // Add a 6 sec cooldown to instant spells without a DBC defined one
        else if (m_spellInfo->CastingTimeIndex == 0 ||
                 m_spellInfo->CastingTimeIndex == 1)
        {
            static_cast<Creature*>(m_caster)->_AddCreatureSpellCooldown(
                m_spellInfo->Id, WorldTimer::time_no_syscall() + 6);
        }

        // Also add a global cooldown
        if (auto cminf = static_cast<Creature*>(m_caster)->GetCharmInfo())
            cminf->GetGlobalCooldownMgr().AddGlobalCooldown(m_spellInfo, 1500);

        // FIXME: This is a hack to get cooldowns to display at client
        static_cast<Player*>(m_caster->GetCharmer())->PetSpellInitialize();
    }
    else if (pet_cast_ && m_caster->GetTypeId() == TYPEID_UNIT)
    {
        static_cast<Creature*>(m_caster)->AddCreatureSpellCooldown(
            m_spellInfo->Id);
        if (static_cast<Creature*>(m_caster)->IsPet())
            static_cast<Pet*>(m_caster)->CheckLearning(m_spellInfo->Id);
    }
}

void Spell::update(uint32 difftime)
{
    // update pointers based at it's GUIDs
    UpdatePointers();

    if (m_targets.getUnitTargetGuid() && !m_targets.getUnitTarget())
    {
        cancel();
        return;
    }

    // Channeled single target spells should finish when resisted or evaded
    if (IsChannelActive() && m_spellInfo->MaxAffectedTargets == 0 &&
        !m_UniqueTargetInfo.empty() &&
        ((m_UniqueTargetInfo.front().missCondition == SPELL_MISS_RESIST &&
             !(m_spellInfo->SpellFamilyName == SPELLFAMILY_MAGE &&
                 (m_spellInfo->SpellFamilyFlags &
                     UI64LIT(
                         0x800)))) || // Arcane missiles is exempt when resisted
            m_UniqueTargetInfo.front().missCondition == SPELL_MISS_EVADE))
    {
        cancel();
        return;
    }

    // check if the caster has moved before the spell finished
    float c_x, c_y, c_z;
    if (m_caster->GetTransport())
        m_caster->m_movementInfo.transport.pos.Get(c_x, c_y, c_z);
    else
        m_caster->GetPosition(c_x, c_y, c_z);
    if (m_timer != 0 && (m_castPositionX != c_x || m_castPositionY != c_y ||
                            m_castPositionZ != c_z) &&
        (m_spellInfo->Effect[EFFECT_INDEX_0] != SPELL_EFFECT_STUCK ||
            !m_caster->m_movementInfo.HasMovementFlag(MOVEFLAG_FALLING_UNK1)))
    {
        // Do not cancel for non-player controlled NPCs
        if (m_caster->GetCharmerOrOwnerPlayerOrPlayerItself() != nullptr)
        {
            // chanelled spells
            if (m_spellState == SPELL_STATE_CASTING &&
                m_spellInfo->ChannelInterruptFlags & CHANNEL_FLAG_MOVEMENT)
                cancel();
            // don't cancel for melee, autorepeat, triggered and instant spells
            else if (!IsNextMeleeSwingSpell() && !IsAutoRepeat() &&
                     !trigger_type_.triggered() &&
                     (m_spellInfo->InterruptFlags &
                         SPELL_INTERRUPT_FLAG_MOVEMENT))
                cancel();
        }
    }

    switch (m_spellState)
    {
    case SPELL_STATE_PREPARING:
    {
        if (m_timer)
        {
            if (difftime >= m_timer)
                m_timer = 0;
            else
                m_timer -= difftime;
        }

        if (m_timer == 0 && !IsNextMeleeSwingSpell() && !IsAutoRepeat())
            cast();
    }
    break;
    case SPELL_STATE_CASTING:
    {
        if (m_timer > 0)
        {
            if (m_caster->GetTypeId() == TYPEID_PLAYER)
            {
                // check if player has jumped before the channeling finished
                if (m_spellInfo->ChannelInterruptFlags &
                        CHANNEL_FLAG_MOVEMENT &&
                    ((Player*)m_caster)
                        ->m_movementInfo.HasMovementFlag(MOVEFLAG_GRAVITY))
                    cancel();

                // check for incapacitating player states
                if (m_caster->hasUnitState(UNIT_STAT_CAN_NOT_REACT))
                    cancel();

                // check if player has turned if flag is set
                if (m_spellInfo->ChannelInterruptFlags & CHANNEL_FLAG_TURNING)
                {
                    float ori;
                    if (m_caster->GetTransport())
                        ori = m_caster->m_movementInfo.transport.pos.o;
                    else
                        ori = m_caster->GetO();
                    if (m_castOrientation != ori)
                        cancel();
                }
            }

            // check if there are alive targets left
            if (!IsAliveUnitPresentInTargetList())
            {
                SendChannelUpdate(0);
                finish();
            }

            if (difftime >= m_timer)
                m_timer = 0;
            else
                m_timer -= difftime;
        }

        if (m_timer == 0)
        {
            SendChannelUpdate(0);

            // channeled spell processed independently for quest targeting
            // cast at creature (or GO) quest objectives update at successful
            // cast channel finished
            // ignore autorepeat/melee casts for speed (not exist quest for
            // spells (hm... )
            if (!IsAutoRepeat() && !IsNextMeleeSwingSpell())
            {
                if (Player* p =
                        m_caster->GetCharmerOrOwnerPlayerOrPlayerItself())
                {
                    for (TargetList::const_iterator ihit =
                             m_UniqueTargetInfo.begin();
                         ihit != m_UniqueTargetInfo.end(); ++ihit)
                    {
                        TargetInfo const& target = *ihit;
                        if (!target.targetGUID.IsCreature())
                            continue;

                        Unit* unit =
                            m_caster->GetObjectGuid() == target.targetGUID ?
                                m_caster :
                                ObjectAccessor::GetUnit(
                                    *m_caster, target.targetGUID);
                        if (unit == nullptr)
                            continue;

                        p->RewardPlayerAndGroupAtCast(unit, m_spellInfo->Id);
                    }

                    for (GOTargetList::const_iterator ihit =
                             m_UniqueGOTargetInfo.begin();
                         ihit != m_UniqueGOTargetInfo.end(); ++ihit)
                    {
                        GOTargetInfo const& target = *ihit;

                        GameObject* go = m_caster->GetMap()->GetGameObject(
                            target.targetGUID);
                        if (!go)
                            continue;

                        p->RewardPlayerAndGroupAtCast(go, m_spellInfo->Id);
                    }
                }
            }

            finish();
        }
    }
    break;
    default:
    {
    }
    break;
    }
}

void Spell::finish(bool ok)
{
    if (!m_caster)
        return;

    if (m_spellState == SPELL_STATE_FINISHED)
        return;

    // remove/restore spell mods before m_spellState update
    if (Player* modOwner = m_caster->GetSpellModOwner())
    {
        if (!ok && m_spellState == SPELL_STATE_PREPARING)
            modOwner->ResetSpellModsDueToCanceledSpell(this);
        else if (!ok)
            modOwner->RemoveSpellMods(this);
        // if (ok) => spellMods removed in Spell::DoFinishPhase
    }

    m_spellState = SPELL_STATE_FINISHED;
    finish_ok_ = ok;

    // Ritual-esk spells:
    if (m_caster->GetTypeId() == TYPEID_PLAYER &&
        m_spellInfo->HasAttribute(SPELL_ATTR_DISABLED_WHILE_ACTIVE) &&
        IsChanneledSpell(m_spellInfo))
    {
        // If interrupted preemptively or allowed to run out we refund mats and
        // cooldown
        bool isCastingRitual = m_caster->IsCastingRitual();
        if ((!ok && isCastingRitual) || (ok && m_timer == 0))
        {
            Player* player = (Player*)m_caster;
            player->RemoveSpellCooldown(m_spellInfo->Id, true);
            // Do not refund reagents casted during preparation phase:
            if (!m_reagentsIgnoredDueToPrepare)
                RefundReagents();

            if (isCastingRitual)
            {
                if (GameObject* pRitual = player->GetCastedRitual())
                    pRitual->EndRitual(false);
            }
        }
    }
    // Ritual of Summoning needs to be added as a particular exception
    else if (m_caster->GetTypeId() == TYPEID_PLAYER && m_spellInfo->Id == 46546)
    {
        bool isCastingRitual = m_caster->IsCastingRitual();
        if ((!ok && isCastingRitual) || (ok && m_timer == 0))
        {
            // Give back a soul shard (Do not refund reagents casted during
            // preparation phase, however)
            /* XXX*/
            if (!m_reagentsIgnoredDueToPrepare)
            {
                inventory::transaction trans;
                trans.add(6265, 1);
                static_cast<Player*>(m_caster)->storage().finalize(trans);
            }

            if (isCastingRitual)
            {
                if (GameObject* pRitual = m_caster->GetCastedRitual())
                    pRitual->EndRitual(false);
            }
        }
    }

    if (m_caster->GetTypeId() == TYPEID_UNIT &&
        !((Creature*)m_caster)->IsTotem() &&
        (m_spellInfo->HasTargetType(TARGET_CHAIN_DAMAGE) ||
            m_spellInfo->HasTargetType(TARGET_SINGLE_FRIEND) ||
            m_spellInfo->HasTargetType(TARGET_DUELVSPLAYER) ||
            m_spellInfo->HasTargetType(TARGET_CURRENT_ENEMY_COORDINATES)))
    {
        if (!m_casttime)
        {
            auto info = m_spellInfo;
            auto caster = m_caster;
            m_caster->queue_action(600, [caster, info]()
                {
                    static_cast<Creature*>(caster)->SetFocusSpellTarget(
                        nullptr, info);
                });
        }
        else
        {
            static_cast<Creature*>(m_caster)->SetFocusSpellTarget(
                nullptr, m_spellInfo);
        }
    }

    // Clear cooldown in pet action-bar on failed cast
    if (!ok && pet_cast_ && send_cast_result_to_pet_owner_ &&
        m_caster->GetTypeId() == TYPEID_UNIT &&
        !static_cast<Creature*>(m_caster)->HasSpellCooldown(m_spellInfo->Id))
    {
        if (Player* owner = m_caster->GetCharmerOrOwnerPlayerOrPlayerItself())
            owner->SendClearCooldown(m_spellInfo->Id, m_caster);
    }

    // other code related only to successfully finished spells
    if (!ok)
        return;

    // Cost refunds on miss/dodge/parry/block
    if (m_caster->GetTypeId() == TYPEID_PLAYER)
    {
        for (TargetList::const_iterator ihit = m_UniqueTargetInfo.begin();
             ihit != m_UniqueTargetInfo.end(); ++ihit)
        {
            if (ihit->targetGUID != m_caster->GetObjectGuid() &&
                ihit->missCondition != SPELL_MISS_NONE)
            {
                // HACK: The client does not show miss/dodge/parry etc properly
                // for the main execute spell, here we hack-send the error
                if (m_caster->getClass() == CLASS_WARRIOR &&
                    m_spellInfo->SpellIconID == 1648 &&
                    m_spellInfo->Id != 20647)
                {
                    if (Unit* target =
                            m_caster->GetMap()->GetUnit(ihit->targetGUID))
                    {
                        m_caster->SendSpellMiss(
                            target, 20647, ihit->missCondition);
                    }
                }
                // Spell refunds on miss/dodge/parry/block
                goto refund_spell;
            }
            goto dont_refund_spell;
        }

    refund_spell:
        // Rogue/Druid spells refund
        if ((m_caster->getClass() == CLASS_ROGUE ||
                m_caster->getClass() == CLASS_DRUID) &&
            !m_spellInfo->HasAttribute(SPELL_ATTR_EX_REQ_TARGET_COMBO_POINTS) &&
            m_spellInfo->powerType == POWER_ENERGY)
        {
            m_caster->SetPower(POWER_ENERGY,
                m_caster->GetPower(POWER_ENERGY) + m_powerCost * 0.8);
        }

        // Quick recovery refunds energy when finishing moves miss
        if (m_spellInfo->IsFitToFamily(SPELLFAMILY_ROGUE, UI64LIT(0x9003A0000)))
        {
            auto& al = m_caster->GetAurasByType(SPELL_AURA_DUMMY);
            for (const auto& elem : al)
            {
                if ((elem)->GetId() == 31244 || (elem)->GetId() == 31245)
                {
                    float coeff = elem->GetId() == 31244 ? 0.4 : 0.8;
                    m_caster->SetPower(POWER_ENERGY,
                        m_caster->GetPower(POWER_ENERGY) + m_powerCost * coeff);
                    break;
                }
            }
        }
    dont_refund_spell:
        ;
    }
}

void Spell::SendMessageToSet(WorldPacket&& data)
{
    m_caster->SendMessageToSet(&data, false);
    // Cannot use grid; must get channel update when OOR of one's own camera
    if (m_caster->GetTypeId() == TYPEID_PLAYER)
        static_cast<Player*>(m_caster)->SendDirectMessage(std::move(data));
}

void Spell::SendCastResult(SpellCastResult result)
{
    Unit* send_to = m_caster;

    // Special handling for sending cast result to pets
    if (send_cast_result_to_pet_owner_ && m_caster->GetTypeId() == TYPEID_UNIT)
    {
        if (!m_caster->HasAuraType(SPELL_AURA_MOD_POSSESS))
        {
            m_caster->SendPetCastFail(m_spellInfo->Id, result);
            return;
        }

        if (Player* owner = m_caster->GetCharmerOrOwnerPlayerOrPlayerItself())
            send_to = owner;
    }

    if (send_to->GetTypeId() != TYPEID_PLAYER)
        return;

    if (static_cast<Player*>(send_to)
            ->GetSession()
            ->PlayerLoading()) // don't send cast results at loading time
        return;

    SendCastResult((Player*)send_to, m_spellInfo, m_cast_count, result);
}

void Spell::SendCastResult(Player* caster, SpellEntry const* spellInfo,
    uint8 cast_count, SpellCastResult result)
{
    if (result == SPELL_CAST_OK)
    {
        /*WorldPacket data(SMSG_CLEAR_EXTRA_AURA_INFO, (8+4));
        data << caster->GetPackGUID();
        data << uint32(spellInfo->Id);
        caster->GetSession()->send_packet(std::move(data));*/
        return;
    }

    WorldPacket data(SMSG_CAST_FAILED, (4 + 1 + 1));
    data << uint32(spellInfo->Id);
    data << uint8(result);     // problem
    data << uint8(cast_count); // single cast or multi 2.3 (0/1)
    switch (result)
    {
    case SPELL_FAILED_REQUIRES_SPELL_FOCUS:
        data << uint32(spellInfo->RequiresSpellFocus);
        break;
    case SPELL_FAILED_REQUIRES_AREA:
        // hardcode areas limitation case
        switch (spellInfo->Id)
        {
        case 41617: // Cenarion Mana Salve
        case 41619: // Cenarion Healing Salve
            data << uint32(3905);
            break;
        case 41618: // Bottled Nethergon Energy
        case 41620: // Bottled Nethergon Vapor
            data << uint32(3842);
            break;
        case 45373: // Bloodberry Elixir
            data << uint32(4075);
            break;
        default: // default case
            data << uint32(spellInfo->AreaId);
            break;
        }
        break;
    case SPELL_FAILED_TOTEMS:
        for (int i = 0; i < MAX_SPELL_TOTEMS; ++i)
            if (spellInfo->Totem[i])
                data << uint32(spellInfo->Totem[i]);
        break;
    case SPELL_FAILED_TOTEM_CATEGORY:
        for (int i = 0; i < MAX_SPELL_TOTEM_CATEGORIES; ++i)
            if (spellInfo->TotemCategory[i])
                data << uint32(spellInfo->TotemCategory[i]);
        break;
    case SPELL_FAILED_EQUIPPED_ITEM_CLASS:
        data << uint32(spellInfo->EquippedItemClass);
        data << uint32(spellInfo->EquippedItemSubClassMask);
        data << uint32(spellInfo->EquippedItemInventoryTypeMask);
        break;
    default:
        break;
    }
    caster->GetSession()->send_packet(std::move(data));
}

void Spell::SendSpellStart()
{
    if (!IsNeedSendToClient())
        return;

    uint32 castFlags = CAST_FLAG_UNKNOWN2;
    if ((trigger_type_.triggered() &&
            !m_spellInfo->HasAttribute(SPELL_ATTR_EX2_AUTOREPEAT_FLAG)) ||
        m_triggeredByAuraSpell)
        castFlags |= CAST_FLAG_HIDDEN_COMBATLOG;
    if (IsRangedSpell())
        castFlags |= CAST_FLAG_AMMO;

    WorldPacket data(SMSG_SPELL_START, (8 + 8 + 4 + 4 + 2));
    if (m_CastItem)
        data << m_CastItem->GetPackGUID();
    else
        data << m_caster->GetPackGUID();

    data << m_caster->GetPackGUID();
    data << uint32(m_spellInfo->Id); // spellId
    data << uint8(m_cast_count);     // pending spell cast?
    data << uint16(castFlags);       // cast flags
    data << uint32(m_timer);         // delay?

    data << m_targets;

    if (castFlags & CAST_FLAG_AMMO) // projectile info
        WriteAmmoToPacket(&data);

    SendMessageToSet(std::move(data));
}

void Spell::SendSpellGo()
{
    // not send invisible spell casting
    if (!IsNeedSendToClient())
        return;

    // Set unit target if we have targets but no unit target
    if (m_targets.getUnitTargetGuid().IsEmpty() &&
        !m_UniqueTargetInfo.empty() &&
        (m_spellInfo->Targets & TARGET_FLAG_DEST_LOCATION) == 0)
    {
        for (auto& target : m_UniqueTargetInfo)
            if (auto unit = m_caster->GetMap()->GetUnit(target.targetGUID))
            {
                m_targets.setUnitTarget(unit);
                break;
            }
    }

    uint32 castFlags = CAST_FLAG_UNKNOWN9;
    if ((trigger_type_.triggered() &&
            !m_spellInfo->HasAttribute(SPELL_ATTR_EX2_AUTOREPEAT_FLAG)) ||
        m_triggeredByAuraSpell)
        castFlags |= CAST_FLAG_HIDDEN_COMBATLOG;
    if (IsRangedSpell())
        castFlags |= CAST_FLAG_AMMO; // arrows/bullets visual

    WorldPacket data(SMSG_SPELL_GO, 50); // guess size

    if (m_CastItem)
        data << m_CastItem->GetPackGUID();
    else
        data << m_caster->GetPackGUID();

    // FIXME: DISGUSTING HACK
    // Immolation Trap shouldn't do the cast animation from the hunter
    // P.S. Mangos made me do it
    if ((m_spellInfo->SpellFamilyName == SPELLFAMILY_HUNTER &&
            m_spellInfo->SpellFamilyFlags & 0x4 &&
            m_spellInfo->SpellIconID == 678 && m_targets.getUnitTarget()) ||
        (m_spellInfo->SpellIconID == 137 &&
            m_spellInfo->SpellVisual ==
                5619)) // Throw proximity bomb has the same shit
        data << m_targets.getUnitTarget()->GetPackGUID();
    else
        data << m_caster->GetPackGUID();
    data << uint32(m_spellInfo->Id);         // spellId
    data << uint16(castFlags);               // cast flags
    data << uint32(WorldTimer::getMSTime()); // timestamp

    WriteSpellGoTargets(&data);

    data << m_targets;

    if (castFlags & CAST_FLAG_AMMO) // projectile info
        WriteAmmoToPacket(&data);

    SendMessageToSet(std::move(data));
}

void Spell::WriteAmmoToPacket(WorldPacket* data)
{
    uint32 ammoInventoryType = 0;
    uint32 ammoDisplayID = 0;

    if (m_caster->GetTypeId() == TYPEID_PLAYER)
    {
        Item* pItem = ((Player*)m_caster)->GetWeaponForAttack(RANGED_ATTACK);
        if (pItem)
        {
            ammoInventoryType = pItem->GetProto()->InventoryType;
            if (ammoInventoryType == INVTYPE_THROWN)
                ammoDisplayID = pItem->GetProto()->DisplayInfoID;
            else
            {
                uint32 ammoID =
                    ((Player*)m_caster)->GetUInt32Value(PLAYER_AMMO_ID);
                if (ammoID)
                {
                    ItemPrototype const* pProto =
                        ObjectMgr::GetItemPrototype(ammoID);
                    if (pProto)
                    {
                        ammoDisplayID = pProto->DisplayInfoID;
                        ammoInventoryType = pProto->InventoryType;
                    }
                }
                else if (m_caster->has_aura(
                             46699, SPELL_AURA_DUMMY)) // Requires No Ammo
                {
                    ammoDisplayID = 5996; // normal arrow
                    ammoInventoryType = INVTYPE_AMMO;
                }
            }
        }
    }
    else
    {
        for (uint8 i = 0; i < MAX_VIRTUAL_ITEM_SLOT; ++i)
        {
            // see Creature::SetVirtualItem for structure data
            if (uint32 item_class =
                    m_caster->GetByteValue(UNIT_VIRTUAL_ITEM_INFO + (i * 2) + 0,
                        VIRTUAL_ITEM_INFO_0_OFFSET_CLASS))
            {
                if (item_class == ITEM_CLASS_WEAPON)
                {
                    switch (m_caster->GetByteValue(
                        UNIT_VIRTUAL_ITEM_INFO + (i * 2) + 0,
                        VIRTUAL_ITEM_INFO_0_OFFSET_SUBCLASS))
                    {
                    case ITEM_SUBCLASS_WEAPON_THROWN:
                        ammoDisplayID = m_caster->GetUInt32Value(
                            UNIT_VIRTUAL_ITEM_SLOT_DISPLAY + i);
                        ammoInventoryType = m_caster->GetByteValue(
                            UNIT_VIRTUAL_ITEM_INFO + (i * 2) + 1,
                            VIRTUAL_ITEM_INFO_1_OFFSET_INVENTORYTYPE);
                        break;
                    case ITEM_SUBCLASS_WEAPON_BOW:
                    case ITEM_SUBCLASS_WEAPON_CROSSBOW:
                        ammoDisplayID = 5996; // is this need fixing?
                        ammoInventoryType = INVTYPE_AMMO;
                        break;
                    case ITEM_SUBCLASS_WEAPON_GUN:
                        ammoDisplayID = 5998; // is this need fixing?
                        ammoInventoryType = INVTYPE_AMMO;
                        break;
                    }

                    if (ammoDisplayID)
                        break;
                }
            }
        }
    }

    *data << uint32(ammoDisplayID);
    *data << uint32(ammoInventoryType);
}

void Spell::WriteSpellGoTargets(WorldPacket* data)
{
    size_t count_pos = data->wpos();
    *data << uint8(0); // placeholder

    // This function also fill data for channeled spells:
    // m_needAliveTargetMask req for stop channeling if one target die
    uint32 hit = m_UniqueGOTargetInfo.size(); // Always hits on GO
    uint32 miss = 0;

    for (auto& elem : m_UniqueTargetInfo)
    {
        // HACK: Whirlwind, should not send caster as target (TODO: find general
        // rule)
        if ((m_spellInfo->Id == 1680 && m_casterGUID == elem.targetGUID) ||
            m_spellInfo->Id == 31687) // Summon Water Elemental
            continue;

        if (elem.effectMask == 0) // No effect apply - all immuned add state
        {
            // possibly SPELL_MISS_IMMUNE2 for this??
            elem.missCondition = SPELL_MISS_IMMUNE2;
            ++miss;
        }
        else if (elem.missCondition == SPELL_MISS_NONE) // Add only hits
        {
            ++hit;
            *data << elem.targetGUID;
            m_needAliveTargetMask |= elem.effectMask;
        }
        else
            ++miss;
    }

    for (GOTargetList::const_iterator ighit = m_UniqueGOTargetInfo.begin();
         ighit != m_UniqueGOTargetInfo.end(); ++ighit)
        *data << ighit->targetGUID; // Always hits

    data->put<uint8>(count_pos, hit);

    *data << (uint8)miss;
    for (TargetList::const_iterator ihit = m_UniqueTargetInfo.begin();
         ihit != m_UniqueTargetInfo.end(); ++ihit)
    {
        // HACK: Whirlwind, should not send caster as target (TODO: find general
        // rule)
        if ((m_spellInfo->Id == 1680 && m_casterGUID == ihit->targetGUID) ||
            m_spellInfo->Id == 31687) // Summon Water Elemental
            continue;

        if (ihit->missCondition != SPELL_MISS_NONE) // Add only miss
        {
            *data << ihit->targetGUID;
            *data << uint8(ihit->missCondition);
            if (ihit->missCondition == SPELL_MISS_REFLECT)
                *data << uint8(ihit->reflectResult);
        }
    }
    // Reset m_needAliveTargetMask for non channeled spell
    if (!IsChanneledSpell(m_spellInfo))
        m_needAliveTargetMask = 0;
}

void Spell::SendLogExecute()
{
    Unit* target =
        m_targets.getUnitTarget() ? m_targets.getUnitTarget() : m_caster;

    WorldPacket data(SMSG_SPELLLOGEXECUTE, (8 + 4 + 4 + 4 + 4 + 8));

    if (m_caster->GetTypeId() == TYPEID_PLAYER)
        data << m_caster->GetPackGUID();
    else
        data << target->GetPackGUID();

    data << uint32(m_spellInfo->Id);
    uint32 count1 = 1;
    data << uint32(count1); // count1 (effect count?)
    for (uint32 i = 0; i < count1; ++i)
    {
        data << uint32(m_spellInfo->Effect[EFFECT_INDEX_0]); // spell effect
        uint32 count2 = 1;
        data << uint32(count2); // count2 (target count?)
        for (uint32 j = 0; j < count2; ++j)
        {
            switch (m_spellInfo->Effect[EFFECT_INDEX_0])
            {
            case SPELL_EFFECT_POWER_DRAIN:
                if (Unit* unit = m_targets.getUnitTarget())
                    data << unit->GetPackGUID();
                else
                    data << uint8(0);
                data << uint32(0);
                data << uint32(0);
                data << float(0);
                break;
            case SPELL_EFFECT_ADD_EXTRA_ATTACKS:
            {
                if (Unit* unit = m_targets.getUnitTarget())
                    data << unit->GetPackGUID();
                else
                    data << uint8(0);
                uint32 cnt = m_spellInfo->EffectBasePoints[EFFECT_INDEX_0] + 1;
                data << uint32(cnt); // count
                break;
            }
            case SPELL_EFFECT_INTERRUPT_CAST:
                if (Unit* unit = m_targets.getUnitTarget())
                    data << unit->GetPackGUID();
                else
                    data << uint8(0);
                data << uint32(0); // spellid
                break;
            case SPELL_EFFECT_DURABILITY_DAMAGE:
                if (Unit* unit = m_targets.getUnitTarget())
                    data << unit->GetPackGUID();
                else
                    data << uint8(0);
                data << uint32(0);
                data << uint32(0);
                break;
            case SPELL_EFFECT_OPEN_LOCK:
            case SPELL_EFFECT_OPEN_LOCK_ITEM:
                if (Item* item = m_targets.getItemTarget())
                    data << item->GetPackGUID();
                else
                    data << uint8(0);
                break;
            case SPELL_EFFECT_CREATE_ITEM:
                data << uint32(m_spellInfo->EffectItemType[EFFECT_INDEX_0]);
                break;
            case SPELL_EFFECT_SUMMON:
            case SPELL_EFFECT_TRANS_DOOR:
            case SPELL_EFFECT_SUMMON_PET:
            case SPELL_EFFECT_SUMMON_OBJECT_WILD:
            case SPELL_EFFECT_CREATE_HOUSE:
            case SPELL_EFFECT_DUEL:
            case SPELL_EFFECT_SUMMON_OBJECT_SLOT1:
            case SPELL_EFFECT_SUMMON_OBJECT_SLOT2:
            case SPELL_EFFECT_SUMMON_OBJECT_SLOT3:
            case SPELL_EFFECT_SUMMON_OBJECT_SLOT4:
                if (summoned_target_)
                    data << summoned_target_.WriteAsPacked();
                else if (auto guid = m_targets.getUnitTargetGuid())
                    data << guid.WriteAsPacked();
                else if (auto guid = m_targets.getItemTargetGuid())
                    data << guid.WriteAsPacked();
                else if (auto guid = m_targets.getGOTargetGuid())
                    data << guid.WriteAsPacked();
                else
                    return;
                break;
            case SPELL_EFFECT_FEED_PET:
                data << uint32(m_targets.getItemTargetEntry());
                break;
            case SPELL_EFFECT_DISMISS_PET:
                if (Unit* unit = m_targets.getUnitTarget())
                    data << unit->GetPackGUID();
                else
                    data << uint8(0);
                break;
            default:
                return;
            }
        }
    }

    SendMessageToSet(std::move(data));
}

void Spell::SendInterrupted(uint8 result)
{
    WorldPacket data(SMSG_SPELL_FAILURE, (8 + 4 + 1));
    data << m_caster->GetPackGUID();
    data << m_spellInfo->Id;
    data << result;
    SendMessageToSet(std::move(data));

    data.initialize(SMSG_SPELL_FAILED_OTHER, (8 + 4));
    data << m_caster->GetPackGUID();
    data << m_spellInfo->Id;
    SendMessageToSet(std::move(data));
}

void Spell::SendChannelUpdate(uint32 time)
{
    if (time == 0)
    {
        // Only finish channeling when latest channeled spell finishes
        if (m_caster->GetUInt32Value(UNIT_CHANNEL_SPELL) != m_spellInfo->Id)
            return;

        m_caster->SetChannelObjectGuid(ObjectGuid());
        m_caster->SetUInt32Value(UNIT_CHANNEL_SPELL, 0);
    }

    WorldPacket data(MSG_CHANNEL_UPDATE, 8 + 4);
    data << m_caster->GetPackGUID();
    data << uint32(time);
    SendMessageToSet(std::move(data));
}

void Spell::SendChannelStart(uint32 duration)
{
    WorldObject* target = nullptr;
    bool self_channel = false;

    // select dynobject created by first effect if any
    if (m_spellInfo->Effect[EFFECT_INDEX_0] ==
        SPELL_EFFECT_PERSISTENT_AREA_AURA)
        target = m_caster->GetDynObject(m_spellInfo->Id, EFFECT_INDEX_0);
    // select first not resisted target from target list for _0_ effect
    else if (!m_UniqueTargetInfo.empty())
    {
        for (TargetList::const_iterator itr = m_UniqueTargetInfo.begin();
             itr != m_UniqueTargetInfo.end(); ++itr)
        {
            if (itr->missCondition == SPELL_MISS_REFLECT)
            {
                // a reflected result means the caster is channeling on himself
                self_channel = true;
                break;
            }
            else if ((itr->effectMask & (1 << EFFECT_INDEX_0)) &&
                     itr->missCondition == SPELL_MISS_NONE &&
                     itr->targetGUID != m_caster->GetObjectGuid())
            {
                target = ObjectAccessor::GetUnit(*m_caster, itr->targetGUID);
                break;
            }
        }
    }
    else if (!m_UniqueGOTargetInfo.empty())
    {
        for (GOTargetList::const_iterator itr = m_UniqueGOTargetInfo.begin();
             itr != m_UniqueGOTargetInfo.end(); ++itr)
        {
            if (itr->effectMask & (1 << EFFECT_INDEX_0))
            {
                target = m_caster->GetMap()->GetGameObject(itr->targetGUID);
                break;
            }
        }
    }

    WorldPacket data(MSG_CHANNEL_START, (8 + 4 + 4));
    data << m_caster->GetPackGUID();
    data << uint32(m_spellInfo->Id);
    data << uint32(duration);
    SendMessageToSet(std::move(data));

    m_timer = duration;

    if (target)
        m_caster->SetChannelObjectGuid(target->GetObjectGuid());
    else if (self_channel)
        m_caster->SetChannelObjectGuid(m_caster->GetObjectGuid());

    m_caster->SetUInt32Value(UNIT_CHANNEL_SPELL, m_spellInfo->Id);
}

void Spell::SendResurrectRequest(Player* target)
{
    // Both players and NPCs can resurrect using spells - have a look at
    // creature 28487 for example
    // However, the packet structure differs slightly

    const char* sentName =
        m_caster->GetTypeId() == TYPEID_PLAYER ?
            "" :
            m_caster->GetNameForLocaleIdx(
                target->GetSession()->GetSessionDbLocaleIndex());

    WorldPacket data(
        SMSG_RESURRECT_REQUEST, (8 + 4 + strlen(sentName) + 1 + 1 + 1));
    data << m_caster->GetObjectGuid();
    data << uint32(strlen(sentName) + 1);

    data << sentName;
    data << uint8(0);

    data << uint8(m_caster->GetTypeId() == TYPEID_PLAYER ? 0 : 1);
    target->GetSession()->send_packet(std::move(data));
}

void Spell::SendPlaySpellVisual(uint32 SpellID)
{
    if (m_caster->GetTypeId() != TYPEID_PLAYER)
        return;

    WorldPacket data(SMSG_PLAY_SPELL_VISUAL, 8 + 4);
    data << m_caster->GetObjectGuid();
    data << uint32(SpellID); // spell visual id?
    ((Player*)m_caster)->GetSession()->send_packet(std::move(data));
}

void Spell::TakeCastItem()
{
    if (!m_CastItem || m_caster->GetTypeId() != TYPEID_PLAYER)
        return;

    m_CastItem->remove_referencing_spell(this);

    // not remove cast item at triggered spell (equipping, weapon damage, etc)
    if (trigger_type_.triggered())
        return;

    ItemPrototype const* proto = m_CastItem->GetProto();

    if (!proto)
    {
        // This code is to avoid a crash
        // I'm not sure, if this is really an error, but I guess every item
        // needs a prototype
        logging.error("Cast item (%s) has no item prototype",
            m_CastItem->GetGuidStr().c_str());
        return;
    }

    // DropSpellCharge() returns true if item should be deleted
    if (m_CastItem->DropSpellCharge())
        static_cast<Player*>(m_caster)->storage().remove_count(m_CastItem, 1);

    // null-out cast item so that no further routines will try to access it or
    // alter it
    ClearCastItem();
}

void Spell::TakePower()
{
    if (m_CastItem || m_triggeredByAuraSpell)
        return;

    // health as power used
    if (m_spellInfo->powerType == POWER_HEALTH)
    {
        m_caster->ModifyHealth(-(int32)m_powerCost);
        return;
    }

    if (m_spellInfo->powerType >= MAX_POWERS)
    {
        logging.error("Spell::TakePower: Unknown power type '%d'",
            m_spellInfo->powerType);
        return;
    }

    Powers powerType = Powers(m_spellInfo->powerType);

    m_caster->ModifyPower(powerType, -(int32)m_powerCost);

    // Set the five second timer
    if (powerType == POWER_MANA && m_powerCost > 0)
        m_caster->SetLastManaUse();
}

void Spell::TakeReagents()
{
    if (m_caster->GetTypeId() != TYPEID_PLAYER)
        return;

    if (IgnoreItemRequirements()) // reagents used in triggered spell removed by
                                  // original spell or don't must be removed.
        return;

    Player* p_caster = (Player*)m_caster;

    for (uint32 x = 0; x < MAX_SPELL_REAGENTS; ++x)
    {
        if (m_spellInfo->Reagent[x] <= 0)
            continue;

        uint32 itemid = m_spellInfo->Reagent[x];
        uint32 itemcount = m_spellInfo->ReagentCount[x];

        // if CastItem is also spell reagent
        if (m_CastItem)
        {
            ItemPrototype const* proto = m_CastItem->GetProto();
            if (proto && proto->ItemId == itemid)
            {
                for (int s = 0; s < MAX_ITEM_PROTO_SPELLS; ++s)
                {
                    // CastItem will be used up and does not count as reagent
                    int32 charges = m_CastItem->GetSpellCharges(s);
                    if (proto->Spells[s].SpellCharges < 0 && abs(charges) < 2)
                    {
                        ++itemcount;
                        break;
                    }
                }

                ClearCastItem(); // nulls out cast item
            }
        }

        // if getItemTarget is also spell reagent
        if (m_targets.getItemTargetEntry() == itemid)
            m_targets.setItemTarget(nullptr);

        // XXX
        inventory::transaction trans(false);
        trans.destroy(itemid, itemcount);
        p_caster->storage().finalize(trans);
    }
}

void Spell::RefundReagents()
{
    if (m_caster->GetTypeId() != TYPEID_PLAYER)
        return;

    if (IgnoreItemRequirements())
        return;

    Player* p_caster = (Player*)m_caster;

    for (uint32 x = 0; x < MAX_SPELL_REAGENTS; ++x)
    {
        if (m_spellInfo->Reagent[x] <= 0)
            continue;

        uint32 itemid = m_spellInfo->Reagent[x];
        uint32 itemcount = m_spellInfo->ReagentCount[x];

        // Note: Not refunding charges on m_castItem

        // if getItemTarget is also spell reagent
        if (m_targets.getItemTargetEntry() == itemid)
            m_targets.setItemTarget(nullptr);

        // XXX
        inventory::transaction trans;
        trans.add(itemid, itemcount);
        p_caster->storage().finalize(trans);
    }
}

// Applies diminishing durations
void Spell::HandleDurationDR(Unit* target, bool isReflected, uint32 effect_mask)
{
    // Don't modify infinite durations
    if (m_duration == -1)
        return;

    if (target == nullptr && !IsChanneledSpell(m_spellInfo))
        return;

    if (target == nullptr)
    {
        // Channeled spell, repeatedly call this function to find the longest
        // duration
        int32 prev_duration = 0;
        for (auto& elem : m_UniqueTargetInfo)
        {
            // skip caster
            if (elem.targetGUID == m_casterGUID)
                continue;
            Unit* unit = ObjectAccessor::GetUnit(*m_caster, elem.targetGUID);
            if (!unit)
                continue;
            HandleDurationDR(unit, elem.missCondition == SPELL_MISS_REFLECT);
            // Reset m_duration if we previously found a target with longer
            // duration
            if (prev_duration > m_duration)
                m_duration = prev_duration;
        }

        // Once we've decided channel duration we need to use that as the
        // highest possible aura duration
        m_durationMax = m_duration;

        return; // NOTE: Don't process the rest of the code; it assumes target
                // != nullptr
    }

    // If immune to channel spell, just set duration to zero
    if (IsChanneledSpell(m_spellInfo) &&
        target->SpellImmunityCheck(m_spellInfo, 0x7) == 0)
    {
        m_duration = 0;
        return;
    }

    if (isReflected && IsSpellReflectIgnored(m_spellInfo))
    {
        m_duration = 0;
        return;
    }

    m_duration = m_durationUnmod;

    if (m_duration > 0)
    {
        bool uncontrolled_proc =
            m_triggeredByAuraSpell || (m_CastItem && trigger_type_.triggered());
        auto dr_grp =
            GetDiminishingReturnsGroupForSpell(m_spellInfo, uncontrolled_proc);
        auto dr_lvl = target->GetDiminishing(dr_grp);
        if (target != m_caster || isReflected)
            target->ApplyDiminishingToDuration(
                dr_grp, m_duration, m_caster, dr_lvl, isReflected);

        // Clever Traps allows you to surpass the cap of 10 seconds
        if (m_spellInfo->SpellFamilyName == SPELLFAMILY_HUNTER &&
            m_spellInfo->SpellFamilyFlags & 0x8)
        {
            if (m_caster->has_aura(19245))
                m_duration *= 1.3f;
            else if (m_caster->has_aura(19239))
                m_duration *= 1.15f;
        }
    }

    m_duration = target->CalculateAuraDuration(
        m_spellInfo, effect_mask, m_duration, m_caster);
    if (m_duration > m_durationMax)
        m_duration = m_durationMax;
}

void Spell::HandleThreatSpells()
{
    if (m_UniqueTargetInfo.empty())
        return;

    SpellThreatEntry const* threatEntry =
        sSpellMgr::Instance()->GetSpellThreatEntry(m_spellInfo->Id);

    if (!threatEntry || (!threatEntry->threat && threatEntry->ap_bonus == 0.0f))
        return;

    float threat = threatEntry->threat;
    if (threatEntry->ap_bonus != 0.0f)
        threat += threatEntry->ap_bonus *
                  m_caster->GetTotalAttackPowerValue(
                      GetWeaponAttackType(m_spellInfo));

    bool positive = true;
    uint8 effectMask = 0;
    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
        if (m_spellInfo->Effect[i])
            effectMask |= (1 << i);

    if (m_negativeEffectMask & effectMask)
    {
        // can only handle spells with clearly defined positive/negative effect,
        // check at spell_threat loading probably not perfect
        // so abort when only some effects are negative.
        if ((m_negativeEffectMask & effectMask) != effectMask)
        {
            LOG_DEBUG(logging,
                "Spell %u, rank %u, is not clearly positive or negative, "
                "ignoring bonus threat",
                m_spellInfo->Id,
                sSpellMgr::Instance()->GetSpellRank(m_spellInfo->Id));
            return;
        }
        positive = false;
    }

    // since 2.0.1 threat from positive effects also is distributed among all
    // targets, so the overall caused threat is at most the defined bonus
    threat /= m_UniqueTargetInfo.size();

    for (TargetList::const_iterator ihit = m_UniqueTargetInfo.begin();
         ihit != m_UniqueTargetInfo.end(); ++ihit)
    {
        if (ihit->missCondition != SPELL_MISS_NONE)
            continue;

        Unit* target = m_caster->GetObjectGuid() == ihit->targetGUID ?
                           m_caster :
                           ObjectAccessor::GetUnit(*m_caster, ihit->targetGUID);
        if (!target)
            continue;

        // positive spells distribute threat among all units that are in combat
        // with target, like healing
        if (positive)
        {
            target->getHostileRefManager().threatAssist(
                m_caster /*real_caster ??*/, threat, m_spellInfo);
        }
        // for negative spells threat gets distributed among affected targets
        else
        {
            if (!target->CanHaveThreatList())
                continue;

            target->AddThreat(m_caster, threat, false,
                GetSpellSchoolMask(m_spellInfo), m_spellInfo);
        }
    }

    LOG_DEBUG(logging,
        "Spell %u added an additional %f threat for %s %u target(s)",
        m_spellInfo->Id, threat, positive ? "assisting" : "harming",
        uint32(m_UniqueTargetInfo.size()));
}

void Spell::HandleEffects(Unit* pUnitTarget, Item* pItemTarget,
    GameObject* pGOTarget, SpellEffectIndex i)
{
    unitTarget = pUnitTarget;
    itemTarget = pItemTarget;
    gameObjTarget = pGOTarget;

    uint8 eff = m_spellInfo->Effect[i];

    damage = int32(CalculateDamage(i, unitTarget));

    LOG_DEBUG(logging, "Spell %u Effect%d : %u Targets: %s, %s, %s",
        m_spellInfo->Id, i, eff,
        unitTarget ? unitTarget->GetGuidStr().c_str() : "-",
        itemTarget ? itemTarget->GetGuidStr().c_str() : "-",
        gameObjTarget ? gameObjTarget->GetGuidStr().c_str() : "-");

    if (eff < TOTAL_SPELL_EFFECTS)
    {
        // Handle Mechanic Resistance for Unit Targets (it only affects the
        // current spell effect, not the entire spell)
        if (pUnitTarget)
        {
            int32 resistChance = 0;
            int32 effMechanic = m_spellInfo->EffectMechanic[i];
            if (effMechanic)
                resistChance = pUnitTarget->GetTotalAuraModifierByMiscValue(
                    SPELL_AURA_MOD_MECHANIC_RESISTANCE, effMechanic);
            if (resistChance > 0)
            {
                uint32 randVal = urand(1, 100);
                if (randVal <= static_cast<uint32>(resistChance))
                {
                    // FIXME: This says fully resisted in the combat log, that's
                    // probably not
                    // correct, but I couldn't find the right packet, so if you
                    // do please fix this.
                    m_caster->SendSpellMiss(
                        pUnitTarget, m_spellInfo->Id, SPELL_MISS_RESIST);
                    return;
                }
            }
        }

        (*this.*SpellEffects[eff])(i);
    }
    else
    {
        logging.error("WORLD: Spell FX %d > TOTAL_SPELL_EFFECTS ", eff);
    }
}

void Spell::AddTriggeredSpell(uint32 spellId)
{
    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellId);

    if (!spellInfo)
    {
        logging.error(
            "Spell::AddTriggeredSpell: unknown spell id %u used as triggred "
            "spell for spell %u)",
            spellId, m_spellInfo->Id);
        return;
    }

    m_TriggerSpells.push_back(spellInfo);
}

void Spell::AddPrecastSpell(uint32 spellId)
{
    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellId);

    if (!spellInfo)
    {
        logging.error(
            "Spell::AddPrecastSpell: unknown spell id %u used as pre-cast "
            "spell for spell %u)",
            spellId, m_spellInfo->Id);
        return;
    }

    m_preCastSpells.push_back(spellInfo);
}

void Spell::CastTriggerSpells()
{
    for (SpellInfoList::const_iterator si = m_TriggerSpells.begin();
         si != m_TriggerSpells.end(); ++si)
    {
        auto spell = new Spell(m_caster, (*si), true, m_originalCasterGUID);
        spell->prepare(&m_targets); // use original spell original targets
    }
}

void Spell::CastPreCastSpells(Unit* target)
{
    for (SpellInfoList::const_iterator si = m_preCastSpells.begin();
         si != m_preCastSpells.end(); ++si)
        m_caster->CastSpell(target, (*si), true, m_CastItem);
}

SpellCastResult Spell::CheckPetCast(Unit* target)
{
    if (!m_caster->isAlive())
        return SPELL_FAILED_CASTER_DEAD;

    // prevent spellcast interruption by another spellcast
    if (m_caster->IsNonMeleeSpellCasted(false) &&
        !path_gen_finished) // having a finished path gen means we've already
                            // interrupted previous spells
    {
        // Instant spells interrupt current
        if (!IsChanneledSpell(m_spellInfo) &&
            GetSpellCastTime(m_spellInfo) == 0)
        {
            m_caster->InterruptNonMeleeSpells(false);
        }
        else
        {
            // Casted spell already in progress
            return SPELL_FAILED_SPELL_IN_PROGRESS;
        }
    }
    if (m_caster->isInCombat() && IsNonCombatSpell(m_spellInfo))
        return SPELL_FAILED_AFFECTING_COMBAT;

    if (m_caster->GetTypeId() == TYPEID_UNIT &&
        (((Creature*)m_caster)->IsPet() || m_caster->isCharmed()))
    {
        // dead owner (pets still alive when owners ressed?)
        if (m_caster->GetCharmerOrOwner() &&
            !m_caster->GetCharmerOrOwner()->isAlive())
            return SPELL_FAILED_CASTER_DEAD;

        if (!target && m_targets.getUnitTarget())
            target = m_targets.getUnitTarget();

        bool need = false;
        bool PosAndNeg = false;
        for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
        {
            if (m_spellInfo->EffectImplicitTargetA[i] == TARGET_CHAIN_DAMAGE ||
                m_spellInfo->EffectImplicitTargetA[i] == TARGET_SINGLE_FRIEND ||
                m_spellInfo->EffectImplicitTargetA[i] ==
                    TARGET_SINGLE_FRIEND_2 ||
                m_spellInfo->EffectImplicitTargetA[i] == TARGET_DUELVSPLAYER ||
                m_spellInfo->EffectImplicitTargetA[i] == TARGET_SINGLE_PARTY ||
                m_spellInfo->EffectImplicitTargetA[i] ==
                    TARGET_CURRENT_ENEMY_COORDINATES)
            {
                need = true;

                if (m_spellInfo->EffectImplicitTargetA[i] ==
                    TARGET_DUELVSPLAYER)
                    PosAndNeg = true;

                if (!target)
                    return SPELL_FAILED_BAD_IMPLICIT_TARGETS;
                break;
            }
        }
        if (need)
            m_targets.setUnitTarget(target);

        Unit* _target = m_targets.getUnitTarget();

        if (_target) // for target dead/target not valid
        {
            if (IsPositiveSpell(m_spellInfo->Id) && !PosAndNeg)
            {
                if (m_caster->IsHostileTo(_target))
                    return SPELL_FAILED_BAD_TARGETS;
            }
            else
            {
                bool duelvsplayertar = false;
                for (int j = 0; j < MAX_EFFECT_INDEX; ++j)
                {
                    // TARGET_DUELVSPLAYER is positive AND negative
                    duelvsplayertar |= (m_spellInfo->EffectImplicitTargetA[j] ==
                                        TARGET_DUELVSPLAYER);
                }
                if (m_caster->IsFriendlyTo(target) && !duelvsplayertar)
                {
                    return SPELL_FAILED_BAD_TARGETS;
                }
                if (!_target->isTargetableForAttack())
                    return SPELL_FAILED_BAD_TARGETS;
            }
        }
        // cooldown
        if (((Creature*)m_caster)->HasSpellCooldown(m_spellInfo->Id))
            return SPELL_FAILED_NOT_READY;
    }

    return CheckCast(true);
}

SpellCastResult Spell::CheckCast(bool strict)
{
    // check cooldowns
    SpellCastResult cooldown_res = CheckCooldowns(strict);
    if (cooldown_res != SPELL_CAST_OK)
        return cooldown_res;

    // check spell range
    if (!trigger_type_.triggered() && !m_triggeredByAuraSpell)
    {
        SpellCastResult castResult = CheckRange(strict);
        if (castResult != SPELL_CAST_OK)
            return castResult;
    }

    // check caster state
    SpellCastResult caster_state_res = CheckCasterState(strict);
    if (caster_state_res != SPELL_CAST_OK)
        return caster_state_res;

    if (m_caster->GetTypeId() == TYPEID_PLAYER)
    {
        // Don't allow targeted spells to be casted on a target that is not at
        // the caster's client
        if (m_targets.getUnitTarget() &&
            !static_cast<Player*>(m_caster)->HaveAtClient(
                m_targets.getUnitTarget()) &&
            !trigger_type_.triggered() && /* ignore triggered spells */
            (m_spellInfo->HasTargetType(TARGET_CHAIN_DAMAGE) ||
                m_spellInfo->HasTargetType(TARGET_DUELVSPLAYER)))
            return SPELL_FAILED_INTERRUPTED;

        // cancel autorepeat spells if cast start when moving
        // (not wand currently autorepeat cast delayed to moving stop anyway in
        // spell update code)
        bool rootHack =
            !((Player*)m_caster)
                 ->m_movementInfo.HasMovementFlag(MOVEFLAG_FALLING_UNK1) &&
            m_caster->hasUnitState(UNIT_STAT_ROOT);
        if (((Player*)m_caster)->isMoving() && !rootHack)
        {
            // skip stuck spell to allow use it in falling case and apply spell
            // limitations at movement
            if ((!((Player*)m_caster)
                        ->m_movementInfo.HasMovementFlag(
                            MOVEFLAG_FALLING_UNK1) ||
                    m_spellInfo->Effect[EFFECT_INDEX_0] !=
                        SPELL_EFFECT_STUCK) &&
                (IsAutoRepeat() ||
                    (m_spellInfo->AuraInterruptFlags &
                        AURA_INTERRUPT_FLAG_NOT_SEATED) != 0))
                return SPELL_FAILED_MOVING;
            // Only check before cast started
            else if (!m_executedCurrently && !IsNextMeleeSwingSpell() &&
                     !trigger_type_.triggered() &&
                     (m_spellInfo->InterruptFlags &
                         SPELL_INTERRUPT_FLAG_MOVEMENT))
            {
                // make sure the spell is not instant (we use peaking to not
                // apply spell mods, in case spell fails)
                if (IsChanneledSpell(m_spellInfo))
                {
                    if (m_spellInfo->ChannelInterruptFlags &
                        CHANNEL_FLAG_MOVEMENT)
                        return SPELL_FAILED_MOVING;
                }
                else if (GetSpellCastTime(m_spellInfo, this, true) > 0)
                    return SPELL_FAILED_MOVING;
            }
        }

        if (!trigger_type_.triggered() && NeedsComboPoints(m_spellInfo) &&
            (!m_targets.getUnitTarget() ||
                m_targets.getUnitTarget()->GetObjectGuid() !=
                    ((Player*)m_caster)->GetComboTargetGuid()))
            // warrior not have real combo-points at client side but use this
            // way for mark allow Overpower use
            return m_caster->getClass() == CLASS_WARRIOR ?
                       SPELL_FAILED_CASTER_AURASTATE :
                       SPELL_FAILED_NO_COMBO_POINTS;
    }

    SpellCastResult cast_target_res = CheckCastTarget();
    if (cast_target_res != SPELL_CAST_OK)
        return cast_target_res;

    // always (except passive spells) check items (focus object can be required
    // for any type casts)
    if (!IsPassiveSpell(m_spellInfo))
    {
        SpellCastResult castResult = CheckItems();
        if (castResult != SPELL_CAST_OK)
            return castResult;
    }

    // check DB targets
    SpellCastResult db_target_res = CheckDbTarget();
    if (db_target_res != SPELL_CAST_OK)
        return db_target_res;

    SpellCastResult power_res = CheckPower();
    if (power_res != SPELL_CAST_OK)
        return power_res;

    if (!trigger_type_.triggered()) // triggered spell not affected by stun/etc
    {
        SpellCastResult castResult = CheckCasterAuras();
        if (castResult != SPELL_CAST_OK)
            return castResult;
    }

    // Patch 2.3: Another misdirection on target will now ALWAYS bounce
    if (m_spellInfo->Id == 34477 &&
        (!m_targets.getUnitTarget() ||
            m_targets.getUnitTarget()->has_aura(35079)))
        return SPELL_FAILED_AURA_BOUNCED;

    // "A more powerful spell is already active" error
    if (!trigger_type_.triggered() && m_targets.getUnitTarget() &&
        m_spellInfo->CastingTimeIndex == 1 && // Instant spells only
        (m_spellInfo->HasEffect(SPELL_EFFECT_APPLY_AURA)) &&
        !IsAreaOfEffectSpell(m_spellInfo) && !does_direct_damage(m_spellInfo) &&
        !(GetAllSpellMechanicMask(m_spellInfo) &
            (1 << (MECHANIC_STUN - 1))) && // Ignore any spells with direct
                                           // damage or of mechanic stun
        !m_spellInfo->HasAttribute(SPELL_ATTR_CUSTOM_ALWAYS_CASTABLE))
    {
        // If all apply aura effect are TARGET_SELF, check on self instead
        auto target = m_caster;
        for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
            if (m_spellInfo->Effect[i] == SPELL_EFFECT_APPLY_AURA &&
                m_spellInfo->EffectImplicitTargetA[i] != TARGET_SELF)
            {
                target = m_targets.getUnitTarget();
                break;
            }
        // Check buff stacking rules
        std::pair<bool, std::set<AuraHolder*>> result =
            buff_stacks(m_spellInfo, m_caster, target, m_CastItem)();
        if (!result.first)
            return SPELL_FAILED_AURA_BOUNCED;
    }

    // check specific spell effects
    SpellCastResult effect_res = CheckEffects(strict);
    if (effect_res != SPELL_CAST_OK)
        return effect_res;

    // check specific spell auras
    SpellCastResult aura_res = CheckAuras();
    if (aura_res != SPELL_CAST_OK)
        return aura_res;

    // check trade slot case (last, for allow catch any another cast problems)
    SpellCastResult trade_res = CheckTrade();
    if (trade_res != SPELL_CAST_OK)
        return trade_res;

    // Database conditions
    auto conditions =
        sConditionMgr::Instance()->GetSpellCastConditions(m_spellInfo->Id);
    if (conditions)
    {
        auto condition_info =
            ConditionSourceInfo(m_caster, m_targets.getUnitTarget());
        if (!sConditionMgr::Instance()->IsObjectMeetToConditions(
                condition_info, conditions))
        {
            if (condition_info.mLastFailedCondition)
            {
                if (condition_info.mLastFailedCondition->ErrorTextId != 0)
                    return static_cast<SpellCastResult>(
                        condition_info.mLastFailedCondition->ErrorTextId - 1);
                if (condition_info.mLastFailedCondition->ConditionTarget == 0)
                    return SPELL_FAILED_CASTER_AURASTATE;
            }
            return SPELL_FAILED_BAD_TARGETS;
        }
    }

    // Some spells require special attention
    switch (m_spellInfo->Id)
    {
    case 10909: // Mind Vision
    {
        if (m_targets.getUnitTarget() == m_caster)
            return SPELL_FAILED_OUT_OF_RANGE;
        break;
    }
    case 10060: // Power Infusion
        // Patch 2.1: "This ability is now unuseable on Rogues or Warriors."
        if (Unit* t = m_targets.getUnitTarget())
            if (t->getClass() == CLASS_WARRIOR || t->getClass() == CLASS_ROGUE)
                return SPELL_FAILED_BAD_TARGETS;
        break;
    default:
        break;
    }

    // TODO: This is a poor design at the moment, but leaving it up to
    // SetTargetMap
    //       means we won't get an error on erroneous casts (such as trying to
    //       make
    //       warlock's imp cast fire shiled on a non-party-member).
    if (m_spellInfo->HasTargetType(TARGET_SINGLE_PARTY))
    {
        Unit* target = m_targets.getUnitTarget();
        if (!target)
            return SPELL_FAILED_BAD_TARGETS;
        Player* me = m_caster->GetCharmerOrOwnerPlayerOrPlayerItself();
        Player* him = target->GetCharmerOrOwnerPlayerOrPlayerItself();
        if (!me || !him || !me->IsInSameRaidWith(him))
            return SPELL_FAILED_BAD_TARGETS;
    }

    // all ok
    return SPELL_CAST_OK;
}

SpellCastResult Spell::CheckCooldowns(bool strict)
{
    // check cooldowns to prevent cheating (ignore passive spells, that client
    // side visual only)
    if (m_caster->GetTypeId() == TYPEID_PLAYER &&
        !m_spellInfo->HasAttribute(SPELL_ATTR_PASSIVE) &&
        !trigger_type_.ignore_cd() &&
        ((Player*)m_caster)
            ->HasSpellCooldown(
                m_spellInfo->Id, m_CastItem ? m_CastItem->GetEntry() : 0))
    {
        if (m_triggeredByAuraSpell)
            return SPELL_FAILED_DONT_REPORT;
        else
            return SPELL_FAILED_NOT_READY;
    }

    // Throw needs to check if ranged attack timer is ready
    if (!IsAutoRepeat() &&
        m_spellInfo->HasAttribute(SPELL_ATTR_EX3_RANGED_ATT_TIMER) &&
        m_caster->getAttackTimer(RANGED_ATTACK) > 0)
        return SPELL_FAILED_NOT_READY;

    // Creatures get the cooldown checked for specific spell schools instead of
    // for spells
    if (m_spellInfo->PreventionType == SPELL_PREVENTION_TYPE_SILENCE &&
        m_caster->GetTypeId() == TYPEID_UNIT &&
        ((Creature*)m_caster)
            ->IsSpellSchoolLocked((SpellSchoolMask)m_spellInfo->SchoolMask))
    {
        if (m_triggeredByAuraSpell)
            return SPELL_FAILED_DONT_REPORT;
        else
            return SPELL_FAILED_NOT_READY;
    }

    // check global cooldown
    if (strict && !trigger_type_.triggered() && !trigger_type_.ignore_gcd() &&
        HasGlobalCooldown())
        return SPELL_FAILED_NOT_READY;

    return SPELL_CAST_OK;
}

SpellCastResult Spell::CheckRange(bool strict)
{
    Unit* target = m_targets.getUnitTarget();

    // special range cases
    switch (m_spellInfo->rangeIndex)
    {
    // self cast doesn't need range checking -- also for Starshards fix
    // spells that can be cast anywhere also need no check
    case SPELL_RANGE_IDX_SELF_ONLY:
    case SPELL_RANGE_IDX_ANYWHERE:
        return SPELL_CAST_OK;
    // combat range spells are treated differently
    case SPELL_RANGE_IDX_COMBAT:
    {
        if (target)
        {
            if (target == m_caster)
                return SPELL_CAST_OK;

            float range_mod = strict ? 0.0f : 5.0f;
            float base = ATTACK_DISTANCE;
            if (Player* modOwner = m_caster->GetSpellModOwner())
                range_mod += modOwner->ApplySpellMod(
                    m_spellInfo->Id, SPELLMOD_RANGE, base, this);

            // with additional 5 dist for non stricted case (some melee spells
            // have delay in apply
            if (!m_caster->CanReachWithMeleeAttack(target, range_mod))
                return SPELL_FAILED_OUT_OF_RANGE;

            if (m_caster->GetTypeId() == TYPEID_PLAYER &&
                (m_spellInfo->FacingCasterFlags & SPELL_FACING_FLAG_INFRONT) &&
                !m_caster->HasInArc(M_PI_F, target))
                return SPELL_FAILED_UNIT_NOT_INFRONT;

            return SPELL_CAST_OK;
        }
        break; // let continue in generic way for no target
    }
    }

    // Add 5 yard extra range for spells that aren't strict (the cast has
    // already been started)
    float range_mod = strict ? 1.25f : 6.25f;

    SpellRangeEntry const* srange =
        sSpellRangeStore.LookupEntry(m_spellInfo->rangeIndex);
    float max_range = GetSpellMaxRange(srange) + range_mod;
    float min_range = GetSpellMinRange(srange);

    if (Player* modOwner = m_caster->GetSpellModOwner())
        modOwner->ApplySpellMod(
            m_spellInfo->Id, SPELLMOD_RANGE, max_range, this);

    if (target && target != m_caster)
    {
        // distance from target in checks
        float dist = m_caster->GetCombatDistance(target);

        if (dist > max_range)
            return SPELL_FAILED_OUT_OF_RANGE;
        if (min_range && dist < min_range)
            return SPELL_FAILED_TOO_CLOSE;
        if (m_caster->GetTypeId() == TYPEID_PLAYER &&
            (m_spellInfo->FacingCasterFlags & SPELL_FACING_FLAG_INFRONT) &&
            !m_caster->HasInArc(M_PI_F, target))
            return SPELL_FAILED_UNIT_NOT_INFRONT;
    }

    // TODO verify that such spells really use bounding radius
    if (m_targets.m_targetMask == TARGET_FLAG_DEST_LOCATION &&
        m_targets.m_destX != 0 && m_targets.m_destY != 0 &&
        m_targets.m_destZ != 0)
    {
        if (!m_caster->IsWithinDist3d(m_targets.m_destX, m_targets.m_destY,
                m_targets.m_destZ,
                max_range > 2.0f ? max_range - 2.0f : max_range))
            return SPELL_FAILED_OUT_OF_RANGE;
        if (min_range &&
            m_caster->IsWithinDist3d(m_targets.m_destX, m_targets.m_destY,
                m_targets.m_destZ, min_range))
            return SPELL_FAILED_TOO_CLOSE;
    }

    return SPELL_CAST_OK;
}

SpellCastResult Spell::CheckCasterState(bool strict)
{
    // player-only caster state checks
    if (m_caster->GetTypeId() == TYPEID_PLAYER)
    {
        Player* player_caster = static_cast<Player*>(m_caster);

        // FIXME: Make general case for this spell error. This is just for
        // Hakkar atm.
        if (!trigger_type_.triggered() && !player_caster->InControl() &&
            player_caster->has_aura(150061))
            return SPELL_FAILED_NOT_IN_CONTROL;

        // Don't allow casting any spell while sitting, except those explicitly
        // marked
        // NOTE: From what I know this can only happen if you use a item-casted
        //       spell (such as a mount) at the exact time you use a sit-to-use
        //       spell (like water).
        if (!trigger_type_.triggered() &&
            !m_spellInfo->HasAttribute(SPELL_ATTR_CASTABLE_WHILE_SITTING) &&
            m_caster->IsSitState())
            return SPELL_FAILED_NOT_READY;

        // only allow triggered spells if at an ended battleground
        BattleGround* bg = player_caster->GetBattleGround();
        if (!trigger_type_.triggered() && bg &&
            bg->GetStatus() == STATUS_WAIT_LEAVE)
            return SPELL_FAILED_DONT_REPORT;

        if (m_spellInfo->HasAttribute(SPELL_ATTR_OUTDOORS_ONLY) &&
            !m_caster->GetTerrain()->IsOutdoors(
                m_caster->GetX(), m_caster->GetY(), m_caster->GetZ()))
            return trigger_type_.triggered() ? SPELL_FAILED_DONT_REPORT :
                                               SPELL_FAILED_ONLY_OUTDOORS;

        if (m_spellInfo->HasAttribute(SPELL_ATTR_INDOORS_ONLY) &&
            m_caster->GetTerrain()->IsOutdoors(
                m_caster->GetX(), m_caster->GetY(), m_caster->GetZ()))
            return trigger_type_.triggered() ? SPELL_FAILED_DONT_REPORT :
                                               SPELL_FAILED_ONLY_INDOORS;

        // don't let players cast spells on mount
        if (m_caster->IsMounted() && !trigger_type_.triggered() &&
            !IsPassiveSpell(m_spellInfo) &&
            !m_spellInfo->HasAttribute(SPELL_ATTR_CASTABLE_WHILE_MOUNTED))
        {
            if (m_caster->IsTaxiFlying())
                return SPELL_FAILED_NOT_ON_TAXI;
            else
                return SPELL_FAILED_NOT_MOUNTED;
        }
    }

    // any unit caster state checks

    if (!trigger_type_.triggered() && IsNonCombatSpell(m_spellInfo) &&
        m_caster->isInCombat())
        return SPELL_FAILED_AFFECTING_COMBAT;

    if (m_spellInfo->HasAttribute(SPELL_ATTR_CUSTOM_DISABLE_ON_TRANSPORT) &&
        m_caster->GetTransport())
        return SPELL_FAILED_NOT_ON_TRANSPORT;

    // only check at first call, stealth auras are already removed at second
    // call
    if (strict && trigger_type_.check_stances())
    {
        // cannot be used in this stance/form
        SpellCastResult shapeError = GetErrorAtShapeshiftedCast(
            m_spellInfo, m_caster->GetShapeshiftForm());
        if (shapeError != SPELL_CAST_OK)
            return !trigger_type_.triggered() ? shapeError :
                                                SPELL_FAILED_DONT_REPORT;

        if (m_spellInfo->HasAttribute(SPELL_ATTR_ONLY_STEALTHED) &&
            !(m_caster->HasStealthAura()))
            return !trigger_type_.triggered() ? SPELL_FAILED_ONLY_STEALTHED :
                                                SPELL_FAILED_DONT_REPORT;
    }

    // caster aura state requirements
    if (m_spellInfo->CasterAuraState &&
        !m_caster->HasAuraState(AuraState(m_spellInfo->CasterAuraState)))
        return SPELL_FAILED_CASTER_AURASTATE;
    if (m_spellInfo->CasterAuraStateNot &&
        m_caster->HasAuraState(AuraState(m_spellInfo->CasterAuraStateNot)))
        return SPELL_FAILED_CASTER_AURASTATE;

    // zone checks
    // update zone/area cache for taxi spells (such as abyssal mission)
    if (m_spellInfo->AreaId != 0 && m_caster->IsTaxiFlying())
        m_caster->UpdateZoneAreaCache();

    uint32 zone, area;
    m_caster->GetZoneAndAreaId(zone, area);

    SpellCastResult locRes =
        sSpellMgr::Instance()->GetSpellAllowedInLocationError(m_spellInfo,
            m_caster->GetMapId(), zone, area,
            m_caster->GetCharmerOrOwnerPlayerOrPlayerItself());
    if (locRes != SPELL_CAST_OK)
        return locRes;

    // Fishing (pre 3.1 we cannot fish in zones above our skill level)
    if (m_spellInfo->HasTargetType(TARGET_SELF_FISHING))
    {
        int32 zone_skill =
            sObjectMgr::Instance()->GetFishingBaseSkillLevel(area);
        if (!zone_skill)
            zone_skill = sObjectMgr::Instance()->GetFishingBaseSkillLevel(zone);
        if (m_caster->GetTypeId() != TYPEID_PLAYER || zone_skill == 0 ||
            static_cast<int32>(static_cast<Player*>(m_caster)->GetSkillValue(
                SKILL_FISHING)) < zone_skill)
            return SPELL_FAILED_LOW_CASTLEVEL;
    }

    return SPELL_CAST_OK;
}

SpellCastResult Spell::CheckCastTarget()
{
    if (Unit* target = m_targets.getUnitTarget())
    {
        // totems cannot be single-target buffed
        if (target->GetTypeId() == TYPEID_UNIT &&
            static_cast<Creature*>(target)->IsTotem() &&
            m_spellInfo->HasEffect(SPELL_EFFECT_APPLY_AURA) &&
            IsPositiveSpell(m_spellInfo) && !trigger_type_.triggered() &&
            !m_spellInfo->HasEffect(SPELL_EFFECT_CHARGE))
            return SPELL_FAILED_BAD_TARGETS;

        if (target != m_caster &&
            m_spellInfo->HasAttribute(SPELL_ATTR_EX3_ONLY_TARGET_PLAYERS) &&
            target->GetTypeId() != TYPEID_PLAYER &&
            !IsAreaOfEffectSpell(m_spellInfo))
            return SPELL_FAILED_BAD_TARGETS;

        // target state requirements (not allowed state), apply to self also
        if (m_spellInfo->TargetAuraStateNot &&
            target->HasAuraState(AuraState(m_spellInfo->TargetAuraStateNot)))
            return SPELL_FAILED_TARGET_AURASTATE;

        // Caster state requirements
        if (m_spellInfo->CasterAuraState &&
            !m_caster->HasAuraState(
                static_cast<AuraState>(m_spellInfo->CasterAuraState)))
            return SPELL_FAILED_TARGET_AURASTATE;

        // dead/alive state
        if (!trigger_type_.triggered() && !target->isAlive() &&
            !CanBeCastOnDeadTargets(m_spellInfo))
            return SPELL_FAILED_TARGETS_DEAD;
        if (!trigger_type_.triggered() && target->isAlive() &&
            IsDeathOnlySpell(m_spellInfo))
            return SPELL_FAILED_TARGET_NOT_DEAD;

        // Spells that cannot have yourself as main target (Mind Vision
        // exception)
        if (m_spellInfo->HasAttribute(SPELL_ATTR_CUSTOM_CANT_TARGET_SELF) &&
            m_caster == target && m_spellInfo->Id != 2096 &&
            m_spellInfo->Id != 10909)
            return SPELL_FAILED_BAD_TARGETS;

        // Spirit of Redemption
        uint32 id = m_spellInfo->Id;
        if (target->GetShapeshiftForm() == FORM_SPIRITOFREDEMPTION &&
            (id != 20711 && id != 27792 && id != 27795 && id != 27827 &&
                id != 32343 &&
                !(m_spellInfo->SpellFamilyName == SPELLFAMILY_PRIEST &&
                    m_spellInfo->SpellFamilyFlags & (0x200 | 0x10000000))))
            return SPELL_FAILED_BAD_TARGETS;

        // Recently Bandaged
        if (m_spellInfo->Mechanic == MECHANIC_BANDAGE &&
            target->IsImmuneToMechanic(MECHANIC_BANDAGE))
            return SPELL_FAILED_CASTER_AURASTATE;

        // Overpower
        if (!trigger_type_.triggered() &&
            m_spellInfo->HasAttribute(SPELL_ATTR_EX_REQ_TARGET_COMBO_POINTS) &&
            m_spellInfo->SpellFamilyName == SPELLFAMILY_WARRIOR &&
            m_caster->overpower_target != target->GetObjectGuid())
            return SPELL_FAILED_CASTER_AURASTATE;

        bool non_caster_target =
            target != m_caster &&
            !IsSpellWithCasterSourceTargetsOnly(m_spellInfo);

        if (non_caster_target)
        {
            // target state requirements (apply to non-self only), to allow cast
            // affects to self like Dirty Deeds
            if (m_spellInfo->TargetAuraState &&
                !target->HasAuraStateForCaster(
                    AuraState(m_spellInfo->TargetAuraState),
                    m_caster->GetObjectGuid()))
                return SPELL_FAILED_TARGET_AURASTATE;

            // Not allow casting on flying player
            if (target->IsTaxiFlying())
                return SPELL_FAILED_BAD_TARGETS;

            if (!trigger_type_.triggered() &&
                !m_spellInfo->HasAttribute(SPELL_ATTR_CUSTOM1_IGNORES_LOS) &&
                !m_caster->IsWithinWmoLOSInMap(target))
                return SPELL_FAILED_LINE_OF_SIGHT;

            // auto selection spell rank implemented in
            // WorldSession::HandleCastSpellOpcode
            // this case can be triggered if rank not found (too low-level
            // target for first rank)
            if (m_caster->GetTypeId() == TYPEID_PLAYER && !m_CastItem &&
                !trigger_type_.triggered())
            {
                // spell expected to be auto-downranking in cast handle, so must
                // be same
                if (m_spellInfo !=
                    sSpellMgr::Instance()->SelectAuraRankForLevel(
                        m_spellInfo, target->getLevel()))
                    return SPELL_FAILED_LOWLEVEL;
            }

            // No spells can ever be cast on a target off the opposing team in a
            // sanctuary
            if (Player* target_player =
                    target->GetCharmerOrOwnerPlayerOrPlayerItself())
            {
                if (target_player->HasFlag(
                        PLAYER_FLAGS, PLAYER_FLAGS_SANCTUARY))
                {
                    if (Player* caster_player =
                            m_caster->GetCharmerOrOwnerPlayerOrPlayerItself())
                        if (caster_player->GetTeam() !=
                            target_player->GetTeam())
                            return trigger_type_.triggered() ?
                                       SPELL_FAILED_DONT_REPORT :
                                       SPELL_FAILED_BAD_TARGETS;
                }
            }
        }
        else if (m_caster == target)
        {
            if (m_caster->GetTypeId() == TYPEID_PLAYER && m_caster->IsInWorld())
            {
                // Additional check for some spells
                // If 0 spell effect empty - client not send target data (need
                // use selection)
                // TODO: check it on next client version
                if (m_targets.m_targetMask == TARGET_FLAG_SELF &&
                    m_spellInfo->EffectImplicitTargetA[EFFECT_INDEX_1] ==
                        TARGET_CHAIN_DAMAGE)
                {
                    target = m_caster->GetMap()->GetUnit(
                        ((Player*)m_caster)->GetSelectionGuid());
                    if (!target)
                        return SPELL_FAILED_BAD_TARGETS;

                    m_targets.setUnitTarget(target);
                }
            }

            // Some special spells with non-caster only mode

            // Fire Shield
            if (m_spellInfo->SpellFamilyName == SPELLFAMILY_WARLOCK &&
                m_spellInfo->SpellIconID == 16)
                return SPELL_FAILED_BAD_TARGETS;

            // Shadow Vulnerability and Blackout can not target caster, but the
            // code for them to proc
            // is fairly spread out, so rather than stopping their proc we just
            // condemn the cast
            if (m_spellInfo->Id == 15258 || m_spellInfo->Id == 15269)
                return SPELL_FAILED_DONT_REPORT;

            // Spells with TARGET_PET (e.g. health funnel) that care about LoS
            if (m_spellInfo->HasTargetType(TARGET_PET))
            {
                Unit* tmp = m_caster->GetPet();
                if (!tmp)
                    tmp = m_caster->GetCharm();
                if (tmp && !trigger_type_.triggered() &&
                    !m_spellInfo->HasAttribute(
                        SPELL_ATTR_CUSTOM1_IGNORES_LOS) &&
                    !m_caster->IsWithinWmoLOSInMap(tmp))
                    return SPELL_FAILED_LINE_OF_SIGHT;
            }
        }

        // check pet presents
        bool PosAndNeg = false;
        for (int j = 0; j < MAX_EFFECT_INDEX; ++j)
        {
            if (m_spellInfo->EffectImplicitTargetA[j] == TARGET_PET)
            {
                Unit* pet = m_caster->GetPet();
                if (!pet)
                    pet = m_caster->GetCharm();
                if (!pet)
                {
                    if (m_triggeredByAuraSpell) // not report pet not existence
                                                // for triggered spells
                        return SPELL_FAILED_DONT_REPORT;
                    else
                        return SPELL_FAILED_NO_PET;
                }
                else if (!pet->isAlive())
                    return SPELL_FAILED_TARGETS_DEAD;
                break;
            }
            if (m_spellInfo->EffectImplicitTargetA[j] == TARGET_DUELVSPLAYER)
                PosAndNeg = true;
        }

        // check creature type
        // ignore self casts (including area casts when caster selected as
        // target)
        if (non_caster_target)
        {
            if (!CheckTargetCreatureType(target))
            {
                if (target->GetTypeId() == TYPEID_PLAYER)
                    return SPELL_FAILED_TARGET_IS_PLAYER;
                else
                    return SPELL_FAILED_BAD_TARGETS;
            }

            // simple cases
            bool explicit_target_mode = false;
            bool target_hostile = false;
            bool target_hostile_checked = false;
            bool target_friendly = false;
            bool target_friendly_checked = false;
            for (int k = 0; k < MAX_EFFECT_INDEX; ++k)
            {
                if (IsExplicitPositiveTarget(
                        m_spellInfo->EffectImplicitTargetA[k]))
                {
                    if (!target_hostile_checked)
                    {
                        target_hostile_checked = true;
                        target_hostile = m_caster->IsHostileTo(target);
                    }

                    // TODO: Can friendly spells be casted at hostile targets as
                    // part of scripting? At the moment I can think of none, so
                    // not adding it
                    if (target_hostile)
                        return SPELL_FAILED_BAD_TARGETS;

                    explicit_target_mode = true;
                }
                else if (IsExplicitNegativeTarget(
                             m_spellInfo->EffectImplicitTargetA[k]))
                {
                    if (!target_friendly_checked)
                    {
                        target_friendly_checked = true;
                        target_friendly = m_caster->IsFriendlyTo(target);
                    }

                    if (target_friendly)
                        return SPELL_FAILED_BAD_TARGETS;

                    explicit_target_mode = true;
                }
            }
            // TODO: this check can be applied and for player to prevent
            // cheating when IsPositiveSpell will return always correct result.
            // check target for pet/charmed casts (not self targeted), self
            // targeted cast used for area effects and etc
            //   -- HACK: && !PosAndNeg && !(m_spellInfo->Id == 35779) -- hacky
            //   fix, pets dont have many spells - should be reworked though)
            if (!explicit_target_mode && m_caster->GetTypeId() == TYPEID_UNIT &&
                m_caster->GetCharmerOrOwnerGuid() && !PosAndNeg &&
                !(m_spellInfo->Id == 35779))
            {
                // check correctness positive/negative cast target (pet cast
                // real check and cheating check)
                if (IsPositiveSpell(m_spellInfo->Id))
                {
                    if (!target_hostile_checked)
                    {
                        target_hostile_checked = true;
                        target_hostile = m_caster->IsHostileTo(target);
                    }

                    if (target_hostile)
                        return SPELL_FAILED_BAD_TARGETS;
                }
                else
                {
                    if (!target_friendly_checked)
                    {
                        target_friendly_checked = true;
                        target_friendly = m_caster->IsFriendlyTo(target);
                    }

                    if (target_friendly)
                        return SPELL_FAILED_BAD_TARGETS;
                }
            }
        }

        // Must be behind the target.
        if (m_spellInfo->HasAttribute(SPELL_ATTR_CUSTOM_CAST_FROM_BEHIND) &&
            target->HasInArc(M_PI_F, m_caster))
        {
            SendInterrupted(2);
            return SPELL_FAILED_NOT_BEHIND;
        }

        // Target must be facing you.
        if (m_spellInfo->HasAttribute(SPELL_ATTR_CUSTOM_TARGET_IN_FRONT) &&
            !target->HasInArc(M_PI_F, m_caster))
        {
            SendInterrupted(2);
            return SPELL_FAILED_NOT_INFRONT;
        }

        // check if target is in combat
        if (non_caster_target &&
            m_spellInfo->HasAttribute(SPELL_ATTR_EX_NOT_IN_COMBAT_TARGET) &&
            target->isInCombat())
            return SPELL_FAILED_TARGET_AFFECTING_COMBAT;
    }
    else if (m_targets.m_targetMask & TARGET_FLAG_DEST_LOCATION) // else to the
                                                                 // check if we
                                                                 // have a unit
                                                                 // target
    {
        // Check Line of Sight to destination location
        if (!trigger_type_.triggered() &&
            !m_spellInfo->HasAttribute(SPELL_ATTR_CUSTOM1_IGNORES_LOS) &&
            !m_caster->IsWithinWmoLOS(
                m_targets.m_destX, m_targets.m_destY, m_targets.m_destZ + 2.0f))
            return SPELL_FAILED_LINE_OF_SIGHT;
    }
    else if (m_targets.getCorpseTargetGuid()) // else to check if we have a
                                              // corpse target
    {
        Corpse* corpse =
            m_caster->GetMap()->GetCorpse(m_targets.getCorpseTargetGuid());
        if (corpse)
        {
            if (!trigger_type_.triggered() &&
                !m_spellInfo->HasAttribute(SPELL_ATTR_CUSTOM1_IGNORES_LOS) &&
                !m_caster->IsWithinWmoLOSInMap(corpse))
                return SPELL_FAILED_LINE_OF_SIGHT;
        }
    }

    return SPELL_CAST_OK;
}

SpellCastResult Spell::CheckItems()
{
    if (m_caster->GetTypeId() != TYPEID_PLAYER)
        return SPELL_CAST_OK;

    Player* p_caster = (Player*)m_caster;

    // cast item checks
    if (m_CastItem)
    {
        if (m_CastItem->in_trade())
            return SPELL_FAILED_ITEM_NOT_FOUND;

        uint32 itemid = m_CastItem->GetEntry();
        if (!p_caster->HasItemCount(itemid, 1))
            return SPELL_FAILED_ITEM_NOT_FOUND;

        ItemPrototype const* proto = m_CastItem->GetProto();
        if (!proto)
            return SPELL_FAILED_ITEM_NOT_FOUND;

        for (int i = 0; i < 5; ++i)
            if (proto->Spells[i].SpellCharges)
            {
                if (m_CastItem->GetSpellCharges(i) == 0)
                    return SPELL_FAILED_NO_CHARGES_REMAIN;

                // If spell may consume item, no other spell may reference it
                // If triggered and does not create item, allow skipping this
                // condition
                if (m_CastItem->already_referenced(this) &&
                    !(trigger_type_.triggered() &&
                        !m_spellInfo->HasEffect(SPELL_EFFECT_CREATE_ITEM)))
                    return SPELL_FAILED_ITEM_NOT_READY;
            }

        // consumable cast item checks
        if (proto->Class == ITEM_CLASS_CONSUMABLE && m_targets.getUnitTarget())
        {
            // such items should only fail if there is no suitable effect at all
            // - see Rejuvenation Potions for example
            SpellCastResult failReason = SPELL_CAST_OK;
            for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                // skip check, pet not required like checks, and for TARGET_PET
                // m_targets.getUnitTarget() is not the real target but the
                // caster
                if (m_spellInfo->EffectImplicitTargetA[i] == TARGET_PET)
                    continue;

                if (m_spellInfo->Effect[i] == SPELL_EFFECT_HEAL)
                {
                    if (m_targets.getUnitTarget()->GetHealth() ==
                        m_targets.getUnitTarget()->GetMaxHealth())
                    {
                        failReason = SPELL_FAILED_ALREADY_AT_FULL_HEALTH;
                        continue;
                    }
                    else
                    {
                        failReason = SPELL_CAST_OK;
                        break;
                    }
                }

                // Mana Potion, Rage Potion, Thistle Tea(Rogue), ...
                if (m_spellInfo->Effect[i] == SPELL_EFFECT_ENERGIZE)
                {
                    if (m_spellInfo->EffectMiscValue[i] < 0 ||
                        m_spellInfo->EffectMiscValue[i] >= MAX_POWERS)
                    {
                        failReason = SPELL_FAILED_ALREADY_AT_FULL_POWER;
                        continue;
                    }

                    Powers power = Powers(m_spellInfo->EffectMiscValue[i]);
                    if (m_targets.getUnitTarget()->GetPower(power) ==
                        m_targets.getUnitTarget()->GetMaxPower(power))
                    {
                        failReason = SPELL_FAILED_ALREADY_AT_FULL_POWER;
                        continue;
                    }
                    else
                    {
                        failReason = SPELL_CAST_OK;
                        break;
                    }
                }
            }
            if (failReason != SPELL_CAST_OK)
                return failReason;
        }
    }

    // check target item (for triggered case not report error)
    if (m_targets.getItemTargetGuid())
    {
        if (m_caster->GetTypeId() != TYPEID_PLAYER)
            return trigger_type_.triggered() &&
                           !(m_targets.m_targetMask & TARGET_FLAG_TRADE_ITEM) ?
                       SPELL_FAILED_DONT_REPORT :
                       SPELL_FAILED_BAD_TARGETS;

        if (!m_targets.getItemTarget())
            return trigger_type_.triggered() &&
                           !(m_targets.m_targetMask & TARGET_FLAG_TRADE_ITEM) ?
                       SPELL_FAILED_DONT_REPORT :
                       SPELL_FAILED_ITEM_GONE;

        if (!m_targets.getItemTarget()->IsFitToSpellRequirements(m_spellInfo))
            return trigger_type_.triggered() &&
                           !(m_targets.m_targetMask & TARGET_FLAG_TRADE_ITEM) ?
                       SPELL_FAILED_DONT_REPORT :
                       SPELL_FAILED_EQUIPPED_ITEM_CLASS;
    }
    // if not item target then required item must be equipped (for triggered
    // case not report error)
    else
    {
        if (m_caster->GetTypeId() == TYPEID_PLAYER &&
            !((Player*)m_caster)->HasItemFitToSpellReqirements(m_spellInfo))
            return trigger_type_.triggered() ? SPELL_FAILED_DONT_REPORT :
                                               SPELL_FAILED_EQUIPPED_ITEM_CLASS;
    }

    // check spell focus object
    if (m_spellInfo->RequiresSpellFocus)
    {
        auto ok = maps::visitors::yield_single<GameObject>{}(m_caster,
            m_caster->GetMap()->GetVisibilityDistance(), [this](GameObject* go)
            {
                if (go->GetGOInfo()->type != GAMEOBJECT_TYPE_SPELL_FOCUS)
                    return false;

                if (go->GetGOInfo()->spellFocus.focusId !=
                    m_spellInfo->RequiresSpellFocus)
                    return false;

                float dist = (float)go->GetGOInfo()->spellFocus.dist;

                return go->IsWithinDistInMap(m_caster, dist);
            });

        if (!ok)
            return SPELL_FAILED_REQUIRES_SPELL_FOCUS;

        focusObject = ok; // game object found in range
    }

    // check reagents (ignore triggered spells with reagents processed by
    // original spell) and special reagent ignore case.
    if (!IgnoreItemRequirements())
    {
        // We don't take reagents if spell is "preparation proof" and we're
        // currently preparing
        if (!(m_spellInfo->HasAttribute(SPELL_ATTR_EX5_NO_REAGENT_WHILE_PREP) &&
                m_caster->HasAuraType(SPELL_AURA_ARENA_PREPARATION)))
        {
            for (uint32 i = 0; i < MAX_SPELL_REAGENTS; ++i)
            {
                if (m_spellInfo->Reagent[i] <= 0)
                    continue;

                uint32 itemid = m_spellInfo->Reagent[i];
                uint32 itemcount = m_spellInfo->ReagentCount[i];

                // if CastItem is also spell reagent
                if (m_CastItem && m_CastItem->GetEntry() == itemid)
                {
                    ItemPrototype const* proto = m_CastItem->GetProto();
                    if (!proto)
                        return SPELL_FAILED_REAGENTS;
                    for (int s = 0; s < MAX_ITEM_PROTO_SPELLS; ++s)
                    {
                        // CastItem will be used up and does not count as
                        // reagent
                        int32 charges = m_CastItem->GetSpellCharges(s);
                        if (proto->Spells[s].SpellCharges < 0 &&
                            !(proto->ExtraFlags & ITEM_EXTRA_NON_CONSUMABLE) &&
                            abs(charges) < 2)
                        {
                            ++itemcount;
                            break;
                        }
                    }
                }

                if (!p_caster->HasItemCount(itemid, itemcount))
                    return SPELL_FAILED_REAGENTS;
            }
        }

        // check totem-item requirements (items presence in inventory)
        uint32 totems = MAX_SPELL_TOTEMS;
        for (int i = 0; i < MAX_SPELL_TOTEMS; ++i)
        {
            if (m_spellInfo->Totem[i] != 0)
            {
                if (p_caster->HasItemCount(m_spellInfo->Totem[i], 1))
                {
                    totems -= 1;
                    continue;
                }
            }
            else
                totems -= 1;
        }

        if (totems != 0)
            return SPELL_FAILED_TOTEMS;

        // Check items for TotemCategory  (items presence in inventory)
        uint32 TotemCategory = MAX_SPELL_TOTEM_CATEGORIES;
        for (int i = 0; i < MAX_SPELL_TOTEM_CATEGORIES; ++i)
        {
            if (m_spellInfo->TotemCategory[i] != 0)
            {
                if (p_caster->HasItemTotemCategory(
                        m_spellInfo->TotemCategory[i]))
                {
                    TotemCategory -= 1;
                    continue;
                }
            }
            else
                TotemCategory -= 1;
        }

        if (TotemCategory != 0)
            return SPELL_FAILED_TOTEM_CATEGORY;
    }

    // special checks for spell effects
    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        switch (m_spellInfo->Effect[i])
        {
        case SPELL_EFFECT_CREATE_ITEM:
        {
            Unit* target = m_targets.getUnitTarget();
            if (!trigger_type_.triggered() && m_spellInfo->EffectItemType[i] &&
                target && target->GetTypeId() == TYPEID_PLAYER)
            {
                /* XXX */
                inventory::transaction trans;
                trans.add(m_spellInfo->EffectItemType[i], 1);
                Player* player_target = static_cast<Player*>(target);
                if (!player_target->storage().verify(trans))
                {
                    player_target->SendEquipError(
                        static_cast<InventoryResult>(trans.error()), nullptr,
                        nullptr, m_spellInfo->EffectItemType[i]);
                    return SPELL_FAILED_DONT_REPORT;
                }
            }
            break;
        }
        case SPELL_EFFECT_ENCHANT_ITEM:
        {
            Item* targetItem = m_targets.getItemTarget();
            if (!targetItem)
                return SPELL_FAILED_ITEM_NOT_FOUND;

            if (targetItem->GetProto()->ItemLevel < m_spellInfo->baseLevel)
                return SPELL_FAILED_LOWLEVEL;
            // Not allow enchant in trade slot for some enchant type
            if (targetItem->GetOwner() != m_caster)
            {
                uint32 enchant_id = m_spellInfo->EffectMiscValue[i];
                SpellItemEnchantmentEntry const* pEnchant =
                    sSpellItemEnchantmentStore.LookupEntry(enchant_id);
                if (!pEnchant)
                    return SPELL_FAILED_ERROR;
                if (pEnchant->slot & ENCHANTMENT_CAN_SOULBOUND)
                    return SPELL_FAILED_NOT_TRADEABLE;
            }
            break;
        }
        case SPELL_EFFECT_ENCHANT_ITEM_TEMPORARY:
        {
            Item* item = m_targets.getItemTarget();
            if (!item)
                return SPELL_FAILED_ITEM_NOT_FOUND;
            // Not allow enchant in trade slot for some enchant type
            if (item->GetOwner() != m_caster)
            {
                uint32 enchant_id = m_spellInfo->EffectMiscValue[i];
                SpellItemEnchantmentEntry const* pEnchant =
                    sSpellItemEnchantmentStore.LookupEntry(enchant_id);
                if (!pEnchant)
                    return SPELL_FAILED_ERROR;
                if (pEnchant->slot & ENCHANTMENT_CAN_SOULBOUND)
                    return SPELL_FAILED_NOT_TRADEABLE;
            }
            break;
        }
        case SPELL_EFFECT_ENCHANT_HELD_ITEM:
            // check item existence in effect code (not output errors at offhand
            // hold item effect to main hand for example
            break;
        case SPELL_EFFECT_DISENCHANT:
        {
            if (!m_targets.getItemTarget())
                return SPELL_FAILED_CANT_BE_DISENCHANTED;

            // prevent disenchanting in trade slot
            if (m_targets.getItemTarget()->GetOwnerGuid() !=
                m_caster->GetObjectGuid())
                return SPELL_FAILED_CANT_BE_DISENCHANTED;

            ItemPrototype const* itemProto =
                m_targets.getItemTarget()->GetProto();
            if (!itemProto)
                return SPELL_FAILED_CANT_BE_DISENCHANTED;

            // must have disenchant loot (other static req. checked at item
            // prototype loading)
            if (!itemProto->DisenchantID)
                return SPELL_FAILED_CANT_BE_DISENCHANTED;

            // 2.0.x addon: Check player enchanting level against the item
            // disenchanting requirements
            int32 item_disenchantskilllevel =
                itemProto->RequiredDisenchantSkill;
            if (item_disenchantskilllevel >
                int32(p_caster->GetSkillValue(SKILL_ENCHANTING)))
                return SPELL_FAILED_LOW_CASTLEVEL;
            break;
        }
        case SPELL_EFFECT_PROSPECTING:
        {
            if (!m_targets.getItemTarget())
                return SPELL_FAILED_CANT_BE_PROSPECTED;
            // ensure item is a prospectable ore
            if (!(m_targets.getItemTarget()->GetProto()->Flags &
                    ITEM_FLAG_PROSPECTABLE))
                return SPELL_FAILED_CANT_BE_PROSPECTED;
            // prevent prospecting in trade slot
            if (m_targets.getItemTarget()->GetOwnerGuid() !=
                m_caster->GetObjectGuid())
                return SPELL_FAILED_CANT_BE_PROSPECTED;
            // Check for enough skill in jewelcrafting
            uint32 item_prospectingskilllevel =
                m_targets.getItemTarget()->GetProto()->RequiredSkillRank;
            if (item_prospectingskilllevel >
                p_caster->GetSkillValue(SKILL_JEWELCRAFTING))
                return SPELL_FAILED_LOW_CASTLEVEL;
            // make sure the player has the required ores in inventory
            if (int32(m_targets.getItemTarget()->GetCount()) <
                CalculateDamage(SpellEffectIndex(i), m_caster))
                return SPELL_FAILED_PROSPECT_NEED_MORE;

            if (!LootTemplates_Prospecting.HaveLootFor(
                    m_targets.getItemTargetEntry()))
                return SPELL_FAILED_CANT_BE_PROSPECTED;

            break;
        }
        case SPELL_EFFECT_WEAPON_DAMAGE:
        case SPELL_EFFECT_WEAPON_DAMAGE_NOSCHOOL:
        {
            if (m_caster->GetTypeId() != TYPEID_PLAYER)
                return SPELL_FAILED_TARGET_NOT_PLAYER;
            if (m_attackType != RANGED_ATTACK)
                break;
            Item* pItem = ((Player*)m_caster)
                              ->GetWeaponForAttack(m_attackType, true, false);
            if (!pItem)
                return SPELL_FAILED_EQUIPPED_ITEM;

            switch (pItem->GetProto()->SubClass)
            {
            case ITEM_SUBCLASS_WEAPON_THROWN:
            {
                uint32 ammo = pItem->GetEntry();
                if (!((Player*)m_caster)->HasItemCount(ammo, 1))
                    return SPELL_FAILED_NO_AMMO;
            };
            break;
            case ITEM_SUBCLASS_WEAPON_GUN:
            case ITEM_SUBCLASS_WEAPON_BOW:
            case ITEM_SUBCLASS_WEAPON_CROSSBOW:
            {
                uint32 ammo =
                    ((Player*)m_caster)->GetUInt32Value(PLAYER_AMMO_ID);
                if (!ammo)
                {
                    // Requires No Ammo
                    if (m_caster->has_aura(46699, SPELL_AURA_DUMMY))
                        break; // skip other checks

                    return SPELL_FAILED_NO_AMMO;
                }

                ItemPrototype const* ammoProto =
                    ObjectMgr::GetItemPrototype(ammo);
                if (!ammoProto)
                    return SPELL_FAILED_NO_AMMO;

                if (ammoProto->Class != ITEM_CLASS_PROJECTILE)
                    return SPELL_FAILED_NO_AMMO;

                // check ammo ws. weapon compatibility
                switch (pItem->GetProto()->SubClass)
                {
                case ITEM_SUBCLASS_WEAPON_BOW:
                case ITEM_SUBCLASS_WEAPON_CROSSBOW:
                    if (ammoProto->SubClass != ITEM_SUBCLASS_ARROW)
                        return SPELL_FAILED_NO_AMMO;
                    break;
                case ITEM_SUBCLASS_WEAPON_GUN:
                    if (ammoProto->SubClass != ITEM_SUBCLASS_BULLET)
                        return SPELL_FAILED_NO_AMMO;
                    break;
                default:
                    return SPELL_FAILED_NO_AMMO;
                }

                if (!((Player*)m_caster)->HasItemCount(ammo, 1))
                    return SPELL_FAILED_NO_AMMO;
            };
            break;
            case ITEM_SUBCLASS_WEAPON_WAND:
                break;
            default:
                break;
            }
            break;
        }
        default:
            break;
        }
    }

    return SPELL_CAST_OK;
}

SpellCastResult Spell::CheckDbTarget()
{
    // Database based targets from spell_target_script

    if (!m_UniqueTargetInfo.empty()) // skip second CheckCast apply (for delayed
                                     // spells for example)
        return SPELL_CAST_OK;

    for (int j = 0; j < MAX_EFFECT_INDEX; ++j)
    {
        if (m_spellInfo->EffectImplicitTargetA[j] == TARGET_SCRIPT ||
            (m_spellInfo->EffectImplicitTargetB[j] == TARGET_SCRIPT &&
                m_spellInfo->EffectImplicitTargetA[j] != TARGET_SELF) ||
            m_spellInfo->EffectImplicitTargetA[j] ==
                TARGET_SCRIPT_COORDINATES ||
            m_spellInfo->EffectImplicitTargetB[j] ==
                TARGET_SCRIPT_COORDINATES ||
            m_spellInfo->EffectImplicitTargetA[j] ==
                TARGET_FOCUS_OR_SCRIPTED_GAMEOBJECT)
        {
            SpellScriptTargetBounds bounds =
                sSpellMgr::Instance()->GetSpellScriptTargetBounds(
                    m_spellInfo->Id);

            if (bounds.first == bounds.second)
            {
                if (m_spellInfo->EffectImplicitTargetA[j] == TARGET_SCRIPT ||
                    m_spellInfo->EffectImplicitTargetB[j] == TARGET_SCRIPT)
                    logging.error(
                        "Spell entry %u, effect %i has "
                        "EffectImplicitTargetA/EffectImplicitTargetB = "
                        "TARGET_SCRIPT, but creature are not defined in "
                        "`spell_script_target`",
                        m_spellInfo->Id, j);

                if (m_spellInfo->EffectImplicitTargetA[j] ==
                        TARGET_SCRIPT_COORDINATES ||
                    m_spellInfo->EffectImplicitTargetB[j] ==
                        TARGET_SCRIPT_COORDINATES)
                    logging.error(
                        "Spell entry %u, effect %i has "
                        "EffectImplicitTargetA/EffectImplicitTargetB = "
                        "TARGET_SCRIPT_COORDINATES, but gameobject or creature "
                        "are not defined in `spell_script_target`",
                        m_spellInfo->Id, j);

                if (m_spellInfo->EffectImplicitTargetA[j] ==
                    TARGET_FOCUS_OR_SCRIPTED_GAMEOBJECT)
                    logging.error(
                        "Spell entry %u, effect %i has "
                        "EffectImplicitTargetA/EffectImplicitTargetB = "
                        "TARGET_FOCUS_OR_SCRIPTED_GAMEOBJECT, but gameobject "
                        "are not defined in `spell_script_target`",
                        m_spellInfo->Id, j);
            }

            SpellRangeEntry const* srange =
                sSpellRangeStore.LookupEntry(m_spellInfo->rangeIndex);
            float range = GetSpellMaxRange(srange);
            if (range > 300)
                range = 300;

            Creature* targetExplicit =
                nullptr; // used for cases where a target is
                         // provided (by script for example)
            Creature* creatureScriptTarget = nullptr;
            GameObject* goScriptTarget = nullptr;

            for (auto i_spellST = bounds.first; i_spellST != bounds.second;
                 ++i_spellST)
            {
                switch (i_spellST->second.type)
                {
                case SPELL_TARGET_TYPE_GAMEOBJECT:
                {
                    GameObject* p_GameObject = nullptr;

                    if (i_spellST->second.targetEntry)
                    {
                        p_GameObject =
                            maps::visitors::yield_best_match<GameObject>{}(
                                m_caster, range,
                                maps::checks::entry_guid{
                                    i_spellST->second.targetEntry, 0});

                        if (p_GameObject)
                        {
                            // remember found target and range, next attempt
                            // will find more near target with another entry
                            creatureScriptTarget = nullptr;
                            goScriptTarget = p_GameObject;
                            range = p_GameObject->GetDistance(m_caster);
                        }
                    }
                    else if (focusObject) // Focus Object
                    {
                        float frange = m_caster->GetDistance(focusObject);
                        if (range >= frange)
                        {
                            creatureScriptTarget = nullptr;
                            goScriptTarget = focusObject;
                            range = frange;
                        }
                    }
                    break;
                }
                case SPELL_TARGET_TYPE_CREATURE:
                case SPELL_TARGET_TYPE_DEAD:
                default:
                {
                    Creature* p_Creature = nullptr;

                    // check if explicit target is provided and check it up
                    // against database valid target entry/state
                    if (Unit* pTarget = m_targets.getUnitTarget())
                    {
                        if (pTarget->GetTypeId() == TYPEID_UNIT &&
                            pTarget->GetEntry() ==
                                i_spellST->second.targetEntry)
                        {
                            if (i_spellST->second.type ==
                                    SPELL_TARGET_TYPE_DEAD &&
                                ((Creature*)pTarget)->IsCorpse())
                            {
                                // always use spellMaxRange, in case
                                // GetLastRange returned different in a previous
                                // pass
                                if (pTarget->IsWithinDistInMap(
                                        m_caster, GetSpellMaxRange(srange)))
                                    targetExplicit = (Creature*)pTarget;
                            }
                            else if (i_spellST->second.type ==
                                         SPELL_TARGET_TYPE_CREATURE &&
                                     pTarget->isAlive())
                            {
                                // always use spellMaxRange, in case
                                // GetLastRange returned different in a previous
                                // pass
                                if (pTarget->IsWithinDistInMap(
                                        m_caster, GetSpellMaxRange(srange)))
                                    targetExplicit = (Creature*)pTarget;
                            }
                        }
                    }

                    // no target provided or it was not valid, so use closest in
                    // range
                    if (!targetExplicit)
                    {
                        p_Creature = maps::visitors::yield_best_match<Creature,
                            Creature, Pet, SpecialVisCreature, TemporarySummon,
                            Totem>{}(m_caster, range,
                            maps::checks::entry_guid{
                                i_spellST->second.targetEntry, 0, m_caster,
                                i_spellST->second.type !=
                                    SPELL_TARGET_TYPE_DEAD});

                        if (p_Creature)
                            range = p_Creature->GetDistance(m_caster);
                    }

                    // always prefer provided target if it's valid
                    if (targetExplicit)
                        creatureScriptTarget = targetExplicit;
                    else if (p_Creature)
                        creatureScriptTarget = p_Creature;

                    if (creatureScriptTarget)
                        goScriptTarget = nullptr;

                    break;
                }
                }
            }

            if (creatureScriptTarget)
            {
                // store coordinates for TARGET_SCRIPT_COORDINATES
                if (m_spellInfo->EffectImplicitTargetA[j] ==
                        TARGET_SCRIPT_COORDINATES ||
                    m_spellInfo->EffectImplicitTargetB[j] ==
                        TARGET_SCRIPT_COORDINATES)
                {
                    m_targets.setDestination(creatureScriptTarget->GetX(),
                        creatureScriptTarget->GetY(),
                        creatureScriptTarget->GetZ());

                    if (m_spellInfo->EffectImplicitTargetA[j] ==
                            TARGET_SCRIPT_COORDINATES &&
                        m_spellInfo->Effect[j] !=
                            SPELL_EFFECT_PERSISTENT_AREA_AURA)
                        AddUnitTarget(
                            creatureScriptTarget, SpellEffectIndex(j));
                }
                // store explicit target for TARGET_SCRIPT
                else
                {
                    if (m_spellInfo->EffectImplicitTargetA[j] ==
                            TARGET_SCRIPT ||
                        m_spellInfo->EffectImplicitTargetB[j] == TARGET_SCRIPT)
                    {
                        AddUnitTarget(
                            creatureScriptTarget, SpellEffectIndex(j));
                        // Set as main target of spell if main target is caster
                        if (m_targets.getUnitTargetGuid() == m_casterGUID)
                            m_targets.setScriptTarget(creatureScriptTarget);
                    }
                }
            }
            else if (goScriptTarget)
            {
                // store coordinates for TARGET_SCRIPT_COORDINATES
                if (m_spellInfo->EffectImplicitTargetA[j] ==
                        TARGET_SCRIPT_COORDINATES ||
                    m_spellInfo->EffectImplicitTargetB[j] ==
                        TARGET_SCRIPT_COORDINATES)
                {
                    m_targets.setDestination(goScriptTarget->GetX(),
                        goScriptTarget->GetY(), goScriptTarget->GetZ());

                    if (m_spellInfo->EffectImplicitTargetA[j] ==
                            TARGET_SCRIPT_COORDINATES &&
                        m_spellInfo->Effect[j] !=
                            SPELL_EFFECT_PERSISTENT_AREA_AURA)
                        AddGOTarget(goScriptTarget, SpellEffectIndex(j));
                }
                // store explicit target for TARGET_FOCUS_OR_SCRIPTED_GAMEOBJECT
                else
                {
                    if (m_spellInfo->EffectImplicitTargetA[j] ==
                            TARGET_FOCUS_OR_SCRIPTED_GAMEOBJECT ||
                        m_spellInfo->EffectImplicitTargetB[j] ==
                            TARGET_FOCUS_OR_SCRIPTED_GAMEOBJECT)
                        AddGOTarget(goScriptTarget, SpellEffectIndex(j));
                }
            }
            // Missing DB Entry or targets for this spellEffect.
            else
            {
                /* For TARGET_FOCUS_OR_SCRIPTED_GAMEOBJECT makes DB targets
                 * optional not required for now
                    * TODO: Makes more research for this target type
                    */
                if (m_spellInfo->EffectImplicitTargetA[j] !=
                    TARGET_FOCUS_OR_SCRIPTED_GAMEOBJECT)
                {
                    // not report target not existence for triggered spells
                    if (m_triggeredByAuraSpell || trigger_type_.triggered())
                        return SPELL_FAILED_DONT_REPORT;
                    else
                        return SPELL_FAILED_OUT_OF_RANGE;
                }
            }
        }
    }

    return SPELL_CAST_OK;
}

SpellCastResult Spell::CheckPower()
{
    // item cast not used power
    if (m_CastItem)
        return SPELL_CAST_OK;

    // health as power used - need check health amount
    if (m_spellInfo->powerType == POWER_HEALTH)
    {
        if (m_caster->GetHealth() <= m_powerCost)
            return SPELL_FAILED_CASTER_AURASTATE;
        return SPELL_CAST_OK;
    }

    // Check valid power type
    if (m_spellInfo->powerType >= MAX_POWERS)
    {
        logging.error("Spell::CheckMana: Unknown power type '%d'",
            m_spellInfo->powerType);
        return SPELL_FAILED_UNKNOWN;
    }

    // Check power amount
    Powers powerType = Powers(m_spellInfo->powerType);
    if (m_caster->GetPower(powerType) < m_powerCost)
        return SPELL_FAILED_NO_POWER;

    return SPELL_CAST_OK;
}

SpellCastResult Spell::CheckCasterAuras() const
{
    // Flag drop spells totally immuned to caster auras
    // FIXME: find more nice check for all totally immuned spells
    // HasAttribute(SPELL_ATTR_EX3_UNK28) ?
    if (m_spellInfo->Id == 23336 || // Alliance Flag Drop
        m_spellInfo->Id == 23334 || // Horde Flag Drop
        m_spellInfo->Id == 34991)   // Summon Netherstorm Flag
        return SPELL_CAST_OK;

    uint8 school_immune = 0;
    uint32 mechanic_immune = 0;
    uint32 dispel_immune = 0;

    // Check if the spell grants school or mechanic immunity.
    // We use bitmasks so the loop is done only once and not on every aura check
    // below.
    if (m_spellInfo->HasAttribute(SPELL_ATTR_EX_DISPEL_AURAS_ON_IMMUNITY))
    {
        for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
        {
            if (m_spellInfo->EffectApplyAuraName[i] ==
                SPELL_AURA_SCHOOL_IMMUNITY)
                school_immune |= uint32(m_spellInfo->EffectMiscValue[i]);
            else if (m_spellInfo->EffectApplyAuraName[i] ==
                     SPELL_AURA_MECHANIC_IMMUNITY)
                mechanic_immune |=
                    1 << uint32(m_spellInfo->EffectMiscValue[i] - 1);
            else if (m_spellInfo->EffectApplyAuraName[i] ==
                     SPELL_AURA_MECHANIC_IMMUNITY_MASK)
                mechanic_immune |= uint32(m_spellInfo->EffectMiscValue[i]);
            else if (m_spellInfo->EffectApplyAuraName[i] ==
                     SPELL_AURA_DISPEL_IMMUNITY)
                dispel_immune |=
                    GetDispelMask(DispelType(m_spellInfo->EffectMiscValue[i]));
        }

        // immune movement impairment and loss of control (spell data have
        // special structure for mark this case)
        if (IsSpellRemoveAllMovementAndControlLossEffects(m_spellInfo))
            mechanic_immune =
                IMMUNE_TO_MOVEMENT_IMPAIRMENT_AND_LOSS_CONTROL_MASK;
    }

    // Check whether the cast should be prevented by any state you might have.
    SpellCastResult prevented_reason = SPELL_CAST_OK;
    // Have to check if there is a stun aura. Otherwise will have problems with
    // ghost aura apply while logging out
    uint32 unitflag =
        m_caster->GetUInt32Value(UNIT_FIELD_FLAGS); // Get unit state
    if (unitflag & UNIT_FLAG_STUNNED)
    {
        // is spell usable while stunned?
        // stun, knockout, freeze and sleep mechanic counterable by this
        // attribute (as proven by Druid's Barkskin)
        if (m_spellInfo->HasAttribute(SPELL_ATTR_EX5_USABLE_WHILE_STUNNED))
        {
            bool is_stun_mechanic = true;
            auto& stunAuras = m_caster->GetAurasByType(SPELL_AURA_MOD_STUN);
            for (const auto& stunAura : stunAuras)
                if (!(stunAura)->HasMechanic(MECHANIC_STUN) &&
                    !(stunAura)->HasMechanic(MECHANIC_KNOCKOUT) &&
                    !(stunAura)->HasMechanic(MECHANIC_FREEZE) &&
                    !(stunAura)->HasMechanic(MECHANIC_SLEEP))
                {
                    is_stun_mechanic = false;
                    break;
                }
            if (!is_stun_mechanic)
                prevented_reason = SPELL_FAILED_STUNNED;
        }
        else
            prevented_reason = SPELL_FAILED_STUNNED;
    }
    else if (unitflag & UNIT_FLAG_CONFUSED &&
             !m_spellInfo->HasAttribute(SPELL_ATTR_EX5_USABLE_WHILE_CONFUSED))
        prevented_reason = SPELL_FAILED_CONFUSED;
    else if (unitflag & UNIT_FLAG_FLEEING &&
             !m_spellInfo->HasAttribute(SPELL_ATTR_EX5_USABLE_WHILE_FEARED))
        prevented_reason = SPELL_FAILED_FLEEING;
    else if (unitflag & UNIT_FLAG_SILENCED &&
             m_spellInfo->PreventionType == SPELL_PREVENTION_TYPE_SILENCE)
        prevented_reason = SPELL_FAILED_SILENCED;
    else if (unitflag & UNIT_FLAG_PACIFIED &&
             m_spellInfo->PreventionType == SPELL_PREVENTION_TYPE_PACIFY)
        prevented_reason = SPELL_FAILED_PACIFIED;

    // Attr must make flag drop spell totally immune from all effects
    if (prevented_reason != SPELL_CAST_OK)
    {
        if (school_immune || mechanic_immune || dispel_immune)
        {
            // Checking auras is needed now, because you are prevented by some
            // state but the spell grants immunity.
            SpellCastResult ret_val = SPELL_CAST_OK;
            m_caster->loop_auras([&](AuraHolder* holder)
                {
                    const SpellEntry* proto = holder->GetSpellProto();

                    if ((GetSpellSchoolMask(proto) & school_immune) &&
                        !proto->HasAttribute(
                            SPELL_ATTR_EX_UNAFFECTED_BY_SCHOOL_IMMUNE))
                        return true; // continue
                    if ((1 << proto->Dispel) & dispel_immune)
                        return true; // continue

                    for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
                    {
                        Aura* aura = holder->GetAura(SpellEffectIndex(i));
                        if (!aura)
                            continue;

                        if (GetSpellMechanicMask(proto, 1 << i) &
                            mechanic_immune)
                            continue;

                        // Make a second check for spell failed so the right
                        // SPELL_FAILED message is returned.
                        // That is needed when your casting is prevented by
                        // multiple states and you are only immune to some of
                        // them.
                        switch (aura->GetModifier()->m_auraname)
                        {
                        case SPELL_AURA_MOD_STUN:
                            if (!(m_spellInfo->AttributesEx5 &
                                    SPELL_ATTR_EX5_USABLE_WHILE_STUNNED) ||
                                !aura->HasMechanic(MECHANIC_STUN))
                                ret_val = SPELL_FAILED_STUNNED;
                            break;
                        case SPELL_AURA_MOD_CONFUSE:
                            if (!m_spellInfo->HasAttribute(
                                    SPELL_ATTR_EX5_USABLE_WHILE_CONFUSED))
                                ret_val = SPELL_FAILED_CONFUSED;
                            break;
                        case SPELL_AURA_MOD_FEAR:
                            if (!m_spellInfo->HasAttribute(
                                    SPELL_ATTR_EX5_USABLE_WHILE_FEARED))
                                ret_val = SPELL_FAILED_FLEEING;
                            break;
                        case SPELL_AURA_MOD_SILENCE:
                        case SPELL_AURA_MOD_PACIFY:
                        case SPELL_AURA_MOD_PACIFY_SILENCE:
                            if (m_spellInfo->PreventionType ==
                                SPELL_PREVENTION_TYPE_PACIFY)
                                ret_val = SPELL_FAILED_PACIFIED;
                            else if (m_spellInfo->PreventionType ==
                                     SPELL_PREVENTION_TYPE_SILENCE)
                                ret_val = SPELL_FAILED_SILENCED;
                            break;
                        default:
                            break;
                        }
                    }
                    return ret_val == SPELL_CAST_OK; // continue while ret_val
                                                     // is non-erroneous
                });

            if (ret_val != SPELL_CAST_OK)
                return ret_val;
        }
        // You are prevented from casting and the spell casted does not grant
        // immunity. Return a failed error.
        else
            return prevented_reason;
    }

    return SPELL_CAST_OK;
}

SpellCastResult Spell::CheckEffects(bool strict)
{
    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        // for effects of spells that have only one target
        switch (m_spellInfo->Effect[i])
        {
        case SPELL_EFFECT_DUMMY:
        {
            if (m_spellInfo->SpellIconID == 1648) // Execute
            {
                if (!m_targets.getUnitTarget() ||
                    m_targets.getUnitTarget()->GetHealth() >
                        m_targets.getUnitTarget()->GetMaxHealth() * 0.2)
                    return SPELL_FAILED_BAD_TARGETS;
            }
            else if (m_spellInfo->Id == 51582) // Rocket Boots Engaged
            {
                if (m_caster->IsInWater())
                    return SPELL_FAILED_ONLY_ABOVEWATER;
            }
            else if (m_spellInfo->SpellIconID == 156) // Holy Shock
            {
                // spell different for friends and enemies
                // hart version required facing
                if (m_targets.getUnitTarget() &&
                    !m_caster->IsFriendlyTo(m_targets.getUnitTarget()) &&
                    !m_caster->HasInArc(M_PI_F, m_targets.getUnitTarget()))
                    return SPELL_FAILED_UNIT_NOT_INFRONT;
                // Can't cast it on other friendly players dueling
                if (Unit* target = m_targets.getUnitTarget())
                {
                    if (target != m_caster &&
                        m_caster->GetTypeId() == TYPEID_PLAYER &&
                        target->GetTypeId() == TYPEID_PLAYER)
                    {
                        Player* ptarget = static_cast<Player*>(target);
                        Player* pcaster = static_cast<Player*>(m_caster);
                        if (ptarget->GetTeam() == pcaster->GetTeam() &&
                            !pcaster->IsInDuelWith(ptarget) && ptarget->duel)
                            return SPELL_FAILED_BAD_TARGETS;
                    }
                }
            }
            break;
        }
        case SPELL_EFFECT_SCHOOL_DAMAGE:
        {
            // Hammer of Wrath
            if (m_spellInfo->SpellVisual == 7250)
            {
                if (!m_targets.getUnitTarget())
                    return SPELL_FAILED_BAD_IMPLICIT_TARGETS;

                if (m_targets.getUnitTarget()->GetHealth() >
                    m_targets.getUnitTarget()->GetMaxHealth() * 0.2)
                    return SPELL_FAILED_BAD_TARGETS;
            }
            break;
        }
        case SPELL_EFFECT_TAMECREATURE:
        {
            // Spell can be triggered, we need to check original caster prior to
            // caster
            Unit* caster = GetAffectiveCaster();
            if (!caster || caster->GetTypeId() != TYPEID_PLAYER ||
                !m_targets.getUnitTarget() ||
                m_targets.getUnitTarget()->GetTypeId() == TYPEID_PLAYER)
                return SPELL_FAILED_BAD_TARGETS;

            Player* plrCaster = (Player*)caster;

            bool gmmode = m_triggeredBySpellInfo == nullptr;

            if (gmmode && !ChatHandler(plrCaster).FindCommand("npc tame"))
            {
                plrCaster->SendPetTameFailure(PETTAME_UNKNOWNERROR);
                return SPELL_FAILED_DONT_REPORT;
            }

            if (plrCaster->getClass() != CLASS_HUNTER && !gmmode)
            {
                plrCaster->SendPetTameFailure(PETTAME_UNITSCANTTAME);
                return SPELL_FAILED_DONT_REPORT;
            }

            Creature* target = (Creature*)m_targets.getUnitTarget();

            if (target->IsPet() || target->isCharmed())
            {
                plrCaster->SendPetTameFailure(PETTAME_CREATUREALREADYOWNED);
                return SPELL_FAILED_DONT_REPORT;
            }

            if (target->getLevel() > plrCaster->getLevel() && !gmmode)
            {
                plrCaster->SendPetTameFailure(PETTAME_TOOHIGHLEVEL);
                return SPELL_FAILED_DONT_REPORT;
            }

            if (!target->GetCreatureInfo()->isTameable())
            {
                plrCaster->SendPetTameFailure(PETTAME_NOTTAMEABLE);
                return SPELL_FAILED_DONT_REPORT;
            }

            if (target->getVictim() != plrCaster)
            {
                SendCastResult(plrCaster, m_spellInfo, m_cast_count,
                    SPELL_FAILED_TRY_AGAIN);
                return SPELL_FAILED_DONT_REPORT;
            }

            if (plrCaster->GetPetGuid() || plrCaster->GetCharmGuid())
            {
                plrCaster->SendPetTameFailure(PETTAME_ANOTHERSUMMONACTIVE);
                return SPELL_FAILED_DONT_REPORT;
            }

            // We may not have a non-stabled pet when taming a new one
            for (const auto& pet_data : plrCaster->_pet_store)
            {
                if (pet_data.deleted ||
                    (PET_SAVE_FIRST_STABLE_SLOT <= pet_data.slot &&
                        pet_data.slot <= PET_SAVE_LAST_STABLE_SLOT))
                    continue;
                plrCaster->SendPetTameFailure(PETTAME_TOOMANY);
                return SPELL_FAILED_DONT_REPORT;
            }

            break;
        }
        case SPELL_EFFECT_ENERGIZE:
        {
            // Consume Magic
            if (m_spellInfo->Id == 32676 && !consume_magic_buff(m_caster))
                return SPELL_FAILED_NOTHING_TO_DISPEL; // FIXME: Not sure this
                                                       // is the correct error
            break;
        }
        case SPELL_EFFECT_LEARN_SPELL:
        {
            if (m_spellInfo->EffectImplicitTargetA[i] != TARGET_PET)
                break;

            Pet* pet = m_caster->GetPet();

            if (!pet)
                return SPELL_FAILED_NO_PET;

            SpellEntry const* learn_spellproto =
                sSpellStore.LookupEntry(m_spellInfo->EffectTriggerSpell[i]);

            if (!learn_spellproto)
                return SPELL_FAILED_NOT_KNOWN;

            if (!pet->CanTakeMoreActiveSpells(learn_spellproto->Id))
                return SPELL_FAILED_TOO_MANY_SKILLS;

            if (m_spellInfo->spellLevel > pet->getLevel())
                return SPELL_FAILED_LOWLEVEL;

            if (!pet->HasTPForSpell(learn_spellproto->Id))
                return SPELL_FAILED_TRAINING_POINTS;

            break;
        }
        case SPELL_EFFECT_DISPEL:
        {
            Unit* target = m_targets.getUnitTarget();
            if (!target)
                break;

            if (IsAreaOfEffectSpell(m_spellInfo))
                break;

            // Devour Magic
            if (m_spellInfo->SpellFamilyName == SPELLFAMILY_WARLOCK &&
                m_spellInfo->SpellIconID == 47)
            {
                // Cannot dispel friendly targets duelling
                if (target != m_caster->GetOwner() &&
                    target->GetTypeId() == TYPEID_PLAYER &&
                    m_caster->IsFriendlyTo(target) &&
                    static_cast<Player*>(target)->duel)
                    return SPELL_FAILED_BAD_TARGETS;
            }

            // Don't verify there's something to dispel if we have other
            // important effects
            uint32 dispel_mask = 0;
            bool skip = false;
            for (int i = 0; i < MAX_EFFECT_INDEX && !skip; ++i)
            {
                if (m_spellInfo->Effect[i] == SPELL_EFFECT_NONE)
                    continue;
                if (m_spellInfo->Effect[i] != SPELL_EFFECT_DISPEL)
                    skip = true;
                else
                    dispel_mask |= GetDispelMask(static_cast<DispelType>(
                        m_spellInfo->EffectMiscValue[i]));
            }

            if (skip || dispel_mask == 0 || trigger_type_.triggered())
                break;

            // Check so there's something to dispel
            auto potential = target->get_dispel_buffs(
                1, dispel_mask, false, m_caster, this, nullptr, false, true);
            if (potential.empty())
                return SPELL_FAILED_NOTHING_TO_DISPEL;
            break;
        }
        case SPELL_EFFECT_LEARN_PET_SPELL:
        {
            Pet* pet = m_caster->GetPet();

            if (!pet)
                return SPELL_FAILED_NO_PET;

            SpellEntry const* learn_spellproto =
                sSpellStore.LookupEntry(m_spellInfo->EffectTriggerSpell[i]);

            if (!learn_spellproto)
                return SPELL_FAILED_NOT_KNOWN;

            if (!pet->CanTakeMoreActiveSpells(learn_spellproto->Id))
                return SPELL_FAILED_TOO_MANY_SKILLS;

            if (m_spellInfo->spellLevel > pet->getLevel())
                return SPELL_FAILED_LOWLEVEL;

            if (!pet->HasTPForSpell(learn_spellproto->Id))
                return SPELL_FAILED_TRAINING_POINTS;

            break;
        }
        case SPELL_EFFECT_FEED_PET:
        {
            if (m_caster->GetTypeId() != TYPEID_PLAYER)
                return SPELL_FAILED_BAD_TARGETS;

            Item* foodItem = m_targets.getItemTarget();
            if (!foodItem)
                return SPELL_FAILED_BAD_TARGETS;

            Pet* pet = m_caster->GetPet();

            if (!pet)
                return SPELL_FAILED_NO_PET;

            if (!pet->HaveInDiet(foodItem->GetProto()))
                return SPELL_FAILED_WRONG_PET_FOOD;

            if (!pet->GetCurrentFoodBenefitLevel(
                    foodItem->GetProto()->ItemLevel))
                return SPELL_FAILED_FOOD_LOWLEVEL;

            if (pet->isInCombat())
                return SPELL_FAILED_AFFECTING_COMBAT;

            break;
        }
        case SPELL_EFFECT_POWER_BURN:
        case SPELL_EFFECT_POWER_DRAIN:
        {
            // Can be area effect, Check only for players and not check if
            // target - caster (spell can have multiply drain/burn effects)
            if (m_caster->GetTypeId() == TYPEID_PLAYER)
                if (Unit* target = m_targets.getUnitTarget())
                    if (target != m_caster)
                    {
                        // Targets have power type mana even if they're not mana
                        // users, so if it's mana we need to check max is >= 1
                        bool matching_type = int32(target->getPowerType()) ==
                                             m_spellInfo->EffectMiscValue[i];
                        if (!matching_type ||
                            (target->getPowerType() == POWER_MANA &&
                                target->GetMaxPower(POWER_MANA) == 0))
                            return SPELL_FAILED_BAD_TARGETS;
                    }
            break;
        }
        case SPELL_EFFECT_CHARGE:
        {
            if (m_caster->hasUnitState(UNIT_STAT_ROOT))
                return SPELL_FAILED_ROOTED;

            Unit* target = m_targets.getUnitTarget();
            if (!target)
                return SPELL_FAILED_DONT_REPORT;

            if (path_gen_finished)
            {
                if (pregenerated_path.empty())
                    return SPELL_FAILED_NOPATH;
                break; // We have a path, need not generate a new one
            }

            float max_range = GetSpellMaxRange(
                sSpellRangeStore.LookupEntry(m_spellInfo->rangeIndex));

            auto callback_func =
                [](Unit* u, std::vector<G3D::Vector3> path, uint32 id)
            {
                u->spell_pathgen_callback(std::move(path), id);
            };
            uint32 id = movement::BuildRetailLikePath(pregenerated_path,
                m_caster, target, max_range * 1.6f, callback_func);
            if (!id)
            {
                if (pregenerated_path.empty())
                    return SPELL_FAILED_NOPATH;
            }
            else
            {
                // We need to wait for path-generation to finish
                waiting_for_path = id;
                return SPELL_CAST_SERVERSIDE_SPECIAL_CHARGE;
            }

            break;
        }
        case SPELL_EFFECT_SKINNING:
        {
            if (m_caster->GetTypeId() != TYPEID_PLAYER ||
                !m_targets.getUnitTarget() ||
                m_targets.getUnitTarget()->GetTypeId() != TYPEID_UNIT)
                return SPELL_FAILED_BAD_TARGETS;

            if (!m_targets.getUnitTarget()->HasFlag(
                    UNIT_FIELD_FLAGS, UNIT_FLAG_SKINNABLE))
                return SPELL_FAILED_TARGET_UNSKINNABLE;

            Creature* creature =
                static_cast<Creature*>(m_targets.getUnitTarget());
            if (!creature->GetLootDistributor())
                return SPELL_FAILED_BAD_TARGETS;

            if (creature->GetLootDistributor()->loot_type() != LOOT_SKINNING)
                return SPELL_FAILED_TARGET_NOT_LOOTED;

            // Cannot cast if someone's already looting
            if (creature->GetLootDistributor()->loot_viewers_count())
                return SPELL_FAILED_CANT_CAST_ON_TAPPED;

            uint32 skill = creature->GetCreatureInfo()->GetRequiredLootSkill();

            int32 skillValue = ((Player*)m_caster)->GetSkillValue(skill);
            int32 TargetLevel = m_targets.getUnitTarget()->getLevel();
            int32 ReqValue =
                (skillValue < 100 ? (TargetLevel - 10) * 10 : TargetLevel * 5);
            if (ReqValue > skillValue)
                return SPELL_FAILED_LOW_CASTLEVEL;

            // chance for fail at orange skinning attempt
            if ((m_selfContainer && (*m_selfContainer) == this) &&
                skillValue < sWorld::Instance()->GetConfigMaxSkillValue() &&
                (ReqValue < 0 ? 0 : ReqValue) >
                    irand(skillValue - 25, skillValue + 37))
                return SPELL_FAILED_TRY_AGAIN;

            break;
        }
        case SPELL_EFFECT_OPEN_LOCK_ITEM:
        case SPELL_EFFECT_OPEN_LOCK:
        {
            if (m_caster->GetTypeId() !=
                TYPEID_PLAYER) // only players can open locks, gather etc.
                return SPELL_FAILED_BAD_TARGETS;

            // we need a go target in case of TARGET_GAMEOBJECT (for other
            // targets acceptable GO and items)
            if (m_spellInfo->EffectImplicitTargetA[i] == TARGET_GAMEOBJECT)
            {
                if (!m_targets.getGOTarget())
                    return SPELL_FAILED_BAD_TARGETS;
            }

            // get the lock entry
            uint32 lockId = 0;
            if (GameObject* go = m_targets.getGOTarget())
            {
                // In BattleGround players can use only flags and banners
                if (((Player*)m_caster)->InBattleGround() &&
                    !((Player*)m_caster)->CanUseBattleGroundObject())
                    return SPELL_FAILED_TRY_AGAIN;

                lockId = go->GetGOInfo()->GetLockId();
                if (!lockId)
                    return SPELL_FAILED_ALREADY_OPEN;

                // Must be in range
                float dist = GetSpellMaxRange(
                    sSpellRangeStore.LookupEntry(m_spellInfo->rangeIndex));
                if (dist < INTERACTION_DISTANCE)
                    dist = INTERACTION_DISTANCE;
                dist += 0.5f;
                if (!m_caster->IsWithinDistInMap(go, dist))
                    return SPELL_FAILED_OUT_OF_RANGE;

                if (go->GetGoType() == GAMEOBJECT_TYPE_CHEST)
                {
                    // Chest types (mines, herbs and, ofc, actual chests) must
                    // be spawned and not currently looted
                    if (!go->isSpawned() ||
                        (go->GetLootDistributor() &&
                            go->GetLootDistributor()->loot_viewers_count()))
                        return SPELL_FAILED_CHEST_IN_USE;
                }

                if (!go->isSpawned())
                    return SPELL_FAILED_INTERRUPTED;
            }
            else if (Item* item = m_targets.getItemTarget())
            {
                // Must be the owner, or in a trade (NOTE: We can never retrieve
                // an item based on GUID if it's not owned by us, so this is
                // probably a redundant check)
                if (item->GetOwner() != m_caster &&
                    (m_targets.m_targetMask & TARGET_FLAG_TRADE_ITEM) == 0)
                    return SPELL_FAILED_ITEM_GONE;

                lockId = item->GetProto()->LockID;

                // if already unlocked
                if (!lockId ||
                    item->HasFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_UNLOCKED))
                    return SPELL_FAILED_ALREADY_OPEN;
            }
            else
                return SPELL_FAILED_BAD_TARGETS;

            SkillType skillId = SKILL_NONE;
            int32 reqSkillValue = 0;
            int32 skillValue = 0;

            // check lock compatibility
            SpellCastResult res = CanOpenLock(SpellEffectIndex(i), lockId,
                skillId, reqSkillValue, skillValue);
            if (res != SPELL_CAST_OK)
                return res;

            // failed attempt at gathering orange nodes
            // check at end of cast
            if (skillId != SKILL_NONE &&
                (m_selfContainer && *m_selfContainer == this))
            {
                bool canFailAtMax =
                    skillId != SKILL_HERBALISM && skillId != SKILL_MINING;

                // chance for failure in orange gather / lockpick (gathering
                // skill can't fail at maxskill)
                if ((canFailAtMax ||
                        skillValue <
                            sWorld::Instance()->GetConfigMaxSkillValue()) &&
                    reqSkillValue > irand(skillValue - 25, skillValue + 37))
                    return SPELL_FAILED_TRY_AGAIN;
            }
            break;
        }
        case SPELL_EFFECT_SUMMON_DEAD_PET:
        {
            Creature* pet = m_caster->GetPet();
            if (!pet && !((Player*)m_caster)->HasDeadPet())
                return SPELL_FAILED_NO_PET;

            if (pet && pet->isAlive())
                return SPELL_FAILED_ALREADY_HAVE_SUMMON;

            break;
        }
        // This is generic summon effect now and don't make this check for
        // summon types similar
        // SPELL_EFFECT_SUMMON_CRITTER, SPELL_EFFECT_SUMMON_WILD or
        // SPELL_EFFECT_SUMMON_GUARDIAN.
        // These won't show up in m_caster->GetPetGUID()
        case SPELL_EFFECT_SUMMON:
        {
            // Halaa Bomb
            if (m_spellInfo->Id == 31958 &&
                m_caster->GetTypeId() == TYPEID_PLAYER)
            {
                uint32 src = ((Player*)m_caster)->m_taxi.GetTaxiSource();
                if (m_caster->GetMapId() != 530 ||
                    !((Player*)m_caster)->IsTaxiFlying() ||
                    (src != 103 && src != 105 && src != 107 && src != 109))
                    return SPELL_FAILED_NOT_HERE;
            }

            // Oscillating Frequency Scanner
            if (m_spellInfo->Id == 37390)
            {
                if (m_caster->has_aura(37407))
                    return SPELL_FAILED_NOT_HERE;
                if (m_caster->GetTypeId() == TYPEID_PLAYER &&
                    static_cast<Player*>(m_caster)->isMoving())
                    return SPELL_FAILED_MOVING;
            }

            if (SummonPropertiesEntry const* summon_prop =
                    sSummonPropertiesStore.LookupEntry(
                        m_spellInfo->EffectMiscValueB[i]))
            {
                if (summon_prop->Group == SUMMON_PROP_GROUP_PETS)
                {
                    if (m_caster->GetPetGuid() &&
                        m_caster->getClass() != CLASS_MAGE) // let mage do
                                                            // replacement
                                                            // summons as well
                        return SPELL_FAILED_ALREADY_HAVE_SUMMON;

                    if (m_caster->GetCharmGuid())
                        return SPELL_FAILED_ALREADY_HAVE_CHARM;
                }
            }
            break;
        }
        case SPELL_EFFECT_SUMMON_PET:
        {
            if (Pet* pet =
                    m_caster->GetPet()) // let warlock do a replacement summon
            {
                if (m_caster->GetTypeId() == TYPEID_PLAYER &&
                    m_caster->getClass() == CLASS_WARLOCK)
                {
                    if (strict) // Summoning Disorientation, trigger pet stun
                                // (cast by pet so it doesn't attack player)
                        pet->CastSpell(pet, 32752, true, nullptr, nullptr,
                            pet->GetObjectGuid());
                }
                else
                    return SPELL_FAILED_ALREADY_HAVE_SUMMON;
            }

            if (m_caster->GetCharmGuid())
                return SPELL_FAILED_ALREADY_HAVE_CHARM;

            break;
        }
        case SPELL_EFFECT_SUMMON_PLAYER:
        {
            if (m_caster->GetTypeId() != TYPEID_PLAYER)
                return SPELL_FAILED_BAD_TARGETS;
            if (!((Player*)m_caster)->GetSelectionGuid())
                return SPELL_FAILED_BAD_TARGETS;

            Player* target = sObjectMgr::Instance()->GetPlayer(
                ((Player*)m_caster)->GetSelectionGuid());
            if (!target || !target->IsInSameRaidWith((Player*)m_caster))
                return SPELL_FAILED_BAD_TARGETS;

            // Evil Twin: prevents you from being summoned
            if (target->has_aura(23445))
                return SPELL_FAILED_BAD_TARGETS;

            // check if our map is dungeon
            if (sMapStore.LookupEntry(m_caster->GetMapId())->IsDungeon())
            {
                InstanceTemplate const* instance =
                    ObjectMgr::GetInstanceTemplate(m_caster->GetMapId());
                if (!instance)
                    return SPELL_FAILED_TARGET_NOT_IN_INSTANCE;
                if (instance->levelMin > target->getLevel())
                    return SPELL_FAILED_LOWLEVEL;
                if (instance->levelMax &&
                    instance->levelMax < target->getLevel())
                    return SPELL_FAILED_HIGHLEVEL;
            }
            break;
        }
        case SPELL_EFFECT_TELEPORT_UNITS:
            if (m_caster->GetTransport())
                return SPELL_FAILED_NOT_ON_TRANSPORT;
            break;
        case SPELL_EFFECT_LEAP:
        case SPELL_EFFECT_TELEPORT_UNITS_FACE_CASTER:
        {
            if (m_caster->GetTransport())
                return SPELL_FAILED_NOT_ON_TRANSPORT;

            // Disable the use of leap effects until the battleground/arena has
            // begun
            if (m_caster->GetTypeId() == TYPEID_PLAYER)
                if (const BattleGround* bg =
                        static_cast<Player*>(m_caster)->GetBattleGround())
                    if (bg->GetStatus() != STATUS_IN_PROGRESS)
                        return SPELL_FAILED_CASTER_AURASTATE;

            // Mage's Blink will ALWAYS fire no matter if we can move forward or
            // not
            if (m_spellInfo->Id == 1953)
                break;

            float dis = GetSpellRadius(sSpellRadiusStore.LookupEntry(
                m_spellInfo->EffectRadiusIndex[i]));
            float fx = m_caster->GetX() + dis * cos(m_caster->GetO());
            float fy = m_caster->GetY() + dis * sin(m_caster->GetO());
            // teleport a bit above terrain level to avoid falling below it
            float fz = m_caster->GetMap()->GetHeight(fx, fy, m_caster->GetZ());
            if (fz <= INVALID_HEIGHT) // note: this also will prevent use effect
                                      // in instances without vmaps height
                                      // enabled
                return SPELL_FAILED_TRY_AGAIN;

            float caster_pos_z = m_caster->GetZ();
            // Control the caster to not climb or drop when +-fz > 8
            if (!(fz <= caster_pos_z + 8 && fz >= caster_pos_z - 8))
                return SPELL_FAILED_TRY_AGAIN;
            break;
        }
        case SPELL_EFFECT_STEAL_BENEFICIAL_BUFF:
        {
            if (m_targets.getUnitTarget() == m_caster)
                return SPELL_FAILED_BAD_TARGETS;
            break;
        }
        case SPELL_EFFECT_CHARGE2:
        {
            logging.error(
                "Spell.cpp: Unsupported effect SPELL_EFFECT_CHARGE2 used by "
                "spell %u",
                m_spellInfo->Id);
            return SPELL_FAILED_DONT_REPORT;
        }
        case SPELL_EFFECT_TRIGGER_SPELL_2:
        {
            // Only Ritual of Summoning has this spell effect (assert in case
            // later expansions change this)
            assert(m_spellInfo->Id == 698);

            // Don't allow summoning if on a transport
            if (m_caster->GetTransport())
                return SPELL_FAILED_NOT_ON_TRANSPORT;

            // Disallow casting Ritual of Summoning in Netherstorm or its
            // associated instances
            switch (m_caster->GetZoneId())
            {
            case 3523: // Netherstorm
            case 3845: // Tempest Keep
            case 3847: // Botanica
            case 3848: // Arcatraz
            case 3849: // Mechanar
                return SPELL_FAILED_NOT_HERE;
            }
            break;
        }
        case SPELL_EFFECT_PICKPOCKET:
        {
            auto target = m_targets.getUnitTarget();
            if (!target || !target->isAlive() ||
                (target->GetCreatureTypeMask() &
                    CREATURE_TYPEMASK_HUMANOID_OR_UNDEAD) == 0)
                return SPELL_FAILED_BAD_TARGETS;
            break;
        }
        case SPELL_EFFECT_RESURRECT_NEW:
        {
            if (!m_targets.getCorpseTargetGuid().IsEmpty())
                break;
            auto target = m_targets.getUnitTarget();
            if (!target || target->isAlive())
                return SPELL_FAILED_INTERRUPTED;
            break;
        }
        default:
            break;
        }
    }

    return SPELL_CAST_OK;
}

SpellCastResult Spell::CheckAuras()
{
    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        switch (m_spellInfo->EffectApplyAuraName[i])
        {
        case SPELL_AURA_MOD_POSSESS:
        {
            if (m_caster->GetTypeId() != TYPEID_PLAYER)
                return SPELL_FAILED_UNKNOWN;

            if (m_targets.getUnitTarget() == m_caster)
                return SPELL_FAILED_BAD_TARGETS;

            if (m_caster->GetCharmGuid())
                return SPELL_FAILED_ALREADY_HAVE_CHARM;

            if (m_caster->GetCharmerGuid())
                return SPELL_FAILED_CHARMED;

            auto target = m_targets.getUnitTarget();
            if (!target)
                return SPELL_FAILED_BAD_IMPLICIT_TARGETS;

            if (target->GetCharmerGuid())
                return SPELL_FAILED_CHARMED;

            if (int32(target->getLevel()) >
                CalculateDamage(SpellEffectIndex(i), target))
                return SPELL_FAILED_HIGHLEVEL;

            if (target->HasAuraType(SPELL_AURA_MOD_FLIGHT_SPEED_MOUNTED) ||
                target->HasAuraType(SPELL_AURA_FLY))
                return SPELL_FAILED_CANT_BE_CHARMED;

            break;
        }
        case SPELL_AURA_MOD_CHARM:
        {
            // FIXME: This is a hack to pass script targets. This should
            //        be made general for all checks using unit target in here
            if (m_targets.getUnitTarget() == m_caster &&
                m_spellInfo->EffectImplicitTargetA[i] != TARGET_SCRIPT)
                return SPELL_FAILED_BAD_TARGETS;

            if (m_caster->GetCharmGuid())
                return SPELL_FAILED_ALREADY_HAVE_CHARM;

            if (m_caster->GetCharmerGuid())
                return SPELL_FAILED_CHARMED;

            auto target = m_targets.getUnitTarget();
            if (!target)
                return SPELL_FAILED_BAD_IMPLICIT_TARGETS;

            if (target->GetCharmerGuid())
                return SPELL_FAILED_CHARMED;

            if (int32(target->getLevel()) >
                CalculateDamage(SpellEffectIndex(i), target))
                return SPELL_FAILED_HIGHLEVEL;

            if (target->HasAuraType(SPELL_AURA_MOD_FLIGHT_SPEED_MOUNTED) ||
                target->HasAuraType(SPELL_AURA_FLY))
                return SPELL_FAILED_CANT_BE_CHARMED;

            // Players cannot enslave/charm other player's pets
            Unit* owner;
            if (m_caster->GetTypeId() == TYPEID_PLAYER &&
                (owner = target->GetCharmerOrOwner()) != nullptr &&
                owner->GetTypeId() == TYPEID_PLAYER)
                return SPELL_FAILED_BAD_TARGETS;

            break;
        }
        case SPELL_AURA_MOD_STEALTH:
        {
            const auto& dispels = m_caster->m_spellImmune[IMMUNITY_DISPEL];
            for (const auto& elem : dispels)
                if (elem.type == DISPEL_STEALTH)
                    return SPELL_FAILED_CASTER_AURASTATE;
            break;
        }
        case SPELL_AURA_MOD_INVISIBILITY:
        {
            const auto& dispels = m_caster->m_spellImmune[IMMUNITY_DISPEL];
            for (const auto& elem : dispels)
                if (elem.type == DISPEL_STEALTH)
                    return SPELL_FAILED_CASTER_AURASTATE;
            break;
        }
        case SPELL_AURA_MOD_POSSESS_PET:
        {
            if (m_caster->GetTypeId() != TYPEID_PLAYER)
                return SPELL_FAILED_UNKNOWN;

            if (m_caster->GetCharmGuid())
                return SPELL_FAILED_ALREADY_HAVE_CHARM;

            if (m_caster->GetCharmerGuid())
                return SPELL_FAILED_CHARMED;

            Pet* pet = m_caster->GetPet();
            if (!pet)
                return SPELL_FAILED_NO_PET;

            if (pet->GetCharmerGuid())
                return SPELL_FAILED_CHARMED;

            break;
        }
        case SPELL_AURA_MOUNTED:
        {
            if (m_caster->IsInWater())
                return SPELL_FAILED_ONLY_ABOVEWATER;

            if (m_caster->GetTypeId() == TYPEID_PLAYER &&
                ((Player*)m_caster)->GetTransport())
                return SPELL_FAILED_NO_MOUNTS_ALLOWED;

            // Ignore map check if spell have AreaId. AreaId already checked and
            // this prevent special mount spells
            if (m_caster->GetTypeId() == TYPEID_PLAYER &&
                !sMapStore.LookupEntry(m_caster->GetMapId())
                     ->IsMountAllowed() &&
                !trigger_type_.triggered() && !m_spellInfo->AreaId)
                return SPELL_FAILED_NO_MOUNTS_ALLOWED;

            if (m_caster->GetAreaId() == 35)
                return SPELL_FAILED_NO_MOUNTS_ALLOWED;

            // Cancel stealth auras when attempting to mount
            m_caster->remove_auras(SPELL_AURA_MOD_STEALTH);

            if (m_caster->IsInDisallowedMountForm())
                return SPELL_FAILED_NOT_SHAPESHIFT;

            break;
        }
        case SPELL_AURA_RANGED_ATTACK_POWER_ATTACKER_BONUS:
        {
            if (!m_targets.getUnitTarget())
                return SPELL_FAILED_BAD_IMPLICIT_TARGETS;

            // can be casted at non-friendly unit or own pet/charm
            if (m_caster->IsFriendlyTo(m_targets.getUnitTarget()))
                return SPELL_FAILED_TARGET_FRIENDLY;

            break;
        }
        case SPELL_AURA_PERIODIC_MANA_LEECH:
        {
            if (!m_targets.getUnitTarget())
                return SPELL_FAILED_BAD_IMPLICIT_TARGETS;

            if (m_caster->GetTypeId() != TYPEID_PLAYER || m_CastItem)
                break;

            if (m_targets.getUnitTarget()->getPowerType() != POWER_MANA ||
                m_targets.getUnitTarget()->GetMaxPower(POWER_MANA) == 0)
                return SPELL_FAILED_BAD_TARGETS;

            break;
        }
        case SPELL_AURA_MIRROR_IMAGE:
        {
            Unit* pTarget = m_targets.getUnitTarget();

            // In case of TARGET_SCRIPT, we have already added a target. Use it
            // here (and find a better solution)
            if (m_UniqueTargetInfo.size() == 1)
                pTarget = m_caster->GetMap()->GetAnyTypeCreature(
                    m_UniqueTargetInfo.front().targetGUID);

            if (!pTarget)
                return SPELL_FAILED_BAD_TARGETS;

            if (pTarget->GetTypeId() != TYPEID_UNIT) // Target must be creature.
                                                     // TODO: Check if target
                                                     // can also be player
                return SPELL_FAILED_BAD_TARGETS;

            if (pTarget == m_caster) // Clone self can't be accepted
                return SPELL_FAILED_BAD_TARGETS;

            // It is assumed that target can not be cloned if already cloned by
            // same or other clone auras
            if (pTarget->HasAuraType(SPELL_AURA_MIRROR_IMAGE))
                return SPELL_FAILED_BAD_TARGETS;

            break;
        }
        case SPELL_AURA_MOD_SHAPESHIFT:
        {
            if (auto target = m_targets.getUnitTarget())
            {
                auto& class_scripts =
                    target->GetAurasByType(SPELL_AURA_OVERRIDE_CLASS_SCRIPTS);
                for (auto& script : class_scripts)
                {
                    auto misc = script->GetSpellProto()
                                    ->EffectMiscValue[script->GetEffIndex()];
                    if (misc == 3655 || misc == 3654)
                        return SPELL_FAILED_CASTER_AURASTATE;
                }
            }
            break;
        }
        default:
            break;
        }
    }

    return SPELL_CAST_OK;
}

SpellCastResult Spell::CheckTrade()
{
    if (m_targets.m_targetMask & TARGET_FLAG_TRADE_ITEM)
    {
        if (m_caster->GetTypeId() != TYPEID_PLAYER)
            return SPELL_FAILED_NOT_TRADING;

        Player* player = static_cast<Player*>(m_caster);

        if (!player->trade())
            return SPELL_FAILED_NOT_TRADING;

        if (!player->trade()->spell_target(player))
            return SPELL_FAILED_ITEM_GONE;

        // If the trade isn't finished we simply save the spell to be cast
        // in the trade, and the trade will cast it again later
        if (!player->trade()->finished())
        {
            player->trade()->set_spell(player,
                m_CastItem ? m_CastItem->GetObjectGuid() : ObjectGuid(),
                m_spellInfo->Id);
            return SPELL_FAILED_DONT_REPORT;
        }
    }

    return SPELL_CAST_OK;
}

uint32 Spell::CalculatePowerCost(SpellEntry const* spellInfo, Unit* caster,
    Spell const* spell, Item* castItem)
{
    // item cast not used power
    if (castItem)
        return 0;

    // Spell drain all exist power on cast (Only paladin lay of Hands)
    if (spellInfo->HasAttribute(SPELL_ATTR_EX_DRAIN_ALL_POWER))
    {
        // If power type - health drain all
        if (spellInfo->powerType == POWER_HEALTH)
            return caster->GetHealth();
        // Else drain all power
        if (spellInfo->powerType < MAX_POWERS)
            return caster->GetPower(Powers(spellInfo->powerType));
        logging.error(
            "Spell::CalculateManaCost: Unknown power type '%d' in spell %d",
            spellInfo->powerType, spellInfo->Id);
        return 0;
    }

    // Base powerCost
    int32 powerCost = spellInfo->manaCost;
    // PCT cost from total amount
    if (spellInfo->ManaCostPercentage)
    {
        switch (spellInfo->powerType)
        {
        // health as power used
        case POWER_HEALTH:
            powerCost +=
                spellInfo->ManaCostPercentage * caster->GetCreateHealth() / 100;
            break;
        case POWER_MANA:
            powerCost +=
                spellInfo->ManaCostPercentage * caster->GetCreateMana() / 100;
            break;
        case POWER_RAGE:
        case POWER_FOCUS:
        case POWER_ENERGY:
        case POWER_HAPPINESS:
            powerCost += spellInfo->ManaCostPercentage *
                         caster->GetMaxPower(Powers(spellInfo->powerType)) /
                         100;
            break;
        default:
            logging.error(
                "Spell::CalculateManaCost: Unknown power type '%d' in spell %d",
                spellInfo->powerType, spellInfo->Id);
            return 0;
        }
    }
    SpellSchools school = GetFirstSchoolInMask(
        spell ? spell->m_spellSchoolMask : GetSpellSchoolMask(spellInfo));
    // Flat mod from caster auras by spell school
    powerCost += caster->GetInt32Value(UNIT_FIELD_POWER_COST_MODIFIER + school);
    // Shiv - costs 20 + weaponSpeed*10 energy (apply only to non-triggered
    // spell with energy cost)
    if (spellInfo->HasAttribute(SPELL_ATTR_EX4_SPELL_VS_EXTEND_COST))
        powerCost += caster->GetAttackTime(OFF_ATTACK) / 100;
    // Apply cost mod by spell
    if (spell)
        if (Player* modOwner = caster->GetSpellModOwner())
            modOwner->ApplySpellMod(
                spellInfo->Id, SPELLMOD_COST, powerCost, spell);

    if (spellInfo->HasAttribute(SPELL_ATTR_LEVEL_DAMAGE_CALCULATION) &&
        spellInfo->baseLevel > 0)
    {
        auto caster_level = caster->getLevel();
        if (spellInfo->maxLevel && caster_level > spellInfo->maxLevel)
            caster_level = spellInfo->maxLevel;
        powerCost *=
            sSpellMgr::Instance()->spell_level_calc_mana(caster_level) /
            sSpellMgr::Instance()->spell_level_calc_mana(spellInfo->baseLevel);
    }

    // PCT mod from user auras by school
    powerCost = int32(
        powerCost *
        (1.0f +
            caster->GetFloatValue(UNIT_FIELD_POWER_COST_MULTIPLIER + school)));
    if (powerCost < 0)
        powerCost = 0;
    return powerCost;
}

bool Spell::IgnoreItemRequirements() const
{
    if (trigger_type_.triggered())
    {
        /// Some triggered spells have same reagents that have master spell
        /// expected in test: master spell have reagents in first slot then
        /// triggered don't must use own
        if (m_triggeredBySpellInfo && !m_triggeredBySpellInfo->Reagent[0])
            return false;

        return true;
    }

    return false;
}

void Spell::Delayed()
{
    if (!m_caster || m_caster->GetTypeId() != TYPEID_PLAYER)
        return;

    if (m_spellState == SPELL_STATE_DELAYED)
        return; // spell is active and can't be time-backed

    // spells not loosing casting time ( slam, dynamites, bombs.. )
    if (!(m_spellInfo->InterruptFlags & SPELL_INTERRUPT_FLAG_DAMAGE))
        return;

    // check resist chance
    int32 resistChance =
        100; // must be initialized to 100 for percent modifiers
    ((Player*)m_caster)
        ->ApplySpellMod(m_spellInfo->Id, SPELLMOD_NOT_LOSE_CASTING_TIME,
            resistChance, this);
    resistChance +=
        m_caster->GetTotalAuraModifier(SPELL_AURA_RESIST_PUSHBACK) - 100;
    if (roll_chance_i(resistChance))
        return;

    int32 delaytime = GetNextDelayAtDamageMsTime();

    if (int32(m_timer) + delaytime > m_casttime)
    {
        delaytime = m_casttime - m_timer;
        m_timer = m_casttime;
    }
    else
        m_timer += delaytime;

    LOG_DEBUG(logging, "Spell %u partially interrupted for (%d) ms at damage",
        m_spellInfo->Id, delaytime);

    WorldPacket data(SMSG_SPELL_DELAYED, 8 + 4);
    data << m_caster->GetPackGUID();
    data << uint32(delaytime);

    SendMessageToSet(std::move(data));
}

void Spell::DelayedChannel()
{
    if (!m_caster || m_caster->GetTypeId() != TYPEID_PLAYER ||
        getState() != SPELL_STATE_CASTING)
        return;

    // check resist chance
    int32 resistChance = 100; // must be initialized to 100 for percent
                              // modifiers
    ((Player*)m_caster)
        ->ApplySpellMod(m_spellInfo->Id, SPELLMOD_NOT_LOSE_CASTING_TIME,
            resistChance, this);
    resistChance +=
        m_caster->GetTotalAuraModifier(SPELL_AURA_RESIST_PUSHBACK) - 100;
    if (roll_chance_i(resistChance))
        return;

    int32 delaytime = GetNextDelayAtDamageMsTime();

    if (int32(m_timer) < delaytime)
    {
        delaytime = m_timer;
        m_timer = 0;
    }
    else
        m_timer -= delaytime;

    LOG_DEBUG(logging,
        "Spell %u partially interrupted for %i ms, new duration: %u ms",
        m_spellInfo->Id, delaytime, m_timer);

    for (TargetList::const_iterator ihit = m_UniqueTargetInfo.begin();
         ihit != m_UniqueTargetInfo.end(); ++ihit)
    {
        if ((*ihit).missCondition == SPELL_MISS_NONE)
        {
            if (Unit* unit =
                    m_caster->GetObjectGuid() == ihit->targetGUID ?
                        m_caster :
                        ObjectAccessor::GetUnit(*m_caster, ihit->targetGUID))
                unit->DelaySpellAuraHolder(
                    m_spellInfo->Id, delaytime, m_caster->GetObjectGuid());
        }
    }

    for (int j = 0; j < MAX_EFFECT_INDEX; ++j)
    {
        // partially interrupt persistent area auras
        if (DynamicObject* dynObj =
                m_caster->GetDynObject(m_spellInfo->Id, SpellEffectIndex(j)))
            dynObj->Delay(delaytime);
    }

    SendChannelUpdate(m_timer);
}

void Spell::UpdateOriginalCasterPointer()
{
    if (m_originalCasterGUID == m_caster->GetObjectGuid())
        m_originalCaster = m_caster;
    else if (m_originalCasterGUID.IsGameObject())
    {
        GameObject* go =
            m_caster->IsInWorld() ?
                m_caster->GetMap()->GetGameObject(m_originalCasterGUID) :
                nullptr;
        m_originalCaster = go ? go->GetOwner() : nullptr;
    }
    else
    {
        Unit* unit = ObjectAccessor::GetUnit(*m_caster, m_originalCasterGUID);
        m_originalCaster = unit && unit->IsInWorld() ? unit : nullptr;
    }
}

void Spell::UpdatePointers()
{
    UpdateOriginalCasterPointer();

    m_targets.Update(m_caster);
}

bool Spell::CheckTargetCreatureType(Unit* target) const
{
    uint32 spellCreatureTargetMask = m_spellInfo->TargetCreatureType;

    // Curse of Doom: not find another way to fix spell target check :/
    if (m_spellInfo->IsFitToFamily(
            SPELLFAMILY_WARLOCK, UI64LIT(0x0000000200000000)))
    {
        // not allow cast at player
        if (target->GetTypeId() == TYPEID_PLAYER)
            return false;

        spellCreatureTargetMask = 0x7FF;
    }

    // Dismiss Pet and Taming Lesson skipped
    if (m_spellInfo->Id == 2641 || m_spellInfo->Id == 23356)
        spellCreatureTargetMask = 0;

    // Skip Grounding Totem mask check
    // This fixes spells like Polymorph, Repentance and such which normally only
    // are castable towards living creatures
    // but the totem is not considered as such.
    if (target->GetEntry() == 5925)
        spellCreatureTargetMask = 0;

    if (spellCreatureTargetMask)
    {
        uint32 TargetCreatureType = target->GetCreatureTypeMask();

        return !TargetCreatureType ||
               (spellCreatureTargetMask & TargetCreatureType);
    }
    return true;
}

CurrentSpellTypes Spell::GetCurrentContainer()
{
    if (IsNextMeleeSwingSpell())
        return (CURRENT_MELEE_SPELL);
    else if (IsAutoRepeat())
        return (CURRENT_AUTOREPEAT_SPELL);
    else if (IsChanneledSpell(m_spellInfo))
        return (CURRENT_CHANNELED_SPELL);
    else
        return (CURRENT_GENERIC_SPELL);
}

bool Spell::CheckTarget(Unit* target, SpellEffectIndex eff)
{
    // Check targets for creature type mask and remove not appropriate (skip
    // explicit self target case, maybe need other explicit targets)
    if (m_spellInfo->EffectImplicitTargetA[eff] != TARGET_SELF)
    {
        if (!CheckTargetCreatureType(target))
            return false;
    }

    // Check targets for not_selectable unit flag and remove
    // A player can cast spells on his pet (or other controlled unit) though in
    // any state
    if (target != m_caster &&
        target->GetCharmerOrOwnerGuid() != m_caster->GetObjectGuid())
    {
        // any unattackable target skipped
        if (!m_scriptTarget &&
            target->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE))
            return false;

        // visibility off targets can never be hit
        if (target->GetVisibility() == VISIBILITY_OFF)
            return false;

        if (!m_scriptTarget &&
            target->HasFlag(
                UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_PLAYER_ATTACKABLE) &&
            m_caster->player_controlled())
            return false;

        // Database target selection conditions
        auto conditions =
            sConditionMgr::Instance()->GetSpellTargetSelectionConditions(
                m_spellInfo->Id);
        if (conditions)
        {
            auto condition_info = ConditionSourceInfo(m_caster, target);
            if (!sConditionMgr::Instance()->IsObjectMeetToConditions(
                    condition_info, conditions))
            {
                return false;
            }
        }

        // unselectable targets skipped in all cases except TARGET_SCRIPT
        // targeting
        // in case TARGET_SCRIPT target selected by server always and can't be
        // cheated
        // Non-player controlled units should also be able to ignore this
        // selectability
        if ((!trigger_type_.triggered() ||
                target != m_targets.getUnitTarget()) &&
            target->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE) &&
            m_spellInfo->EffectImplicitTargetA[eff] != TARGET_SCRIPT &&
            m_spellInfo->EffectImplicitTargetB[eff] != TARGET_SCRIPT &&
            m_spellInfo->EffectImplicitTargetA[eff] !=
                TARGET_AREAEFFECT_INSTANT &&
            m_spellInfo->EffectImplicitTargetB[eff] !=
                TARGET_AREAEFFECT_INSTANT &&
            m_spellInfo->EffectImplicitTargetA[eff] !=
                TARGET_AREAEFFECT_CUSTOM &&
            m_spellInfo->EffectImplicitTargetB[eff] !=
                TARGET_AREAEFFECT_CUSTOM &&
            (m_caster->GetTypeId() != TYPEID_UNIT ||
                m_caster->IsControlledByPlayer()))
            return false;
    }

    // Check player targets and remove if in GM mode or GM invisibility (for not
    // self casting case)
    if (target != m_caster && target->GetTypeId() == TYPEID_PLAYER)
    {
        if (((Player*)target)->isGameMaster() &&
            !IsPositiveSpell(m_spellInfo->Id))
            return false;
    }

    // No spells can ever be cast on a target off the opposing team in a
    // sanctuary
    if (Player* target_player = target->GetCharmerOrOwnerPlayerOrPlayerItself())
    {
        if (target_player->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_SANCTUARY))
        {
            if (Player* caster_player =
                    m_caster->GetCharmerOrOwnerPlayerOrPlayerItself())
                if (caster_player->GetTeam() != target_player->GetTeam())
                    return false;
        }
    }

    // Check that target is not too low level
    if (m_spellInfo !=
        sSpellMgr::Instance()->SelectAuraRankForLevel(
            m_spellInfo, target->getLevel()))
        return false;

    // Check targets for LOS visibility (except spells without range limitations
    // )
    switch (m_spellInfo->Effect[eff])
    {
    case SPELL_EFFECT_SUMMON_PLAYER: // from anywhere
        break;
    case SPELL_EFFECT_DUMMY:
        if (m_spellInfo->Id != 20577) // Cannibalize
            break;
    // fall through
    case SPELL_EFFECT_RESURRECT_NEW:
        // player far away, maybe his corpse near?
        if (target != m_caster && !target->IsWithinWmoLOSInMap(m_caster))
        {
            if (!m_targets.getCorpseTargetGuid())
                return false;

            Corpse* corpse =
                m_caster->GetMap()->GetCorpse(m_targets.getCorpseTargetGuid());
            if (!corpse)
                return false;

            if (target->GetObjectGuid() != corpse->GetOwnerGuid())
                return false;

            if (!corpse->IsWithinWmoLOSInMap(m_caster))
                return false;
        }

        // all ok by some way or another, skip normal check
        break;
    default: // normal case
        // Get GO cast coordinates if original caster -> GO
        if (target != m_caster)
            if (WorldObject* caster = GetCastingObject())
            {
                if (target->IsInMap(caster) &&
                    (m_spellInfo->HasAttribute(
                         SPELL_ATTR_CUSTOM1_IGNORES_LOS) ||
                        m_spellInfo->HasTargetType(TARGET_AREAEFFECT_PARTY) ||
                        m_spellInfo->HasTargetType(
                            TARGET_AREAEFFECT_PARTY_AND_CLASS)))
                    return true;
                if (!trigger_type_.triggered() ||
                    m_spellInfo->HasAttribute(SPELL_ATTR_CUSTOM1_AOE_FORCE_LOS))
                {
                    // If spell is detination targeted, check destination to
                    // target,
                    // otherwise check caster to target for LoS
                    if (m_targets.m_targetMask & TARGET_FLAG_DEST_LOCATION)
                    {
                        if (!target->IsInMap(caster))
                            return false;

                        if (!m_spellInfo->HasAttribute(
                                SPELL_ATTR_CUSTOM1_IGNORES_LOS) &&
                            !target->IsWithinWmoLOS(m_targets.m_destX,
                                m_targets.m_destY, m_targets.m_destZ + 2.0f))
                            return false;
                    }
                    // Chain damage does not require caster LoS
                    else if (m_spellInfo->EffectImplicitTargetA[eff] !=
                             TARGET_CHAIN_DAMAGE)
                    {
                        if (!m_spellInfo->HasAttribute(
                                SPELL_ATTR_CUSTOM1_IGNORES_LOS) &&
                            !target->IsWithinWmoLOSInMap(caster))
                            return false;
                    }
                }
            }
        break;
    }

    switch (m_spellInfo->Id)
    {
    case 37433: // Spout (The Lurker Below), only players affected if its not in
                // water
        if (target->GetTypeId() != TYPEID_PLAYER || target->IsInWater())
            return false;
    default:
        break;
    }

    if (target->GetShapeshiftForm() == FORM_SPIRITOFREDEMPTION)
    {
        auto id = m_spellInfo->Id;
        if (id != 20711 && id != 27792 && id != 27795 && id != 27827 &&
            id != 32343)
            return false;

        return true;
    }

    return true;
}

bool Spell::IsNeedSendToClient() const
{
    return m_spellInfo->SpellVisual != 0 || IsChanneledSpell(m_spellInfo) ||
           m_spellInfo->speed > 0.0f || trigger_type_.combat_log() ||
           (!m_triggeredByAuraSpell && !trigger_type_.triggered());
}

bool Spell::IsTriggeredSpellWithRedundentData() const
{
    return m_triggeredByAuraSpell || m_triggeredBySpellInfo ||
           // possible not need after above check?
           (trigger_type_.triggered() &&
               (m_spellInfo->manaCost || m_spellInfo->ManaCostPercentage));
}

bool Spell::HaveTargetsForEffect(SpellEffectIndex effect) const
{
    for (const auto& elem : m_UniqueTargetInfo)
        if (elem.effectMask & (1 << effect))
            return true;

    for (const auto& elem : m_UniqueGOTargetInfo)
        if (elem.effectMask & (1 << effect))
            return true;

    for (const auto& elem : m_UniqueItemInfo)
        if (elem.effectMask & (1 << effect))
            return true;

    return false;
}

SpellEvent::SpellEvent(Spell* spell) : BasicEvent()
{
    m_Spell = spell;
}

SpellEvent::~SpellEvent()
{
    if (m_Spell->getState() != SPELL_STATE_FINISHED)
        m_Spell->cancel();

    if (m_Spell->IsDeletable())
    {
        delete m_Spell;
    }
    else
    {
        // A spell can be non-deletable if it's still referenced in targets'
        // spell queues
        // when the target logs off (for example)
        // In that case we push it onto the backburner for deletion at some
        // point in the future
        sWorld::Instance()->backburn_spell(m_Spell);
    }
}

bool SpellEvent::Execute(uint64 e_time, uint32 p_time)
{
    // update spell if it is not finished
    if (m_Spell->getState() != SPELL_STATE_FINISHED)
        m_Spell->update(p_time);

    // check spell state to process
    switch (m_Spell->getState())
    {
    case SPELL_STATE_FINISHED:
    {
        // spell was finished, check deletable state
        if (m_Spell->IsDeletable())
        {
            m_Spell->DoFinishPhase();
            return true; // spell is deletable, finish event
        }
        // event will be re-added automatically at the end of routine)
    }
    break;

    case SPELL_STATE_CASTING:
    {
        // this spell is in channeled state, process it on the next update
        // event will be re-added automatically at the end of routine)
    }
    break;

    case SPELL_STATE_CHARGE_SPECIAL:
    {
        // this state means we're waiting for path-generation, see if it has
        // returned yet
        if (m_Spell->path_gen_finished)
            m_Spell->prepare(&m_Spell->m_targets);
    }
    break;

    case SPELL_STATE_DELAYED:
    {
        // first, check, if we have just started
        if (m_Spell->GetDelayStart() != 0)
        {
            // no, we aren't, do the typical update
            // check, if we have channeled spell on our hands
            if (IsChanneledSpell(m_Spell->m_spellInfo))
            {
                // evented channeled spell is processed separately, casted once
                // after delay, and not destroyed till finish
                // check, if we have casting anything else except this channeled
                // spell and autorepeat
                if (m_Spell->GetCaster()->IsNonMeleeSpellCasted(
                        false, true, true))
                {
                    // another non-melee non-delayed spell is casted now, abort
                    m_Spell->cancel();
                }
                else
                {
                    // do the action (pass spell to channeling state)
                    m_Spell->handle_immediate();
                }
                // event will be re-added automatically at the end of routine)
            }
            else
            {
                // run the spell handler and think about what we can do next
                uint64 t_offset = e_time - m_Spell->GetDelayStart();
                uint64 n_offset = m_Spell->handle_delayed(t_offset);
                if (n_offset)
                {
                    // re-add us to the queue
                    m_Spell->GetCaster()->m_Events.AddEvent(
                        this, m_Spell->GetDelayStart() + n_offset, false);
                    return false; // event not complete
                }
                // event complete
                // finish update event will be re-added automatically at the end
                // of routine)
            }
        }
        else
        {
            // delaying had just started, record the moment
            m_Spell->SetDelayStart(e_time);
            // re-plan the event for the delay moment
            if (IsChanneledSpell(m_Spell->m_spellInfo))
            {
                m_Spell->GetCaster()->m_Events.AddEvent(
                    this, e_time + m_Spell->GetDelayMoment(), false);
            }
            else if (m_Spell->GetDelayMoment())
            {
                m_Spell->GetCaster()->m_Events.AddEvent(
                    this, e_time + m_Spell->GetDelayMoment(), false);
            }
            else if (m_Spell->m_targets.m_destX != 0 ||
                     m_Spell->m_targets.m_destY != 0 ||
                     m_Spell->m_targets.m_destZ != 0)
            {
                // Spells without a unit target or GO target, but a x, y, z
                // target,
                // will have no delay time set at this point, so we need to set
                // it.
                float dist = m_Spell->GetCaster()->GetDistance(
                    m_Spell->m_targets.m_destX, m_Spell->m_targets.m_destY,
                    m_Spell->m_targets.m_destZ);
                m_Spell->SetDelayMoment((uint64)floor(
                    dist / m_Spell->m_spellInfo->speed * 1000.0f));
                m_Spell->GetCaster()->m_Events.AddEvent(
                    this, e_time + m_Spell->GetDelayMoment(), false);
            }
            return false; // event not complete
        }
    }
    break;

    default:
    {
        // all other states
        // event will be re-added automatically at the end of routine)
    }
    break;
    }

    // spell processing not complete, plan event on the next update interval
    m_Spell->GetCaster()->m_Events.AddEvent(this, e_time + 1, false);
    return false; // event not complete
}

void SpellEvent::Abort(uint64 /*e_time*/)
{
    // oops, the spell we try to do is aborted
    if (m_Spell->getState() != SPELL_STATE_FINISHED)
        m_Spell->cancel();
}

bool SpellEvent::IsDeletable() const
{
    return m_Spell->IsDeletable();
}

SpellCastResult Spell::CanOpenLock(SpellEffectIndex effIndex, uint32 lockId,
    SkillType& skillId, int32& reqSkillValue, int32& skillValue)
{
    if (!lockId) // possible case for GO and maybe for items.
        return SPELL_CAST_OK;

    // Get LockInfo
    LockEntry const* lockInfo = sLockStore.LookupEntry(lockId);

    if (!lockInfo)
        return SPELL_FAILED_BAD_TARGETS;

    bool reqKey = false; // some locks not have reqs

    for (int j = 0; j < 8; ++j)
    {
        switch (lockInfo->Type[j])
        {
        // check key item (many fit cases can be)
        case LOCK_KEY_ITEM:
            if (lockInfo->Index[j] && m_CastItem &&
                m_CastItem->GetEntry() == lockInfo->Index[j])
                return SPELL_CAST_OK;
            reqKey = true;
            break;
        // check key skill (only single first fit case can be)
        case LOCK_KEY_SKILL:
        {
            reqKey = true;

            // wrong locktype, skip
            if (uint32(m_spellInfo->EffectMiscValue[effIndex]) !=
                lockInfo->Index[j])
                continue;

            skillId = SkillByLockType(LockType(lockInfo->Index[j]));

            if (skillId != SKILL_NONE)
            {
                // skill bonus provided by casting spell (mostly item spells)
                // add the damage modifier from the spell casted (cheat lock /
                // skeleton key etc.) (use m_currentBasePoints, CalculateDamage
                // returns wrong value)
                uint32 spellSkillBonus = uint32(m_currentBasePoints[effIndex]);
                reqSkillValue = lockInfo->Skill[j];

                // castitem check: rogue using skeleton keys. the skill values
                // should not be added in this case.
                skillValue =
                    m_CastItem || m_caster->GetTypeId() != TYPEID_PLAYER ?
                        0 :
                        ((Player*)m_caster)->GetSkillValue(skillId);

                skillValue += spellSkillBonus;

                if (skillValue < reqSkillValue)
                    return SPELL_FAILED_LOW_CASTLEVEL;
            }

            return SPELL_CAST_OK;
        }
        }
    }

    if (reqKey)
        return SPELL_FAILED_BAD_TARGETS;

    return SPELL_CAST_OK;
}

void Spell::DropComboPointsIfNeeded(bool finish_phase)
{
    // Deadly throw needs to drop CP earlier than other spells, to prevent
    // double cast due to travel time
    if (finish_phase == (m_spellInfo->Id == 26679))
        return;

    if (m_caster->GetTypeId() == TYPEID_PLAYER && NeedsComboPoints(m_spellInfo))
    {
        // Don't drop CP if we missed one of our targets
        bool needDrop = true;
        if (!IsPositiveSpell(m_spellInfo->Id))
        {
            for (auto& target : m_UniqueTargetInfo)
            {
                if (target.missCondition != SPELL_MISS_NONE &&
                    target.targetGUID != m_caster->GetObjectGuid())
                {
                    needDrop = false;
                    break;
                }
            }
        }
        if (needDrop)
            static_cast<Player*>(m_caster)->ClearComboPoints();
    }
}

bool Spell::DoImmunePowerException(TargetInfo* target)
{
    if (m_spellInfo->HasAttribute(SPELL_ATTR_UNAFFECTED_BY_INVULNERABILITY))
        return false;

    if (m_powerCost == 0 || !m_spellInfo->HasAttribute(SPELL_ATTR_ABILITY) ||
        trigger_type_.triggered() || m_spellInfo->CastingTimeIndex != 1 ||
        m_caster->GetTypeId() != TYPEID_PLAYER ||
        (m_spellInfo->powerType != POWER_RAGE &&
            m_spellInfo->powerType != POWER_ENERGY) ||
        IsAreaOfEffectSpell(m_spellInfo))
        return false;

    if (GetAffectiveCaster() != m_caster)
        return false;

    Unit* u_tar = m_targets.getUnitTarget();
    if (!u_tar || u_tar->GetObjectGuid() != target->targetGUID)
        return false;

    // need full immunity to everything for this mechanic to apply
    uint32 total_mask = 0;
    const SpellImmuneList& school_list = u_tar->m_spellImmune[IMMUNITY_SCHOOL];
    for (auto& e : school_list)
        total_mask |= e.type;
    if (total_mask != 127)
        return false;

    bool full_refund = false;

    auto name = m_spellInfo->SpellFamilyName;
    if (name == SPELLFAMILY_ROGUE)
    {
        // finishing moves not affected
        if (m_spellInfo->HasAttribute(SPELL_ATTR_EX_REQ_TARGET_COMBO_POINTS))
            return false;
    }
    else if (name == SPELLFAMILY_WARRIOR)
    {
        // mortal strike / bloodthirst / shield slam does a FULL refund
        if (m_spellInfo->SpellFamilyFlags &
            (0x2000000 | 0x40000000000 | 0x100000000))
            full_refund = true;
    }
    else if (name == SPELLFAMILY_DRUID)
    {
        // finishing moves not affected
        if (m_spellInfo->HasAttribute(SPELL_ATTR_EX_REQ_TARGET_COMBO_POINTS))
            return false;
    }
    else
    {
        return false;
    }

    if (!full_refund)
    {
        int32 new_cost = m_spellInfo->manaCost * 0.25f;

        // for some odd reason blizzard decided that rage is times 10, energy
        // isn't
        float coeff = m_spellInfo->powerType == POWER_RAGE ? 10.0f : 1.0f;
        if (new_cost > 8 * coeff)
            new_cost = 8 * coeff;

        if (new_cost > (int32)m_powerCost)
            return false;

        if (m_spellInfo->powerType == POWER_RAGE)
            new_cost =
                (new_cost / 10) * 10; // remove decimal point; 25 => 20, etc

        int32 refund = (int32)m_spellInfo->manaCost - new_cost;
        if (refund > 0)
        {
            Powers t = static_cast<Powers>(m_spellInfo->powerType);
            m_caster->SetPower(t, m_caster->GetPower(t) + refund);
        }
    }
    else
    {
        if (m_powerCost > 0)
        {
            Powers t = static_cast<Powers>(m_spellInfo->powerType);
            m_caster->SetPower(t, m_caster->GetPower(t) + m_powerCost);
        }
    }

    m_caster->SendSpellMiss(u_tar, m_spellInfo->Id, SPELL_MISS_IMMUNE);

    return true;
}

struct aoe_targets_worker
{
    Spell::UnitList* i_data;
    Spell& i_spell;
    SpellNotifyPushType i_push_type;
    float i_radius;
    SpellTargets i_TargetType;
    WorldObject* i_originalCaster;
    WorldObject* i_castingObject;
    bool i_playerControlled;
    float i_centerX;
    float i_centerY;

    aoe_targets_worker(Spell& spell, Spell::UnitList& data, float radius,
        SpellNotifyPushType type,
        SpellTargets TargetType = SPELL_TARGETS_NOT_FRIENDLY,
        WorldObject* originalCaster = nullptr)
      : i_data(&data), i_spell(spell), i_push_type(type), i_radius(radius),
        i_TargetType(TargetType), i_originalCaster(originalCaster),
        i_castingObject(i_spell.GetCastingObject()), i_playerControlled(false),
        i_centerX(0), i_centerY(0)
    {
        if (!i_originalCaster)
            i_originalCaster = i_spell.GetAffectiveCasterObject();
        i_playerControlled =
            i_originalCaster ? i_originalCaster->IsControlledByPlayer() : false;

        switch (i_push_type)
        {
        case PUSH_IN_FRONT:
        case PUSH_IN_FRONT_90:
        case PUSH_IN_FRONT_15:
        case PUSH_IN_BACK:
        case PUSH_IN_BACK_90:
        case PUSH_IN_BACK_15:
        case PUSH_SELF_CENTER:
            if (i_castingObject)
            {
                i_centerX = i_castingObject->GetX();
                i_centerY = i_castingObject->GetY();
            }
            break;
        case PUSH_DEST_CENTER:
            i_centerX = i_spell.m_targets.m_destX;
            i_centerY = i_spell.m_targets.m_destY;
            break;
        case PUSH_TARGET_CENTER:
            if (Unit* target = i_spell.m_targets.getUnitTarget())
            {
                i_centerX = target->GetX();
                i_centerY = target->GetY();
            }
            break;
        default:
            logging.error(
                "aoe_targets_worker: unsupported PUSH_* case %u.", i_push_type);
        }
    }

    void operator()(Unit* elem)
    {
        // there are still more spells which can be casted on dead, but
        // they are no AOE and don't have such a nice SPELL_ATTR flag
        if ((i_TargetType != SPELL_TARGETS_ALL &&
                !elem->isTargetableForAttack(i_spell.m_spellInfo->HasAttribute(
                    SPELL_ATTR_EX3_CAST_ON_DEAD)))
            // mostly phase check
            ||
            !elem->IsInMap(i_originalCaster))
            return;

        // Can't hit same-faction targets that are duelling
        if (i_originalCaster->GetTypeId() == TYPEID_PLAYER)
        {
            if (Player* player_target =
                    elem->GetCharmerOrOwnerPlayerOrPlayerItself())
            {
                if (player_target->duel &&
                    player_target->GetTeam() ==
                        static_cast<Player*>(i_originalCaster)->GetTeam() &&
                    !player_target->IsInDuelWith(
                        static_cast<Player*>(i_originalCaster)) &&
                    player_target != i_originalCaster)
                    return;
            }
        }

        // player caster & player owned target hostility checks
        if (i_originalCaster->GetTypeId() == TYPEID_PLAYER ||
            i_originalCaster->GetTypeId() == TYPEID_UNIT)
        {
            if (Player* target_player =
                    elem->GetCharmerOrOwnerPlayerOrPlayerItself())
            {
                if (Player* caster_player =
                        static_cast<Unit*>(i_originalCaster)
                            ->GetCharmerOrOwnerPlayerOrPlayerItself())
                {
                    // No spells can ever be cast on a target off the
                    // opposing team in a sanctuary
                    if (target_player->HasFlag(
                            PLAYER_FLAGS, PLAYER_FLAGS_SANCTUARY) &&
                        caster_player->GetTeam() != target_player->GetTeam())
                        return;

                    // caster without PvP flag enabled cannot accidentally
                    // AoE an enemy
                    if (i_TargetType != SPELL_TARGETS_FRIENDLY &&
                        i_TargetType != SPELL_TARGETS_NOT_HOSTILE &&
                        caster_player->ShouldIgnoreTargetBecauseOfPvpFlag(
                            target_player))
                        return;
                }
            }
        }
        else if (i_originalCaster->GetTypeId() == TYPEID_GAMEOBJECT &&
                 i_TargetType != SPELL_TARGETS_FRIENDLY &&
                 i_TargetType != SPELL_TARGETS_NOT_HOSTILE)
        {
            // Hunter trap like objects
            if (Unit* owner =
                    static_cast<GameObject*>(i_originalCaster)->GetOwner())
            {
                if (Player* hunter =
                        static_cast<Unit*>(owner)
                            ->GetCharmerOrOwnerPlayerOrPlayerItself())
                    if (hunter->ShouldIgnoreTargetBecauseOfPvpFlag(elem))
                        return;
            }
        }

        if (i_originalCaster->GetTypeId() == TYPEID_UNIT)
        {
            auto ccaster = static_cast<Creature*>(i_originalCaster);
            if (ccaster->GetCreatureInfo()->flags_extra &
                    CREATURE_FLAG_EXTRA_AGGRESSIVE_PLAYER_DEMON &&
                !ccaster->getThreatManager().hasTarget(elem) &&
                !elem->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PVP) &&
                (elem->GetTypeId() == TYPEID_PLAYER ||
                    static_cast<Creature*>(elem)->IsPlayerPet()))
                return;
        }

        switch (i_TargetType)
        {
        case SPELL_TARGETS_HOSTILE:
            if (!i_originalCaster->IsHostileTo(elem))
                return;
            break;
        case SPELL_TARGETS_NOT_FRIENDLY:
            if (i_originalCaster->IsFriendlyTo(elem))
                return;
            break;
        case SPELL_TARGETS_NOT_HOSTILE:
            if (i_originalCaster->IsHostileTo(elem))
                return;
            break;
        case SPELL_TARGETS_FRIENDLY:
            if (!i_originalCaster->IsFriendlyTo(elem))
                return;
            break;
        case SPELL_TARGETS_AOE_DAMAGE:
        {
            if (elem->GetTypeId() == TYPEID_UNIT &&
                ((Creature*)elem)->IsTotem())
                return;

            if (i_playerControlled)
            {
                if (i_originalCaster->IsFriendlyTo(elem))
                    return;
            }
            else
            {
                if (!i_originalCaster->IsHostileTo(elem))
                    return;
            }
        }
        break;
        case SPELL_TARGETS_ALL:
            break;
        default:
            return;
        }

        // we don't need to check InMap here, it's already done some lines
        // above
        switch (i_push_type)
        {
        case PUSH_IN_FRONT:
            if (i_castingObject->isInFront(
                    (Unit*)(elem), i_radius, 2 * M_PI_F / 3))
                i_data->push_back(elem);
            break;
        case PUSH_IN_FRONT_90:
            if (i_castingObject->isInFront((Unit*)(elem), i_radius, M_PI_F / 2))
                i_data->push_back(elem);
            break;
        case PUSH_IN_FRONT_15:
            if (i_castingObject->isInFront(
                    (Unit*)(elem), i_radius, M_PI_F / 12))
                i_data->push_back(elem);
            break;
        case PUSH_IN_BACK:
            if (i_castingObject->isInBack(
                    (Unit*)(elem), i_radius, 2 * M_PI_F / 3))
                i_data->push_back(elem);
            break;
        case PUSH_IN_BACK_90:
            if (i_castingObject->isInBack((Unit*)(elem), i_radius, M_PI_F / 2))
                i_data->push_back(elem);
            break;
        case PUSH_IN_BACK_15:
            if (i_castingObject->isInBack((Unit*)(elem), i_radius, M_PI_F / 12))
                i_data->push_back(elem);
            break;
        case PUSH_SELF_CENTER:
            if (i_castingObject->IsWithinDist((Unit*)(elem), i_radius))
                i_data->push_back(elem);
            break;
        case PUSH_DEST_CENTER:
            if (elem->IsWithinDist3d(i_spell.m_targets.m_destX,
                    i_spell.m_targets.m_destY, i_spell.m_targets.m_destZ,
                    i_radius))
                i_data->push_back(elem);
            break;
        case PUSH_TARGET_CENTER:
            if (i_spell.m_targets.getUnitTarget() &&
                i_spell.m_targets.getUnitTarget()->IsWithinDist(
                    (Unit*)(elem), i_radius))
                i_data->push_back(elem);
            break;
        }
    }
};

/**
 * Fill target list by units around (x,y) points at radius distance

 * @param targetUnitMap        Reference to target list that filled by function
 * @param x                    X coordinates of center point for target search
 * @param y                    Y coordinates of center point for target search
 * @param radius               Radius around (x,y) for target search
 * @param pushType             Additional rules for target area selection (in
 front, angle, etc)
 * @param spellTargets         Additional rules for target selection base at
 hostile/friendly state to original spell caster
 * @param originalCaster       If provided set alternative original caster, if
 =NULL then used Spell::GetAffectiveObject() return
 */
void Spell::FillAreaTargets(UnitList& targetUnitMap, float radius,
    SpellNotifyPushType pushType, SpellTargets spellTargets,
    WorldObject* originalCaster /*=NULL*/)
{
    if (unlikely(radius <= 0))
        return;

    aoe_targets_worker notifier(
        *this, targetUnitMap, radius, pushType, spellTargets, originalCaster);

    if (!notifier.i_originalCaster || !notifier.i_castingObject ||
        !maps::verify_coords(notifier.i_centerX, notifier.i_centerY))
        return;

    framework::grid::single_visitor<Player, Creature, Pet, SpecialVisCreature,
        TemporarySummon> visitor_obj;

    auto map = m_caster->GetMap();

    // Visit circle, and invoke callback for all T in range
    framework::grid::visit_circle(MAP_CELL_MID, MAP_CELL_SIZE,
        notifier.i_centerX, notifier.i_centerY, radius,
        [map, &notifier, &visitor_obj, radius](int x, int y) mutable
        {
            visitor_obj(x, y, map->get_map_grid().get_grid(),
                [&notifier, radius](auto&& t)
                {
                    // NOTE: We don't check dist, notifier does that if needed
                    notifier(t);
                });
        });
}

void Spell::FillRaidOrPartyTargets(UnitList& targetUnitMap, Unit* member,
    float radius, bool raid, bool withPets, bool withcaster)
{
    Player* pMember = member->GetCharmerOrOwnerPlayerOrPlayerItself();
    Group* group = pMember ? pMember->GetGroup() : nullptr;

    if (group && !pMember->duel)
    {
        uint8 subgroup = pMember->GetSubGroup();

        for (auto member : group->members(true))
        {
            if (member->isDead() &&
                !m_spellInfo->HasAttribute(SPELL_ATTR_EX3_CAST_ON_DEAD))
                continue;

            // IsHostileTo check duel and controlled by enemy
            if ((raid || subgroup == member->GetSubGroup()) &&
                !m_caster->IsHostileTo(member) && !member->duel)
            {
                if ((member == m_caster && withcaster) ||
                    (member != m_caster &&
                        m_caster->IsWithinDistInMap(member, radius)))
                    targetUnitMap.push_back(member);

                if (withPets)
                    if (Pet* pet = member->GetPet())
                        if ((pet == m_caster && withcaster) ||
                            (pet != m_caster &&
                                m_caster->IsWithinDistInMap(pet, radius)))
                            targetUnitMap.push_back(pet);
            }
        }
    }
    else
    {
        Unit* ownerOrSelf =
            pMember ? pMember : member->GetCharmerOrOwnerOrSelf();
        if ((ownerOrSelf == m_caster && withcaster) ||
            (ownerOrSelf != m_caster &&
                m_caster->IsWithinDistInMap(ownerOrSelf, radius)))
            targetUnitMap.push_back(ownerOrSelf);

        if (withPets)
            if (Pet* pet = ownerOrSelf->GetPet())
                if ((pet == m_caster && withcaster) ||
                    (pet != m_caster &&
                        m_caster->IsWithinDistInMap(pet, radius)))
                    targetUnitMap.push_back(pet);
    }
}

WorldObject* Spell::GetAffectiveCasterObject() const
{
    if (!m_originalCasterGUID)
        return m_caster;

    if (m_originalCasterGUID.IsGameObject() && m_caster->IsInWorld())
        return m_caster->GetMap()->GetGameObject(m_originalCasterGUID);
    return m_originalCaster;
}

WorldObject* Spell::GetCastingObject() const
{
    if (m_originalCasterGUID.IsGameObject())
        return m_caster->IsInWorld() ?
                   m_caster->GetMap()->GetGameObject(m_originalCasterGUID) :
                   nullptr;
    else
        return m_caster;
}

void Spell::ResetEffectDamageAndHeal()
{
    m_damage = 0;
    m_healing = 0;
}

void Spell::SelectMountByAreaAndSkill(Unit* target,
    SpellEntry const* parentSpell, uint32 spellId75, uint32 spellId150,
    uint32 spellId225, uint32 spellId300, uint32 spellIdSpecial)
{
    if (!target || target->GetTypeId() != TYPEID_PLAYER)
        return;

    // Prevent stacking of mounts
    target->remove_auras(SPELL_AURA_MOUNTED);
    uint16 skillval = ((Player*)target)->GetSkillValue(SKILL_RIDING);
    if (!skillval)
        return;

    if (skillval >= 225 && (spellId300 > 0 || spellId225 > 0))
    {
        uint32 spellid = skillval >= 300 ? spellId300 : spellId225;
        SpellEntry const* pSpell = sSpellStore.LookupEntry(spellid);
        if (!pSpell)
        {
            logging.error(
                "SelectMountByAreaAndSkill: unknown spell id %i by caster: %s",
                spellid, target->GetGuidStr().c_str());
            return;
        }

        // zone check
        uint32 zone, area;
        target->GetZoneAndAreaId(zone, area);

        SpellCastResult locRes =
            sSpellMgr::Instance()->GetSpellAllowedInLocationError(pSpell,
                target->GetMapId(), zone, area,
                target->GetCharmerOrOwnerPlayerOrPlayerItself());
        if (locRes != SPELL_CAST_OK)
            target->CastSpell(target, spellId150, true, nullptr, nullptr,
                ObjectGuid(), parentSpell);
        else if (spellIdSpecial > 0)
        {
            for (PlayerSpellMap::const_iterator iter =
                     ((Player*)target)->GetSpellMap().begin();
                 iter != ((Player*)target)->GetSpellMap().end(); ++iter)
            {
                if (iter->second.state != PLAYERSPELL_REMOVED)
                {
                    SpellEntry const* spellInfo =
                        sSpellStore.LookupEntry(iter->first);
                    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
                    {
                        if (spellInfo->EffectApplyAuraName[i] ==
                            SPELL_AURA_MOD_FLIGHT_SPEED_MOUNTED)
                        {
                            int32 mountSpeed = spellInfo->CalculateSimpleValue(
                                SpellEffectIndex(i));

                            // speed higher than 280 replace it
                            if (mountSpeed > 280)
                            {
                                target->CastSpell(target, spellIdSpecial, true,
                                    nullptr, nullptr, ObjectGuid(),
                                    parentSpell);
                                return;
                            }
                        }
                    }
                }
            }
            target->CastSpell(target, pSpell, true, nullptr, nullptr,
                ObjectGuid(), parentSpell);
        }
        else
            target->CastSpell(target, pSpell, true, nullptr, nullptr,
                ObjectGuid(), parentSpell);
    }
    else if (skillval >= 150 && spellId150 > 0)
        target->CastSpell(target, spellId150, true, nullptr, nullptr,
            ObjectGuid(), parentSpell);
    else if (spellId75 > 0)
        target->CastSpell(target, spellId75, true, nullptr, nullptr,
            ObjectGuid(), parentSpell);

    return;
}

void Spell::set_cast_item(Item* target)
{
    // NOTE: don't modify current m_CastItem if any

    m_CastItem = target;
    if (target)
        target->add_referencing_spell(this);
}

void Spell::ClearCastItem()
{
    if (m_CastItem)
        m_CastItem->remove_referencing_spell(this);

    if (m_CastItem == m_targets.getItemTarget())
        m_targets.setItemTarget(nullptr);

    m_CastItem = nullptr;
}

bool Spell::IsWandAttack() const
{
    return m_attackType == RANGED_ATTACK &&
           (m_caster->getClassMask() & CLASSMASK_WAND_USERS) != 0 &&
           m_caster->GetTypeId() == TYPEID_PLAYER;
}

bool Spell::HasGlobalCooldown()
{
    // global cooldown have only player or controlled units
    if (m_caster->GetCharmInfo())
        return m_caster->GetCharmInfo()
            ->GetGlobalCooldownMgr()
            .HasGlobalCooldown(m_spellInfo);
    else if (m_caster->GetTypeId() == TYPEID_PLAYER)
        return ((Player*)m_caster)
            ->GetGlobalCooldownMgr()
            .HasGlobalCooldown(m_spellInfo);
    else
        return false;
}

void Spell::TriggerGlobalCooldown()
{
    int32 gcd = m_spellInfo->StartRecoveryTime;
    if (!gcd)
        return;

    // global cooldown can't leave range 1..1.5 secs (if it it)
    // exist some spells (mostly not player directly casted) that have < 1 sec
    // and > 1.5 sec global cooldowns
    // but its as test show not affected any spell mods.
    if (gcd >= 1000 && gcd <= 1500)
    {
        // apply haste rating
        gcd = int32(float(gcd) * m_caster->GetFloatValue(UNIT_MOD_CAST_SPEED));

        if (gcd < 1000)
            gcd = 1000;
        else if (gcd > 1500)
            gcd = 1500;
    }

    // global cooldown have only player or controlled units
    if (m_caster->GetCharmInfo())
        m_caster->GetCharmInfo()->GetGlobalCooldownMgr().AddGlobalCooldown(
            m_spellInfo, gcd);
    else if (m_caster->GetTypeId() == TYPEID_PLAYER)
        ((Player*)m_caster)
            ->GetGlobalCooldownMgr()
            .AddGlobalCooldown(m_spellInfo, gcd);
}

void Spell::CancelGlobalCooldown(bool keepHandsOff)
{
    if (!m_spellInfo->StartRecoveryTime)
        return;

    // cancel global cooldown when interrupting current cast
    if (!keepHandsOff &&
        m_caster->GetCurrentSpell(CURRENT_GENERIC_SPELL) != this)
        return;

    // global cooldown have only player or controlled units
    if (m_caster->GetCharmInfo())
        m_caster->GetCharmInfo()->GetGlobalCooldownMgr().CancelGlobalCooldown(
            m_spellInfo);
    else if (m_caster->GetTypeId() == TYPEID_PLAYER)
        ((Player*)m_caster)
            ->GetGlobalCooldownMgr()
            .CancelGlobalCooldown(m_spellInfo);
}

bool Spell::GetSpreadingRadius(float maxRadius, float& moddedRadius) const
{
    switch (m_spellInfo->Id)
    {
    // Poison clouds with even spread:
    case 30915: // Broggok normal
    case 38463: // Broggok heroic
    case 28241:
        if (m_triggeredByAuraSpell)
        {
            AuraHolder* holder = m_caster->get_aura(m_triggeredByAuraSpell->Id);
            if (holder && holder->GetAuraMaxDuration() > 0)
            {
                // The cloud has a constant spread so radius is max *
                // (elapsed_time / max_time)
                int32 elapsedDuration =
                    holder->GetAuraMaxDuration() - holder->GetAuraDuration();
                moddedRadius =
                    maxRadius * ((float)elapsedDuration /
                                    (float)holder->GetAuraMaxDuration());
                return true;
            }
        }
    default:
        break;
    }
    return false;
}

AuraHolder* Spell::consume_magic_buff(Unit* target)
{
    std::vector<AuraHolder*> valid_buffs;

    target->loop_auras([&valid_buffs](AuraHolder* holder)
        {
            // AuraHolder::IsPositive has the exact same logic as
            // IsPositiveSpell
            // except it retrieves a cached value calculated from
            // IsPositiveSpell on its creation
            if (!holder->IsPositive())
                return true; // continue

            const SpellEntry* info = holder->GetSpellProto();
            if (info->Dispel != DISPEL_MAGIC ||
                info->SpellFamilyName != SPELLFAMILY_PRIEST ||
                (info->manaCost == 0 &&
                    info->ManaCostPercentage == 0)) // Spells can either have a
                                                    // flat base mana cost or a
                                                    // % of base mana. Spells
                                                    // can never have mana/level
                                                    // without a base cost
                return true;                        // continue

            valid_buffs.push_back(holder);
            return true; // continue
        });

    if (!valid_buffs.empty())
        return valid_buffs[urand(0, valid_buffs.size() - 1)];
    return nullptr;
}
