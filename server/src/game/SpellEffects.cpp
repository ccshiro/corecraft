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

#include "BattleGround.h"
#include "BattleGroundEY.h"
#include "BattleGroundMgr.h"
#include "BattleGroundWS.h"
#include "Common.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "DynamicObject.h"
#include "GameObject.h"
#include "GossipDef.h"
#include "Group.h"
#include "Language.h"
#include "logging.h"
#include "Mail.h"
#include "MapManager.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "Pet.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SharedDefines.h"
#include "SkillExtraItems.h"
#include "SocialMgr.h"
#include "SpecialVisCreature.h"
#include "Totem.h"
#include "TemporarySummon.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "TemporarySummon.h"
#include "Totem.h"
#include "Unit.h"
#include "UpdateData.h"
#include "UpdateMask.h"
#include "Util.h"
#include "VMapFactory.h"
#include "World.h"
#include "WorldPacket.h"
#include "loot_distributor.h"
#include "pet_behavior.h"
#include "Database/DatabaseEnv.h"
#include "maps/checks.h"
#include "maps/visitors.h"
#include "movement/IdleMovementGenerator.h"
#include "movement/PointMovementGenerator.h"
#include "movement/TargetedMovementGenerator.h"
#include <memory>

static auto& gm_logger = logging.get_logger("gm.command");

pEffect SpellEffects[TOTAL_SPELL_EFFECTS] = {
    &Spell::EffectNULL,      //  0
    &Spell::EffectInstaKill, //  1 SPELL_EFFECT_INSTAKILL
    &Spell::EffectSchoolDMG, //  2 SPELL_EFFECT_SCHOOL_DAMAGE
    &Spell::EffectDummy,     //  3 SPELL_EFFECT_DUMMY
    &Spell::EffectUnused,    //  4 SPELL_EFFECT_PORTAL_TELEPORT          unused
                             //  from pre-1.2.1
    &Spell::EffectTeleportUnits,    //  5 SPELL_EFFECT_TELEPORT_UNITS
    &Spell::EffectApplyAura,        //  6 SPELL_EFFECT_APPLY_AURA
    &Spell::EffectEnvironmentalDMG, //  7 SPELL_EFFECT_ENVIRONMENTAL_DAMAGE
    &Spell::EffectPowerDrain,       //  8 SPELL_EFFECT_POWER_DRAIN
    &Spell::EffectHealthLeech,      //  9 SPELL_EFFECT_HEALTH_LEECH
    &Spell::EffectHeal,             // 10 SPELL_EFFECT_HEAL
    &Spell::EffectBind,             // 11 SPELL_EFFECT_BIND
    &Spell::EffectUnused, // 12 SPELL_EFFECT_PORTAL                   unused
                          // from pre-1.2.1, exist 2 spell, but not exist any
                          // data about its real usage
    &Spell::EffectUnused, // 13 SPELL_EFFECT_RITUAL_BASE              unused
                          // from pre-1.2.1
    &Spell::EffectUnused, // 14 SPELL_EFFECT_RITUAL_SPECIALIZE        unused
                          // from pre-1.2.1
    &Spell::EffectUnused, // 15 SPELL_EFFECT_RITUAL_ACTIVATE_PORTAL   unused
                          // from pre-1.2.1
    &Spell::EffectQuestComplete,   // 16 SPELL_EFFECT_QUEST_COMPLETE
    &Spell::EffectWeaponDmg,       // 17 SPELL_EFFECT_WEAPON_DAMAGE_NOSCHOOL
    &Spell::EffectResurrect,       // 18 SPELL_EFFECT_RESURRECT
    &Spell::EffectAddExtraAttacks, // 19 SPELL_EFFECT_ADD_EXTRA_ATTACKS
    &Spell::EffectEmpty, // 20 SPELL_EFFECT_DODGE                    one spell:
                         // Dodge
    &Spell::EffectEmpty, // 21 SPELL_EFFECT_EVADE                    one spell:
                         // Evade (DND)
    &Spell::EffectParry, // 22 SPELL_EFFECT_PARRY
    &Spell::EffectBlock, // 23 SPELL_EFFECT_BLOCK                    one spell:
                         // Block
    &Spell::EffectCreateItem, // 24 SPELL_EFFECT_CREATE_ITEM
    &Spell::EffectEmpty, // 25 SPELL_EFFECT_WEAPON                   spell per
                         // weapon type, in ItemSubclassmask store mask that can
                         // be used for usability check at equip, but current
                         // way using skill also work.
    &Spell::EffectEmpty, // 26 SPELL_EFFECT_DEFENSE                  one spell:
                         // Defense
    &Spell::EffectPersistentAA,        // 27 SPELL_EFFECT_PERSISTENT_AREA_AURA
    &Spell::EffectSummonType,          // 28 SPELL_EFFECT_SUMMON
    &Spell::EffectLeapForward,         // 29 SPELL_EFFECT_LEAP
    &Spell::EffectEnergize,            // 30 SPELL_EFFECT_ENERGIZE
    &Spell::EffectWeaponDmg,           // 31 SPELL_EFFECT_WEAPON_PERCENT_DAMAGE
    &Spell::EffectTriggerMissileSpell, // 32 SPELL_EFFECT_TRIGGER_MISSILE
    &Spell::EffectOpenLock,            // 33 SPELL_EFFECT_OPEN_LOCK
    &Spell::EffectSummonChangeItem,    // 34 SPELL_EFFECT_SUMMON_CHANGE_ITEM
    &Spell::EffectApplyAreaAura,       // 35 SPELL_EFFECT_APPLY_AREA_AURA_PARTY
    &Spell::EffectLearnSpell,          // 36 SPELL_EFFECT_LEARN_SPELL
    &Spell::EffectEmpty,  // 37 SPELL_EFFECT_SPELL_DEFENSE            one spell:
                          // SPELLDEFENSE (DND)
    &Spell::EffectDispel, // 38 SPELL_EFFECT_DISPEL
    &Spell::EffectEmpty,  // 39 SPELL_EFFECT_LANGUAGE                 misc store
                          // lang id
    &Spell::EffectDualWield, // 40 SPELL_EFFECT_DUAL_WIELD
    &Spell::EffectUnused, // 41 SPELL_EFFECT_41 (old SPELL_EFFECT_SUMMON_WILD)
    &Spell::EffectUnused, // 42 SPELL_EFFECT_42 (old
                          // SPELL_EFFECT_SUMMON_GUARDIAN)
    &Spell::
        EffectTeleUnitsFaceCaster, // 43 SPELL_EFFECT_TELEPORT_UNITS_FACE_CASTER
    &Spell::EffectLearnSkill,      // 44 SPELL_EFFECT_SKILL_STEP
    &Spell::EffectAddHonor, // 45 SPELL_EFFECT_ADD_HONOR honor/pvp related
    &Spell::EffectNULL, // 46 SPELL_EFFECT_SPAWN                    spawn/login
                        // animation, expected by spawn unit cast, also base
                        // points store some dynflags
    &Spell::EffectTradeSkill, // 47 SPELL_EFFECT_TRADE_SKILL
    &Spell::EffectUnused, // 48 SPELL_EFFECT_STEALTH                  one spell:
                          // Base Stealth
    &Spell::EffectUnused, // 49 SPELL_EFFECT_DETECT                   one spell:
                          // Detect
    &Spell::EffectTransmitted, // 50 SPELL_EFFECT_TRANS_DOOR
    &Spell::EffectUnused, // 51 SPELL_EFFECT_FORCE_CRITICAL_HIT       unused
                          // from pre-1.2.1
    &Spell::EffectUnused, // 52 SPELL_EFFECT_GUARANTEE_HIT            unused
                          // from pre-1.2.1
    &Spell::EffectEnchantItemPerm, // 53 SPELL_EFFECT_ENCHANT_ITEM
    &Spell::EffectEnchantItemTmp,  // 54 SPELL_EFFECT_ENCHANT_ITEM_TEMPORARY
    &Spell::EffectTameCreature,    // 55 SPELL_EFFECT_TAMECREATURE
    &Spell::EffectSummonPet,       // 56 SPELL_EFFECT_SUMMON_PET
    &Spell::EffectLearnPetSpell,   // 57 SPELL_EFFECT_LEARN_PET_SPELL
    &Spell::EffectWeaponDmg,       // 58 SPELL_EFFECT_WEAPON_DAMAGE
    &Spell::EffectOpenLock,        // 59 SPELL_EFFECT_OPEN_LOCK_ITEM
    &Spell::EffectProficiency,     // 60 SPELL_EFFECT_PROFICIENCY
    &Spell::EffectSendEvent,       // 61 SPELL_EFFECT_SEND_EVENT
    &Spell::EffectPowerBurn,       // 62 SPELL_EFFECT_POWER_BURN
    &Spell::EffectThreat,          // 63 SPELL_EFFECT_THREAT
    &Spell::EffectTriggerSpell,    // 64 SPELL_EFFECT_TRIGGER_SPELL
    &Spell::EffectUnused, // 65 SPELL_EFFECT_HEALTH_FUNNEL            unused
    &Spell::EffectUnused, // 66 SPELL_EFFECT_POWER_FUNNEL             unused
                          // from pre-1.2.1
    &Spell::EffectHealMaxHealth, // 67 SPELL_EFFECT_HEAL_MAX_HEALTH
    &Spell::EffectInterruptCast, // 68 SPELL_EFFECT_INTERRUPT_CAST
    &Spell::EffectDistract,      // 69 SPELL_EFFECT_DISTRACT
    &Spell::EffectPull, // 70 SPELL_EFFECT_PULL                     one spell:
                        // Distract Move
    &Spell::EffectPickPocket,  // 71 SPELL_EFFECT_PICKPOCKET
    &Spell::EffectAddFarsight, // 72 SPELL_EFFECT_ADD_FARSIGHT
    &Spell::EffectUnused,      // 73 SPELL_EFFECT_73 (old
                               // SPELL_EFFECT_SUMMON_POSSESSED
    &Spell::EffectUnused, // 74 SPELL_EFFECT_74 (old SPELL_EFFECT_SUMMON_TOTEM)
    &Spell::EffectHealMechanical,   // 75 SPELL_EFFECT_HEAL_MECHANICAL
                                    // one spell: Mechanical Patch Kit
    &Spell::EffectSummonObjectWild, // 76 SPELL_EFFECT_SUMMON_OBJECT_WILD
    &Spell::EffectScriptEffect,     // 77 SPELL_EFFECT_SCRIPT_EFFECT
    &Spell::EffectUnused,           // 78 SPELL_EFFECT_ATTACK
    &Spell::EffectSanctuary,        // 79 SPELL_EFFECT_SANCTUARY
    &Spell::EffectAddComboPoints,   // 80 SPELL_EFFECT_ADD_COMBO_POINTS
    &Spell::EffectUnused, // 81 SPELL_EFFECT_CREATE_HOUSE             one spell:
                          // Create House (TEST)
    &Spell::EffectNULL,   // 82 SPELL_EFFECT_BIND_SIGHT
    &Spell::EffectDuel,   // 83 SPELL_EFFECT_DUEL
    &Spell::EffectStuck,  // 84 SPELL_EFFECT_STUCK
    &Spell::EffectSummonPlayer,   // 85 SPELL_EFFECT_SUMMON_PLAYER
    &Spell::EffectActivateObject, // 86 SPELL_EFFECT_ACTIVATE_OBJECT
    &Spell::EffectUnused,         // 87 SPELL_EFFECT_87 (old
                                  // SPELL_EFFECT_SUMMON_TOTEM_SLOT1)
    &Spell::EffectUnused,         // 88 SPELL_EFFECT_88 (old
                                  // SPELL_EFFECT_SUMMON_TOTEM_SLOT2)
    &Spell::EffectUnused,         // 89 SPELL_EFFECT_89 (old
                                  // SPELL_EFFECT_SUMMON_TOTEM_SLOT3)
    &Spell::EffectUnused,         // 90 SPELL_EFFECT_90 (old
                                  // SPELL_EFFECT_SUMMON_TOTEM_SLOT4)
    &Spell::EffectUnused, // 91 SPELL_EFFECT_THREAT_ALL               one spell:
                          // zzOLDBrainwash
    &Spell::EffectEnchantHeldItem,     // 92 SPELL_EFFECT_ENCHANT_HELD_ITEM
    &Spell::EffectUnused,              // 93 SPELL_EFFECT_93 (old
                                       // SPELL_EFFECT_SUMMON_PHANTASM)
    &Spell::EffectSelfResurrect,       // 94 SPELL_EFFECT_SELF_RESURRECT
    &Spell::EffectSkinning,            // 95 SPELL_EFFECT_SKINNING
    &Spell::EffectCharge,              // 96 SPELL_EFFECT_CHARGE
    &Spell::EffectUnused,              // 97 SPELL_EFFECT_97 (old
                                       // SPELL_EFFECT_SUMMON_CRITTER)
    &Spell::EffectKnockBack,           // 98 SPELL_EFFECT_KNOCK_BACK
    &Spell::EffectDisEnchant,          // 99 SPELL_EFFECT_DISENCHANT
    &Spell::EffectInebriate,           // 100 SPELL_EFFECT_INEBRIATE
    &Spell::EffectFeedPet,             // 101 SPELL_EFFECT_FEED_PET
    &Spell::EffectDismissPet,          // 102 SPELL_EFFECT_DISMISS_PET
    &Spell::EffectReputation,          // 103 SPELL_EFFECT_REPUTATION
    &Spell::EffectSummonObject,        // 104 SPELL_EFFECT_SUMMON_OBJECT_SLOT1
    &Spell::EffectSummonObject,        // 105 SPELL_EFFECT_SUMMON_OBJECT_SLOT2
    &Spell::EffectSummonObject,        // 106 SPELL_EFFECT_SUMMON_OBJECT_SLOT3
    &Spell::EffectSummonObject,        // 107 SPELL_EFFECT_SUMMON_OBJECT_SLOT4
    &Spell::EffectDispelMechanic,      // 108 SPELL_EFFECT_DISPEL_MECHANIC
    &Spell::EffectSummonDeadPet,       // 109 SPELL_EFFECT_SUMMON_DEAD_PET
    &Spell::EffectDestroyAllTotems,    // 110 SPELL_EFFECT_DESTROY_ALL_TOTEMS
    &Spell::EffectDurabilityDamage,    // 111 SPELL_EFFECT_DURABILITY_DAMAGE
    &Spell::EffectUnused,              // 112 SPELL_EFFECT_112 (old
                                       // SPELL_EFFECT_SUMMON_DEMON)
    &Spell::EffectResurrectNew,        // 113 SPELL_EFFECT_RESURRECT_NEW
    &Spell::EffectTaunt,               // 114 SPELL_EFFECT_ATTACK_ME
    &Spell::EffectDurabilityDamagePCT, // 115 SPELL_EFFECT_DURABILITY_DAMAGE_PCT
    &Spell::EffectSkinPlayerCorpse,    // 116 SPELL_EFFECT_SKIN_PLAYER_CORPSE
                                       // one spell: Remove Insignia, bg usage,
                                       // required special corpse flags...
    &Spell::EffectSpiritHeal, // 117 SPELL_EFFECT_SPIRIT_HEAL              one
                              // spell: Spirit Heal
    &Spell::EffectSkill,      // 118 SPELL_EFFECT_SKILL professions and more
    &Spell::EffectApplyAreaAura, // 119 SPELL_EFFECT_APPLY_AREA_AURA_PET
    &Spell::EffectUnused,    // 120 SPELL_EFFECT_TELEPORT_GRAVEYARD       one
                             // spell: Graveyard Teleport Test
    &Spell::EffectWeaponDmg, // 121 SPELL_EFFECT_NORMALIZED_WEAPON_DMG
    &Spell::EffectUnused,    // 122 SPELL_EFFECT_122                      unused
    &Spell::EffectSendTaxi,  // 123 SPELL_EFFECT_SEND_TAXI
                             // taxi/flight related (misc value is taxi path id)
    &Spell::EffectPlayerPull, // 124 SPELL_EFFECT_PLAYER_PULL
                              // opposite of knockback effect (pulls player
                              // twoard caster)
    &Spell::EffectModifyThreatPercent, // 125 SPELL_EFFECT_MODIFY_THREAT_PERCENT
    &Spell::EffectStealBeneficialBuff, // 126 SPELL_EFFECT_STEAL_BENEFICIAL_BUFF
                                       // spell steal effect?
    &Spell::EffectProspecting, // 127 SPELL_EFFECT_PROSPECTING Prospecting spell
    &Spell::EffectApplyAreaAura,  // 128 SPELL_EFFECT_APPLY_AREA_AURA_FRIEND
    &Spell::EffectApplyAreaAura,  // 129 SPELL_EFFECT_APPLY_AREA_AURA_ENEMY
    &Spell::EffectRedirectThreat, // 130 SPELL_EFFECT_REDIRECT_THREAT
    &Spell::EffectPlaySound, // 131 SPELL_EFFECT_PLAY_SOUND               sound
                             // id in misc value (SoundEntries.dbc)
    &Spell::EffectPlayMusic, // 132 SPELL_EFFECT_PLAY_MUSIC               sound
                             // id in misc value (SoundEntries.dbc)
    &Spell::EffectUnlearnSpecialization, // 133
                                         // SPELL_EFFECT_UNLEARN_SPECIALIZATION
                                         // unlearn profession specialization
    &Spell::EffectKillCreditGroup,       // 134 SPELL_EFFECT_KILL_CREDIT_GROUP
                                         // misc value is creature entry
    &Spell::EffectNULL,                  // 135 SPELL_EFFECT_CALL_PET
    &Spell::EffectHealPct,               // 136 SPELL_EFFECT_HEAL_PCT
    &Spell::EffectEnergisePct,           // 137 SPELL_EFFECT_ENERGIZE_PCT
    &Spell::EffectLeapBack, // 138 SPELL_EFFECT_LEAP_BACK                Leap
                            // back
    &Spell::EffectUnused,   // 139 SPELL_EFFECT_CLEAR_QUEST              (misc -
                            // is quest ID), unused
    &Spell::EffectForceCast, // 140 SPELL_EFFECT_FORCE_CAST
    &Spell::EffectNULL, // 141 SPELL_EFFECT_141                      damage and
                        // reduce speed?
    &Spell::EffectTriggerSpellWithValue, // 142
    // SPELL_EFFECT_TRIGGER_SPELL_WITH_VALUE
    &Spell::EffectApplyAreaAura, // 143 SPELL_EFFECT_APPLY_AREA_AURA_OWNER
    &Spell::EffectNULL, // 144 SPELL_EFFECT_144                      Spectral
                        // Blast
    &Spell::EffectNULL, // 145 SPELL_EFFECT_145                      Black Hole
                        // Effect
    &Spell::EffectUnused,    // 146 SPELL_EFFECT_146                      unused
    &Spell::EffectQuestFail, // 147 SPELL_EFFECT_QUEST_FAIL               quest
                             // fail
    &Spell::EffectUnused,    // 148 SPELL_EFFECT_148                      unused
    &Spell::EffectCharge2,   // 149 SPELL_EFFECT_CHARGE2                  swoop
    &Spell::EffectUnused,    // 150 SPELL_EFFECT_150                      unused
    &Spell::EffectTriggerRitualOfSummoning, // 151 SPELL_EFFECT_TRIGGER_SPELL_2
    &Spell::EffectNULL, // 152 SPELL_EFFECT_152                      summon
                        // Refer-a-Friend
    &Spell::EffectNULL, // 153 SPELL_EFFECT_CREATE_PET               misc value
                        // is creature entry
};

void Spell::EffectEmpty(SpellEffectIndex /*eff_idx*/)
{
    // NOT NEED ANY IMPLEMENTATION CODE, EFFECT POSISBLE USED AS MARKER OR
    // CLIENT INFORM
}

void Spell::EffectNULL(SpellEffectIndex /*eff_idx*/)
{
    LOG_DEBUG(logging, "WORLD: Spell Effect DUMMY");
}

void Spell::EffectUnused(SpellEffectIndex /*eff_idx*/)
{
    // NOT USED BY ANY SPELL OR USELESS OR IMPLEMENTED IN DIFFERENT WAY IN
    // MANGOS
}

void Spell::EffectResurrectNew(SpellEffectIndex eff_idx)
{
    if (!unitTarget || unitTarget->isAlive())
        return;

    if (unitTarget->GetTypeId() != TYPEID_PLAYER)
        return;

    if (!unitTarget->IsInWorld())
        return;

    Player* pTarget = ((Player*)unitTarget);

    if (pTarget->isRessurectRequested()) // already have one active request
        return;

    uint32 health = damage;
    uint32 mana = m_spellInfo->EffectMiscValue[eff_idx];
    pTarget->setResurrectRequestData(m_caster->GetObjectGuid(),
        m_caster->GetMapId(), m_caster->GetX(), m_caster->GetY(),
        m_caster->GetZ(), health, mana);
    SendResurrectRequest(pTarget);
}

void Spell::EffectInstaKill(SpellEffectIndex /*eff_idx*/)
{
    if (!unitTarget || !unitTarget->isAlive())
        return;

    // Demonic Sacrifice
    if (m_spellInfo->Id == 18788 && unitTarget->GetTypeId() == TYPEID_UNIT)
    {
        uint32 entry = unitTarget->GetEntry();
        uint32 spellID;
        switch (entry)
        {
        case 416:
            spellID = 18789;
            break; // imp
        case 417:
            spellID = 18792;
            break; // fellhunter
        case 1860:
            spellID = 18790;
            break; // void
        case 1863:
            spellID = 18791;
            break; // succubus
        case 17252:
            spellID = 35701;
            break; // fellguard
        default:
            logging.error(
                "EffectInstaKill: Unhandled creature entry (%u) case.", entry);
            return;
        }

        m_caster->CastSpell(m_caster, spellID, true);
    }

    if (m_caster == unitTarget) // prevent interrupt message
        ignore_interrupt_ = true;

    WorldObject* caster =
        GetCastingObject(); // we need the original casting object

    WorldPacket data(SMSG_SPELLINSTAKILLLOG, (8 + 8 + 4));
    data << (caster && caster->GetTypeId() != TYPEID_GAMEOBJECT ?
                 m_caster->GetObjectGuid() :
                 ObjectGuid());          // Caster GUID
    data << unitTarget->GetObjectGuid(); // Victim GUID
    data << uint32(m_spellInfo->Id);
    m_caster->SendMessageToSet(&data, true);

    m_caster->Kill(unitTarget, false);

    // don't resummon after demonic sacrifice in BGs
    if (m_spellInfo->Id == 18788 && m_caster->GetTypeId() == TYPEID_PLAYER)
        static_cast<Player*>(m_caster)->SetBgResummonGuid(ObjectGuid());
}

void Spell::EffectEnvironmentalDMG(SpellEffectIndex eff_idx)
{
    if (m_caster->GetTypeId() != TYPEID_PLAYER)
        return;

    damage = m_spellInfo->CalculateSimpleValue(eff_idx);

    static_cast<Player*>(m_caster)->EnvironmentalDamage(DAMAGE_FIRE, damage);
}

void Spell::EffectSchoolDMG(SpellEffectIndex effect_idx)
{
    if (unitTarget && unitTarget->isAlive())
    {
        switch (m_spellInfo->SpellFamilyName)
        {
        case SPELLFAMILY_GENERIC:
        {
            // Gore
            if (m_spellInfo->SpellIconID == 2269)
                damage += rand_norm_f() < 0.5f ? damage : 0;

            switch (m_spellInfo->Id) // better way to check unknown
            {
            // Meteor like spells (divided damage to targets)
            case 24340:
            case 26558:
            case 28884: // Meteor
            case 36837:
            case 38903:
            case 41276: // Meteor
            case 26789: // Shard of the Fallen Star
            case 31436: // Malevolent Cleave
            case 35181: // Dive Bomb
            case 40810:
            case 43267:
            case 43268: // Saber Lash
            case 42384: // Brutal Swipe
            case 45150: // Meteor Slash
            {
                uint32 count = 0;
                for (TargetList::const_iterator ihit =
                         m_UniqueTargetInfo.begin();
                     ihit != m_UniqueTargetInfo.end(); ++ihit)
                    if (ihit->effectMask & (1 << effect_idx))
                        ++count;

                damage /= count; // divide to all targets
                break;
            }
            // percent from health with min
            case 25599: // Thundercrash
            {
                damage = unitTarget->GetHealth() / 2;
                if (damage < 200)
                    damage = 200;
                break;
            }
            // Void Zone
            case 28865: // Consumption
            {
                int dmg = 0;

                if (auto spell = sSpellStore.LookupEntry(
                        m_caster->GetUInt32Value(UNIT_CREATED_BY_SPELL)))
                {
                    if (spell->Effect[0] == SPELL_EFFECT_SUMMON &&
                        spell->EffectMiscValue[0] == 16697)
                    {
                        dmg = spell->EffectBasePoints[1] + 1;
                        if (dmg > 200)
                            dmg = urand(dmg - 200, dmg);
                    }
                }

                if (dmg > 0)
                    damage = dmg;
                break;
            }
            // Cataclysmic Bolt
            case 38441:
                damage = unitTarget->GetMaxHealth() / 2;
                break;
            // Hellfire tick in Mechanar, Raging Flames
            case 35283:
            {
                if (m_caster->GetMap()->IsRegularDifficulty())
                    damage = 1500;
                else
                    damage = 3000;
                break;
            }
            // Gruul's Shatter
            case 33671:
            {
                float point_dist =
                    G3D::distance(unitTarget->GetX() - m_caster->GetX(),
                        unitTarget->GetY() - m_caster->GetY(),
                        unitTarget->GetZ() - m_caster->GetZ());
                damage = 13400 * (1.0f - point_dist / 20.1f);
                break;
            }
            }
            break;
        }
        case SPELLFAMILY_WARRIOR:
        {
            // Bloodthirst
            if (m_spellInfo->SpellFamilyFlags & UI64LIT(0x40000000000))
            {
                damage = uint32(
                    damage * (m_caster->GetTotalAttackPowerValue(BASE_ATTACK)) /
                    100);
            }
            // Shield Slam
            else if (m_spellInfo->SpellFamilyFlags & UI64LIT(0x100000000))
                damage += int32(m_caster->GetShieldBlockValue());
            // Victory Rush
            else if (m_spellInfo->SpellFamilyFlags & UI64LIT(0x10000000000))
            {
                damage = uint32(
                    damage * m_caster->GetTotalAttackPowerValue(BASE_ATTACK) /
                    100);
                m_caster->ModifyAuraState(
                    AURA_STATE_WARRIOR_VICTORY_RUSH, false);
            }
            break;
        }
        case SPELLFAMILY_WARLOCK:
        {
            // Incinerate Rank 1 & 2
            if ((m_spellInfo->SpellFamilyFlags & UI64LIT(0x00004000000000)) &&
                m_spellInfo->SpellIconID == 2128)
            {
                // Incinerate does more dmg (dmg*0.25) if the target is
                // Immolated.
                if (unitTarget->HasAuraState(AURA_STATE_CONFLAGRATE))
                    damage += int32(damage * 0.25);
            }
            // Conflagrate - consumes Immolate
            else if (m_spellInfo->TargetAuraState == AURA_STATE_CONFLAGRATE)
            {
                // for caster applied auras only
                auto& periodic =
                    unitTarget->GetAurasByType(SPELL_AURA_PERIODIC_DAMAGE);
                for (const auto& elem : periodic)
                {
                    if ((elem)->GetCasterGuid() == m_caster->GetObjectGuid() &&
                        // Immolate
                        (elem)->GetSpellProto()->IsFitToFamily(
                            SPELLFAMILY_WARLOCK, UI64LIT(0x0000000000000004)))
                    {
                        unitTarget->remove_auras((elem)->GetId(),
                            [this](AuraHolder* holder)
                            {
                                return holder->GetCasterGuid() ==
                                       m_caster->GetObjectGuid();
                            });
                        break;
                    }
                }
            }
            break;
        }
        case SPELLFAMILY_DRUID:
        {
            // Ferocious Bite
            if (m_spellInfo->SpellFamilyFlags & 0x800000 &&
                m_spellInfo->SpellVisual == 6587)
            {
                if (m_caster->GetTypeId() != TYPEID_PLAYER)
                    break;

                // scaling for ferocious bite is:
                // damage = (0.03 * cp) * AP + DmgMultiplier * (energy-35) +
                // flat
                // where flat is what damage currently is, and druid's energy
                // has already had the 35 subtracted
                float ap_coeff =
                    0.03f * static_cast<Player*>(m_caster)->GetComboPoints();
                damage += int32(
                    ap_coeff * m_caster->GetTotalAttackPowerValue(BASE_ATTACK));
                damage += int32(m_spellInfo->DmgMultiplier[effect_idx] *
                                m_caster->GetPower(POWER_ENERGY));
                m_caster->SetPower(POWER_ENERGY, 0);
            }
            // Rake
            else if (m_spellInfo->SpellFamilyFlags &
                         UI64LIT(0x0000000000001000) &&
                     m_spellInfo->Effect[EFFECT_INDEX_2] ==
                         SPELL_EFFECT_ADD_COMBO_POINTS)
            {
                // $AP*0.01 bonus
                damage += int32(
                    m_caster->GetTotalAttackPowerValue(BASE_ATTACK) / 100);
            }
            // Swipe
            else if (m_spellInfo->SpellFamilyFlags &
                     UI64LIT(0x0010000000000000))
            {
                damage += int32(
                    m_caster->GetTotalAttackPowerValue(BASE_ATTACK) * 0.08f);
            }
            break;
        }
        case SPELLFAMILY_ROGUE:
        {
            // Envenom
            if (m_caster->GetTypeId() == TYPEID_PLAYER &&
                (m_spellInfo->SpellFamilyFlags & UI64LIT(0x800000000)))
            {
                // consume from stack dozes not more that have combo-points
                if (uint32 combo = ((Player*)m_caster)->GetComboPoints())
                {
                    Aura* poison = nullptr;
                    // Lookup for Deadly poison (only attacker applied)
                    auto& auras =
                        unitTarget->GetAurasByType(SPELL_AURA_PERIODIC_DAMAGE);
                    for (const auto& aura : auras)
                    {
                        if ((aura)->GetSpellProto()->SpellFamilyName ==
                                SPELLFAMILY_ROGUE &&
                            ((aura)->GetSpellProto()->SpellFamilyFlags &
                                UI64LIT(0x10000)) &&
                            (aura)->GetSpellProto()->SpellVisual == 5100 &&
                            (aura)->GetCasterGuid() ==
                                m_caster->GetObjectGuid())
                        {
                            poison = aura;
                            break;
                        }
                    }
                    // count consumed deadly poison doses at target
                    if (poison)
                    {
                        uint32 spellId = poison->GetId();
                        uint32 doses = poison->GetStackAmount();
                        if (doses > combo)
                            doses = combo;

                        if (auto holder = unitTarget->get_aura(
                                spellId, m_caster->GetObjectGuid()))
                            if (holder->ModStackAmount(
                                    -static_cast<int32>(doses)))
                                unitTarget->RemoveAuraHolder(holder);

                        damage *= doses;
                        damage +=
                            int32(((Player*)m_caster)
                                      ->GetTotalAttackPowerValue(BASE_ATTACK) *
                                  0.03f * doses);
                    }
                    else
                        damage = 0;
                    // Eviscerate and Envenom Bonus Damage (item set effect)
                    if (m_caster->has_aura(37169, SPELL_AURA_DUMMY))
                        damage += ((Player*)m_caster)->GetComboPoints() * 40;
                }
            }
            // Eviscerate
            else if ((m_spellInfo->SpellFamilyFlags & UI64LIT(0x00020000)) &&
                     m_caster->GetTypeId() == TYPEID_PLAYER)
            {
                if (uint32 combo = ((Player*)m_caster)->GetComboPoints())
                {
                    damage +=
                        int32(m_caster->GetTotalAttackPowerValue(BASE_ATTACK) *
                              combo * 0.03f);

                    // Eviscerate and Envenom Bonus Damage (item set effect)
                    if (m_caster->has_aura(37169, SPELL_AURA_DUMMY))
                        damage += combo * 40;
                }
            }
            break;
        }
        case SPELLFAMILY_HUNTER:
        {
            // Steady Shot
            if (m_spellInfo->SpellFamilyFlags & UI64LIT(0x100000000))
            {
                int32 base = irand((int32)m_caster->GetWeaponDamageRange(
                                       RANGED_ATTACK, MINDAMAGE),
                    (int32)m_caster->GetWeaponDamageRange(
                        RANGED_ATTACK, MAXDAMAGE));
                damage += int32(float(base) /
                                m_caster->GetAttackTime(RANGED_ATTACK) * 2800);
            }
            break;
        }
        case SPELLFAMILY_PALADIN:
        {
            // Judgement of Vengeance
            if ((m_spellInfo->SpellFamilyFlags & UI64LIT(0x800000000)) &&
                m_spellInfo->SpellIconID == 2292)
            {
                // Get stack of Holy Vengeance on the target added by caster
                uint32 stacks = 0;
                auto& auras =
                    unitTarget->GetAurasByType(SPELL_AURA_PERIODIC_DAMAGE);
                for (const auto& aura : auras)
                {
                    if (((aura)->GetId() == 31803) &&
                        (aura)->GetCasterGuid() == m_caster->GetObjectGuid())
                    {
                        stacks = (aura)->GetStackAmount();
                        break;
                    }
                }
                if (!stacks)
                    // No damage if the target isn't affected by this
                    damage = -1;
                else
                    damage *= stacks;
            }
            break;
        }
        }

        if (damage >= 0)
            m_damage += damage;
    }
}

void Spell::EffectDummy(SpellEffectIndex eff_idx)
{
    if (!unitTarget && !gameObjTarget && !itemTarget)
        return;

    // selection by spell family
    switch (m_spellInfo->SpellFamilyName)
    {
    case SPELLFAMILY_GENERIC:
    {
        switch (m_spellInfo->Id)
        {
        case 3360: // Curse of the Eye
        {
            if (!unitTarget)
                return;

            uint32 spell_id =
                (unitTarget->getGender() == GENDER_MALE) ? 10651 : 10653;

            m_caster->CastSpell(unitTarget, spell_id, true);
            return;
        }
        case 7671: // Transformation (human<->worgen)
        {
            if (!unitTarget)
                return;

            // Transform Visual
            unitTarget->CastSpell(unitTarget, 24085, true);
            return;
        }
        case 8063: // Deviate Fish
        {
            if (m_caster->GetTypeId() != TYPEID_PLAYER)
                return;

            uint32 spell_id = 0;
            switch (urand(1, 5))
            {
            case 1:
                spell_id = 8064;
                break; // Sleepy
            case 2:
                spell_id = 8065;
                break; // Invigorate
            case 3:
                spell_id = 8066;
                break; // Shrink
            case 4:
                spell_id = 8067;
                break; // Party Time!
            case 5:
                spell_id = 8068;
                break; // Healthy Spirit
            }
            m_caster->CastSpell(m_caster, spell_id, true, nullptr);
            return;
        }
        case 8213: // Savory Deviate Delight
        {
            if (m_caster->GetTypeId() != TYPEID_PLAYER)
                return;

            uint32 spell_id = 0;
            switch (urand(1, 2))
            {
            // Flip Out - ninja
            case 1:
                spell_id = (m_caster->getGender() == GENDER_MALE ? 8219 : 8220);
                break;
            // Yaaarrrr - pirate
            case 2:
                spell_id = (m_caster->getGender() == GENDER_MALE ? 8221 : 8222);
                break;
            }

            m_caster->CastSpell(m_caster, spell_id, true, nullptr);
            return;
        }
        case 8344: // Universal Remote
        {
            if (!unitTarget || m_caster->GetTypeId() != TYPEID_PLAYER)
                return;

            if (roll_chance_i(20)) // 20% fail chance. TODO: figure out real
                                   // number, this one is not based on research
            {
                if (urand(0, 1))
                    m_caster->CastSpell(unitTarget, 8346, true);
                else
                    m_caster->CastSpell(unitTarget, 8347, true);
            }
            else
            {
                // Remove GCD for the 8344 spell so that we don't need to
                // trigger the channel (or the buff won't be right clickable)
                static_cast<Player*>(m_caster)
                    ->GetGlobalCooldownMgr()
                    .CancelGlobalCooldown(m_spellInfo);
                m_caster->CastSpell(unitTarget, 8345, false);
            }

            return;
        }
        case 9976: // Polly Eats the E.C.A.C.
        {
            if (!unitTarget || unitTarget->GetTypeId() != TYPEID_UNIT)
                return;

            // Summon Polly Jr.
            unitTarget->CastSpell(unitTarget, 9998, true);

            ((Creature*)unitTarget)->ForcedDespawn(100);
            return;
        }
        case 10254: // Stone Dwarf Awaken Visual
        {
            if (m_caster->GetTypeId() != TYPEID_UNIT)
                return;

            // see spell 10255 (aura dummy)
            m_caster->clearUnitState(UNIT_STAT_ROOT);
            m_caster->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
            return;
        }
        case 12975: // Last Stand
        {
            int32 healthModSpellBasePoints0 =
                int32(m_caster->GetMaxHealth() * 0.3);
            m_caster->CastCustomSpell(m_caster, 12976,
                &healthModSpellBasePoints0, nullptr, nullptr, true, nullptr);
            return;
        }
        case 13006: // Gnomish shrink ray
        {
            if (!unitTarget)
                return;

            // Intended effect & target
            uint32 spell_id = 13003;
            Unit* target = unitTarget;

            // 33% chance to backfire with another effect (TODO: Guessed
            // percentage)
            // NOTE: Wowwiki lists a few more effects, but I've been unable to
            // find those spells
            // (http://www.wowwiki.com/Gnomish_Shrink_Ray?oldid=1546737)
            if (urand(0, 2) < 1)
            {
                switch (urand(0, 3))
                {
                case 0:
                    target = m_caster;
                    break; // Shrinks the caster instead
                case 1:
                    spell_id = 13004;
                    target = m_caster;
                    break; // Grows the caster
                case 2:
                    spell_id = 13010;
                    target = m_caster;
                    break; // Shrinks caster's party
                case 3:
                    spell_id = 13004;
                    break; // Grows the caster's party
                }
            }

            m_caster->CastSpell(target, spell_id, true);
            return;
        }
        case 13180: // Gnomish Mind Control Cap
        {
            if (!unitTarget)
                return;

            // small chance to cause you to become the controlled target (TODO:
            // the chance is made up, fix it)
            if (roll_chance_i(20))
                unitTarget->CastSpell(m_caster, 13181, true);
            else
                m_caster->CastSpell(unitTarget, 13181, true);

            return;
        }
        case 13120: // Gnomish Net-o-Matic Projector
        {
            if (!unitTarget)
                return;

            uint32 roll = urand(0, 99);

            uint32 spell_id;
            if (roll < 10) // 10% for 30 sec self root
                spell_id = 16566;
            else if (roll < 20) // 10% for 20 sec root, charge to target
                spell_id = 13119;
            else // normal root
                spell_id = 13099;

            m_caster->CastSpell(unitTarget, spell_id, true);
            return;
        }
        case 13278: // Gnomish Death Ray (charge up)
        {
            if (!unitTarget)
                return;

            // If you change this formula also ee SpellAuras.cpp,
            // HandleAuraDummy (apply), case 13278
            int32 hp = 150 + m_caster->GetHealth() / 16;
            // seems to be capped out, to not do insane damage in TBC
            if (hp > 400)
                hp = 400;

            int32 damage =
                600 + (4 * hp) * 2; // 4 self-hurt ticks, double the damage

            // Place a death ray marker on the target with the correct damage
            // points
            m_caster->CastCustomSpell(
                unitTarget, 150039, &damage, nullptr, nullptr, true);

            return;
        }
        case 13280: // Gnomish Death Ray (release)
        {
            // Find the target with our death ray marker on it
            auto units = maps::visitors::yield_set<Unit, Player, Creature, Pet,
                SpecialVisCreature, TemporarySummon, Totem>{}(m_caster, 60,
                [](auto&& elem)
                {
                    return elem->isAlive();
                });

            int32 dmg = 0;
            Unit* target = nullptr;
            for (auto unit : units)
            {
                AuraHolder* holder =
                    unit->get_aura(150039, m_caster->GetObjectGuid());
                Aura* aura;
                if (holder &&
                    (aura = holder->GetAura(EFFECT_INDEX_0)) != nullptr)
                {
                    dmg = aura->GetBasePoints();
                    target = unit;
                    break;
                }
            }

            if (!target)
                return;

            // Cast the damage spell on the target
            m_caster->CastCustomSpell(
                target, 13279, &dmg, nullptr, nullptr, true);

            return;
        }
        case 13567: // Dummy Trigger
        {
            // can be used for different aura triggering, so select by aura
            if (!m_triggeredByAuraSpell || !unitTarget)
                return;

            switch (m_triggeredByAuraSpell->Id)
            {
            case 26467: // Persistent Shield
                m_caster->CastCustomSpell(
                    unitTarget, 26470, &damage, nullptr, nullptr, true);
                break;
            default:
                logging.error(
                    "EffectDummy: Non-handled case for spell 13567 for "
                    "triggered aura %u",
                    m_triggeredByAuraSpell->Id);
                break;
            }
            return;
        }
        case 14185: // Preparation Rogue
        {
            if (m_caster->GetTypeId() != TYPEID_PLAYER)
                return;

            // immediately finishes the cooldown on certain Rogue abilities
            const SpellCooldowns& cm =
                ((Player*)m_caster)->GetSpellCooldownMap();
            for (auto itr = cm.begin(); itr != cm.end();)
            {
                SpellEntry const* spellInfo =
                    sSpellStore.LookupEntry(itr->first);

                // Don't reset the cooldown of coldblood (disable while active)
                // if it's currently on
                if (spellInfo->SpellFamilyName == SPELLFAMILY_ROGUE &&
                    (spellInfo->SpellFamilyFlags &
                        UI64LIT(0x0000026000000860)) &&
                    !(spellInfo->HasAttribute(
                          SPELL_ATTR_DISABLED_WHILE_ACTIVE) &&
                        m_caster->has_aura(spellInfo->Id)))
                    ((Player*)m_caster)
                        ->RemoveSpellCooldown((itr++)->first, true);
                else
                    ++itr;
            }
            return;
        }
        case 14537: // Six Demon Bag
        {
            if (!unitTarget)
                return;

            Unit* newTarget = unitTarget;
            uint32 spell_id = 0;
            uint32 roll = urand(0, 99);
            if (roll < 25) // Fireball (25% chance)
                spell_id = 15662;
            else if (roll < 50) // Frostbolt (25% chance)
                spell_id = 11538;
            else if (roll < 70) // Chain Lighting (20% chance)
                spell_id = 21179;
            else if (roll < 77) // Polymorph (10% chance, 7% to target)
                spell_id = 14621;
            else if (roll < 80) // Polymorph (10% chance, 3% to self, backfire)
            {
                spell_id = 14621;
                newTarget = m_caster;
            }
            else if (roll < 95) // Enveloping Winds (15% chance)
                spell_id = 25189;
            else // Summon Felhund minion (5% chance)
            {
                spell_id = 14642;
                newTarget = m_caster;
            }

            m_caster->CastSpell(newTarget, spell_id, true, m_CastItem);
            return;
        }
        case 15998: // Capture Worg Pup
        case 29435: // Capture Female Kaliri Hatchling
        {
            if (!unitTarget || unitTarget->GetTypeId() != TYPEID_UNIT)
                return;

            Creature* creatureTarget = (Creature*)unitTarget;

            creatureTarget->ForcedDespawn();
            return;
        }
        case 16589: // Noggenfogger Elixir
        {
            if (m_caster->GetTypeId() != TYPEID_PLAYER)
                return;

            uint32 spell_id = 0;
            float roll = rand_norm_f();
            // 60% chance for skeleton
            if (roll < 0.6f)
                spell_id = 16591;
            // 20% chance for feather fall
            else if (roll < 0.8f)
                spell_id = 16593;
            // 20% chance for shrink
            else
                spell_id = 16595;

            m_caster->CastSpell(m_caster, spell_id, true, nullptr);
            return;
        }
        case 17009: // Voodoo
        {
            if (!unitTarget)
                return;
            static const std::array<uint32, 7> ids{
                16707, 16708, 16709, 16711, 16712, 16713, 16716};
            m_caster->CastSpell(unitTarget, ids[urand(0, ids.size() - 1)],
                TRIGGER_TYPE_TRIGGERED | TRIGGER_TYPE_BYPASS_SPELL_QUEUE);
            return;
        }
        case 17251: // Spirit Healer Res
        {
            if (!unitTarget)
                return;

            Unit* caster = GetAffectiveCaster();

            if (caster && caster->GetTypeId() == TYPEID_PLAYER)
            {
                WorldPacket data(SMSG_SPIRIT_HEALER_CONFIRM, 8);
                data << unitTarget->GetObjectGuid();
                ((Player*)caster)->GetSession()->send_packet(std::move(data));
            }
            return;
        }
        case 17271: // Test Fetid Skull
        {
            if (!itemTarget && m_caster->GetTypeId() != TYPEID_PLAYER)
                return;

            uint32 spell_id = roll_chance_i(50) ?
                                  17269 // Create Resonating Skull
                                  :
                                  17270; // Create Bone Dust

            m_caster->CastSpell(m_caster, spell_id, true, nullptr);
            return;
        }
        case 17770: // Wolfshead Helm Energy
        {
            m_caster->CastSpell(m_caster, 29940, true, nullptr);
            return;
        }
        case 17950: // Shadow Portal
        {
            if (!unitTarget)
                return;

            // Shadow Portal
            const uint32 spell_list[6] = {
                17863, 17939, 17943, 17944, 17946, 17948};

            m_caster->CastSpell(unitTarget, spell_list[urand(0, 5)], true);
            return;
        }
        case 19028: // Soul Link
        {
            // Works on Enslaved Demons since Patch 2.1
            if (unitTarget && unitTarget->HasAuraType(SPELL_AURA_MOD_CHARM) &&
                unitTarget->GetTypeId() == TYPEID_UNIT &&
                !static_cast<Creature*>(unitTarget)->IsPet())
            {
                unitTarget->CastSpell(unitTarget, 25228, true);
                return;
            }
            break;
        }
        case 20577: // Cannibalize
        {
            if (unitTarget)
                m_caster->CastSpell(m_caster, 20578, false, nullptr);

            return;
        }
        case 21147: // Arcane Vacuum
        {
            if (!unitTarget)
                return;

            // Spell used by Azuregos to teleport all the players to him
            // This also resets the target threat
            if (m_caster->getThreatManager().getThreat(unitTarget))
                m_caster->getThreatManager().modifyThreatPercent(
                    unitTarget, -100);

            // cast summon player
            m_caster->CastSpell(unitTarget, 21150, true);

            return;
        }
        case 21795: // Frostwolf Muzzle
        case 21867: // Alterac Ram Collar
        {
            if (!unitTarget || unitTarget->GetTypeId() != TYPEID_UNIT)
                return;

            m_caster->SummonCreature(
                unitTarget->GetEntry() == 10990 ? 10994 : 10995,
                unitTarget->GetX(), unitTarget->GetY(), unitTarget->GetZ(),
                unitTarget->GetO(), TEMPSUMMON_TIMED_DESPAWN,
                10 * MINUTE * IN_MILLISECONDS);

            static_cast<Creature*>(unitTarget)->ForcedDespawn();

            return;
        }
        case 21908:
        {
            // Lava Burst A-I
            const std::array<uint32, 9> spell_set = {
                21886, 21900, 21901, 21902, 21903, 21904, 21905, 21906, 21907,
            };
            m_caster->CastSpell(
                m_caster, spell_set[urand(0, spell_set.size() - 1)], true);
            return;
        }
        case 22276: // Elemental Shield
        {
            static const int SHIELDS_NUM = 5;
            static const uint32 spells[SHIELDS_NUM] = {
                22277, 22278, 22279, 22280, 22281};
            for (int i = 0; i < SHIELDS_NUM; ++i)
                m_caster->remove_auras(spells[i]);
            m_caster->CastSpell(
                m_caster, spells[urand(0, SHIELDS_NUM - 1)], true);
            return;
        }
        case 22681: // Shadowblink
        {
            static const int POS_NUM = 10;
            static const G3D::Vector3 positions[POS_NUM] = {
                {-7522.7f, -1223.1f, 476.8f}, {-7537.2f, -1279.5f, 476.8f},
                {-7561.7f, -1244.4f, 476.8f}, {-7581.6f, -1217.2f, 476.8f},
                {-7543.1f, -1190.9f, 476.4f}, {-7506.7f, -1165.9f, 476.8f},
                {-7490.5f, -1193.5f, 476.8f}, {-7461.6f, -1226.4f, 476.8f},
                {-7496.6f, -1250.8f, 476.8f}, {-7536.5f, -1278.9f, 476.8f},
            };
            auto pos = positions[urand(0, POS_NUM - 1)];
            m_caster->NearTeleportTo(pos.x, pos.y, pos.z, m_caster->GetO());
            return;
        }
        case 23002: // Alarm-o-Bot false warning
        {
            if (urand(0, 5) == 0)
                m_caster->CastSpell(m_caster, 150038, true);
            return;
        }
        case 23019: // Crystal Prison Dummy DND
        {
            if (!unitTarget || !unitTarget->isAlive() ||
                unitTarget->GetTypeId() != TYPEID_UNIT ||
                ((Creature*)unitTarget)->IsPet())
                return;

            Creature* creatureTarget = (Creature*)unitTarget;
            if (creatureTarget->IsPet())
                return;

            auto pGameObj = new GameObject;

            Map* map = creatureTarget->GetMap();

            // create before death for get proper coordinates
            if (!pGameObj->Create(
                    map->GenerateLocalLowGuid(HIGHGUID_GAMEOBJECT), 179644, map,
                    creatureTarget->GetX(), creatureTarget->GetY(),
                    creatureTarget->GetZ(), creatureTarget->GetO()))
            {
                delete pGameObj;
                return;
            }

            pGameObj->SetRespawnTime(creatureTarget->GetRespawnTime() -
                                     WorldTimer::time_no_syscall());
            pGameObj->SetOwnerGuid(m_caster->GetObjectGuid());
            pGameObj->SetUInt32Value(GAMEOBJECT_LEVEL, m_caster->getLevel());
            pGameObj->SetSpellId(m_spellInfo->Id);
            pGameObj->SetTemporary(true);

            creatureTarget->ForcedDespawn();

            LOG_DEBUG(logging, "AddObject at SpellEfects.cpp EffectDummy");
            if (!map->insert(pGameObj))
            {
                delete pGameObj;
                return;
            }

            WorldPacket data(SMSG_GAMEOBJECT_SPAWN_ANIM_OBSOLETE, 8);
            data << ObjectGuid(pGameObj->GetObjectGuid());
            m_caster->SendMessageToSet(&data, true);

            return;
        }
        case 23074: // Arcanite Dragonling
        {
            if (!m_CastItem)
                return;

            m_caster->CastSpell(m_caster, 19804, true, m_CastItem);
            return;
        }
        case 23075: // Mithril Mechanical Dragonling
        {
            if (!m_CastItem)
                return;

            m_caster->CastSpell(m_caster, 12749, true, m_CastItem);
            return;
        }
        case 23076: // Mechanical Dragonling
        {
            if (!m_CastItem)
                return;

            m_caster->CastSpell(m_caster, 4073, true, m_CastItem);
            return;
        }
        case 23133: // Gnomish Battle Chicken
        {
            if (!m_CastItem)
                return;

            m_caster->CastSpell(m_caster, 13166, true, m_CastItem);
            return;
        }
        case 23134: // Goblin Bomb
        {
            if (!m_CastItem)
                return;

            // 10% chance to malfunction (NOTE: Guessed chance)
            if (urand(0, 9) < 1)
                m_caster->CastSpell(m_caster, 13261, true, m_CastItem);
            else
                m_caster->CastSpell(m_caster, 13258, true, m_CastItem);
            return;
        }
        case 23173: // Brood Affliction
        {
            static const int AFFLICTIONS_NUM = 5;
            static const uint32 spells[AFFLICTIONS_NUM] = {
                23153, 23154, 23155, 23169, 23170};

            auto spell = spells[urand(0, AFFLICTIONS_NUM - 1)];

            // Down prioritize any target that already has the aura
            std::vector<Player*> has_tmp;
            std::vector<Player*> dont_have_tmp;
            maps::visitors::simple<Player>{}(m_caster, 100.0f,
                [this, &has_tmp, &dont_have_tmp, spell](Player* p)
                {
                    if (!p->isGameMaster() && p->isAlive() &&
                        p->IsHostileTo(m_caster))
                    {
                        if (p->has_aura(spell))
                            has_tmp.push_back(p);
                        else
                            dont_have_tmp.push_back(p);
                    }
                });

            std::vector<Player*> set;
            set.reserve(has_tmp.size() + dont_have_tmp.size());
            set.insert(set.end(), dont_have_tmp.begin(), dont_have_tmp.end());
            set.insert(set.end(), has_tmp.begin(), has_tmp.end());
            if (set.empty())
                return;

            int cast_count = 15;
            while (cast_count)
            {
                for (auto itr = set.begin(); itr != set.end() && cast_count;
                     ++itr)
                {
                    --cast_count;
                    m_caster->CastSpell(*itr, spell, true);
                }
            }

            return;
        }
        case 23424: // Corrupted Totems
        {
            switch (urand(0, 3))
            {
            case 0:
                m_caster->CastSpell(m_caster, 23419, true);
                break;
            case 1:
                m_caster->CastSpell(m_caster, 23420, true);
                break;
            case 2:
                m_caster->CastSpell(m_caster, 23422, true);
                break;
            case 3:
                m_caster->CastSpell(m_caster, 23423, true);
                break;
            }
            return;
        }
        case 23448: // Transporter Arrival - Ultrasafe Transporter: Gadgetzan -
                    // backfires
            {
                switch (urand(0, 2))
                {
                case 0: // Transporter Malfunction - polymorph
                    m_caster->CastSpell(m_caster, 23444, true);
                    break;
                case 1: // Evil Twin
                    m_caster->CastSpell(m_caster, 23445, true);
                    break;
                case 2: // Transporter Malfunction - miss the target
                    m_caster->CastSpell(m_caster, 36902, true);
                    break;
                }
                return;
            }
        case 23453: // Gnomish Transporter - Ultrasafe Transporter: Gadgetzan
        {
            if (roll_chance_i(50)) // Gadgetzan Transporter         - success
                m_caster->CastSpell(m_caster, 23441, true);
            else // Gadgetzan Transporter Failure - failure
                m_caster->CastSpell(m_caster, 23446, true);

            return;
        }
        case 23645:                        // Hourglass Sand
            m_caster->remove_auras(23170); // Brood Affliction: Bronze
            return;
        case 23595: // Luffa
        {
            if (!m_caster)
                return;

            auto& periodic =
                m_caster->GetAurasByType(SPELL_AURA_PERIODIC_DAMAGE);
            for (const auto& elem : periodic)
            {
                if ((elem)->HasMechanic(15))
                {
                    Unit* caster = (elem)->GetCaster();

                    // Doesn't work on bleed effects over level 60
                    if (caster && !(caster->getLevel() > 60))
                    {
                        m_caster->remove_auras((elem)->GetId());
                        return;
                    }
                }
            }
            return;
        }
        case 23725: // Gift of Life (warrior bwl trinket)
            m_caster->CastSpell(m_caster, 23782, true);
            m_caster->CastSpell(m_caster, 23783, true);
            return;
        case 24019: // Axe Flurry
        {
            if (m_caster->GetTypeId() != TYPEID_UNIT)
                return;

            auto focus_target = maps::visitors::yield_single<Unit, Player,
                Creature, TemporarySummon, SpecialVisCreature>()(m_caster,
                30.0f, [this](Unit* target)
                {
                    return target->get_aura(
                               150062, m_caster->GetObjectGuid()) != nullptr;
                });

            if (!focus_target || rand_norm_f() < 0.4f)
            {
                if (auto target =
                        static_cast<Creature*>(m_caster)->SelectAttackingTarget(
                            ATTACKING_TARGET_RANDOM, 0, 24020))
                {
                    focus_target = target;
                    m_caster->CastSpell(focus_target, 150062, true);
                }
            }

            if (focus_target)
                m_caster->CastSpell(focus_target, 24020, true);
            return;
        }
        case 24930: // Hallow's End Treat
        {
            uint32 spell_id = 0;

            switch (urand(1, 4))
            {
            case 1:
                spell_id = 24924;
                break; // Larger and Orange
            case 2:
                spell_id = 24925;
                break; // Skeleton
            case 3:
                spell_id = 24926;
                break; // Pirate
            case 4:
                spell_id = 24927;
                break; // Ghost
            }

            m_caster->CastSpell(m_caster, spell_id, true);
            return;
        }
        case 25860: // Reindeer Transformation
        {
            if (!m_caster->HasAuraType(SPELL_AURA_MOUNTED))
                return;

            float flyspeed = m_caster->GetSpeedRate(MOVE_FLIGHT);
            float speed = m_caster->GetSpeedRate(MOVE_RUN);

            m_caster->remove_auras(SPELL_AURA_MOUNTED);

            // 5 different spells used depending on mounted speed and if mount
            // can fly or not
            if (flyspeed >= 4.1f)
                // Flying Reindeer
                m_caster->CastSpell(
                    m_caster, 44827, true); // 310% flying Reindeer
            else if (flyspeed >= 3.8f)
                // Flying Reindeer
                m_caster->CastSpell(
                    m_caster, 44825, true); // 280% flying Reindeer
            else if (flyspeed >= 1.6f)
                // Flying Reindeer
                m_caster->CastSpell(
                    m_caster, 44824, true); // 60% flying Reindeer
            else if (speed >= 2.0f)
                // Reindeer
                m_caster->CastSpell(
                    m_caster, 25859, true); // 100% ground Reindeer
            else
                // Reindeer
                m_caster->CastSpell(
                    m_caster, 25858, true); // 60% ground Reindeer

            return;
        }
        case 26074: // Holiday Cheer
            // implemented at client side
            return;
        case 26593: // Truesilver Boar
        case 31038: // Felsteel Boar
        case 46782: // Khorium Boar
        {
            // Jewelcrafting Pets Scaling (TODO: These values all need a lot
            // more research)
            uint32 pet_entry, start_val;
            int start_lvl;
            float r = 1;
            switch (m_spellInfo->Id)
            {
            case 26593:
                pet_entry = 15935;
                r = 2;
                start_lvl = 42;
                start_val = 200;
                break;
            case 31038:
                pet_entry = 17706;
                start_lvl = 70;
                start_val = 760;
                break;
            case 46782:
                pet_entry = 26238;
                start_lvl = 70;
                start_val = 990;
                break;
            default:
                return;
            }
            if (Pet* pet = m_caster->FindGuardianWithEntry(pet_entry))
            {
                float lvl = static_cast<int>(m_caster->getLevel()) - start_lvl;
                if (lvl <= 0)
                    lvl = 1;

                float val = pow(2, lvl / 10.0f); // double each 10 level
                val *= r;

                pet->SetMaxHealth(start_val + val);
                pet->SetHealth(start_val + val);
                float divide = start_val < 1000 ? 10.0f : 30.0f;
                pet->SetStatFloatValue(
                    UNIT_FIELD_MINDAMAGE, start_val / divide + val / 9.0f);
                pet->SetStatFloatValue(
                    UNIT_FIELD_MAXDAMAGE, start_val / divide + val / 8.0f);
            }
            return;
        }
        case 28006: // Arcane Cloaking
        {
            if (unitTarget && unitTarget->GetTypeId() == TYPEID_PLAYER)
                // Naxxramas Entry Flag Effect DND
                m_caster->CastSpell(unitTarget, 29294, true);

            return;
        }
        case 28730: // Arcane Torrent (Mana)
        {
            if (AuraHolder* holder = m_caster->get_aura(28734))
            {
                int32 bp = damage * holder->GetStackAmount();
                m_caster->CastCustomSpell(
                    m_caster, 28733, &bp, nullptr, nullptr, true);
                m_caster->remove_auras(28734);
            }
            return;
        }
        case 29200: // Purify Helboar Meat
        {
            if (m_caster->GetTypeId() != TYPEID_PLAYER)
                return;

            uint32 spell_id = roll_chance_i(50) ?
                                  29277 // Summon Purified Helboar Meat
                                  :
                                  29278; // Summon Toxic Helboar Meat

            m_caster->CastSpell(m_caster, spell_id, true, nullptr);
            return;
        }
        case 29767: // Overload (Karazhan)
        {
            if (unitTarget)
            {
                AuraHolder* holder = unitTarget->get_aura(29768);
                Aura* aura;
                if (holder &&
                    (aura = holder->GetAura(EFFECT_INDEX_0)) != nullptr)
                {
                    int32 bp0 = 200;
                    for (uint32 i = 1; i < aura->GetAuraTicks(); ++i)
                        bp0 *= 2;
                    unitTarget->CastCustomSpell(
                        unitTarget, 29766, &bp0, nullptr, nullptr, true);
                }
            }
            return;
        }
        case 29858: // Soulshatter
        {
            if (unitTarget && unitTarget->GetTypeId() == TYPEID_UNIT &&
                unitTarget->IsHostileTo(m_caster))
                m_caster->CastSpell(unitTarget, 32835, true);

            return;
        }
        case 29883: // Blink (Karazhan, Arcane Anomaly)
        {
            if (!unitTarget)
                return;
            m_caster->NearTeleportTo(unitTarget->GetX(), unitTarget->GetY(),
                unitTarget->GetZ(), m_caster->GetO());
            // Drop all threat
            for (
                const auto& elem : m_caster->getThreatManager().getThreatList())
            {
                Unit* unit = m_caster->GetMap()->GetUnit((elem)->getUnitGuid());

                if (unit && m_caster->getThreatManager().getThreat(unit))
                    m_caster->getThreatManager().modifyThreatPercent(
                        unit, -100);
            }
            // Give some base threat
            m_caster->getThreatManager().addThreatDirectly(unitTarget, 1000.0f);
            return;
        }
        case 30280: // Distracting Ash (Trigger the real one)
        {
            if (unitTarget)
                unitTarget->AddAuraThroughNewHolder(30130, m_caster);
            return;
        }
        case 30458: // Nigh Invulnerability
        {
            if (!m_CastItem)
                return;

            if (roll_chance_i(86)) // Nigh-Invulnerability   - success
                m_caster->CastSpell(m_caster, 30456, true, m_CastItem);
            else // Complete Vulnerability - backfire in 14% casts
                m_caster->CastSpell(m_caster, 30457, true, m_CastItem);

            return;
        }
        case 30507: // Gnomish Poultryizer
        {
            if (!m_CastItem)
                return;

            if (roll_chance_i(80)) // Poultryized! - success
                m_caster->CastSpell(unitTarget, 30501, true, m_CastItem);
            else // Poultryized! - backfire 20%
                m_caster->CastSpell(unitTarget, 30506, true, m_CastItem);

            return;
        }
        case 30543:
        {
            if (m_caster->GetTypeId() == TYPEID_UNIT)
            {
                static_cast<Creature*>(m_caster)->_AddCreatureSpellCooldown(
                    37153, WorldTimer::time_no_syscall() + 12);
                static_cast<Creature*>(m_caster)->_AddCreatureSpellCooldown(
                    37148, WorldTimer::time_no_syscall() + 12);
                static_cast<Creature*>(m_caster)->_AddCreatureSpellCooldown(
                    37144, WorldTimer::time_no_syscall() + 12);
                static_cast<Creature*>(m_caster)->_AddCreatureSpellCooldown(
                    30284, WorldTimer::time_no_syscall() + 12);
                if (auto charmer = m_caster->GetCharmer())
                    if (charmer->GetTypeId() == TYPEID_PLAYER)
                        static_cast<Player*>(charmer)->PetSpellInitialize();
            }
            return;
        }
        case 30610: // Wrath of the Titans Stacker (Karazhan)
        {
            // Apply Wrath of the Titans
            m_caster->CastSpell(m_caster, 30554, true);
            return;
        }
        case 30822: // Poisoned Thrust (Karazhan)
        {
            if (!unitTarget)
                return;

            unitTarget->AddAuraThroughNewHolder(30822, m_caster);
            return;
        }
        case 31114: // Medivh's Journal
        {
            // Do not summon if nearby Image of Medivh exists
            auto targets = maps::visitors::yield_set<Creature>{}(
                m_caster, 120.0f, maps::checks::entry_guid{17651, 0});
            for (auto& target : targets)
                if ((target)->isAlive())
                    return;
            // Summon:
            m_caster->SummonCreature(17651, -11154.5f, -1891.1f, 91.5f, 2.2f,
                TEMPSUMMON_MANUAL_DESPAWN, 0); // Image of Medivh
            m_caster->SummonCreature(17652, -11201.7f, -1827.9f, 67.8f, 5.4f,
                TEMPSUMMON_MANUAL_DESPAWN, 0); // Image of Arcanagos
            return;
        }
        case 32146: // Liquid Fire
        {
            if (!unitTarget || unitTarget->GetTypeId() != TYPEID_UNIT ||
                m_caster->GetTypeId() != TYPEID_PLAYER)
                return;

            ((Player*)m_caster)
                ->KilledMonsterCredit(
                    unitTarget->GetEntry(), unitTarget->GetObjectGuid());
            ((Creature*)unitTarget)->ForcedDespawn();
            return;
        }
        case 32300: // Focus Fire
        {
            if (!unitTarget)
                return;

            unitTarget->CastSpell(unitTarget,
                unitTarget->GetMap()->IsRegularDifficulty() ? 32302 : 38382,
                true);
            return;
        }
        case 33048: // Wrath of the Astromancer
        {
            if (unitTarget && GetAffectiveCaster())
                m_caster->CastSpell(unitTarget, 33049, true, nullptr, nullptr,
                    GetAffectiveCaster()->GetObjectGuid());
            return;
        }
        case 33060: // Make a Wish
        {
            if (m_caster->GetTypeId() != TYPEID_PLAYER)
                return;

            uint32 spell_id = 0;

            switch (urand(1, 5))
            {
            case 1:
                spell_id = 33053;
                break; // Mr Pinchy's Blessing
            case 2:
                spell_id = 33057;
                break; // Summon Mighty Mr. Pinchy
            case 3:
                spell_id = 33059;
                break; // Summon Furious Mr. Pinchy
            case 4:
                spell_id = 33062;
                break; // Tiny Magical Crawdad
            case 5:
                spell_id = 33064;
                break; // Mr. Pinchy's Gift
            }

            m_caster->CastSpell(m_caster, spell_id, true, nullptr);
            return;
        }
        case 34094: // Power of Arrazius
        {
            if (!unitTarget)
                return;

            uint32 triggered_spell_id = 0;

            // Feeble Weapons: Warrior, Rogue, Hunter
            // Doubting Mind: Paladin,  Shaman, Druid
            // Chilling Words: Mage, Warlock, Priest
            switch (unitTarget->getClass())
            {
            case CLASS_WARRIOR:
            case CLASS_ROGUE:
            case CLASS_HUNTER:
                triggered_spell_id = 34088;
                break;
            case CLASS_PALADIN:
            case CLASS_SHAMAN:
            case CLASS_DRUID:
                triggered_spell_id = 34089;
                break;
            case CLASS_MAGE:
            case CLASS_WARLOCK:
            case CLASS_PRIEST:
                triggered_spell_id = 34087;
                break;
            default:
                return;
            }

            // This spell has lower range than the main spell,
            // and the spell might fail because of that; this is working as
            // intended
            m_caster->CastSpell(unitTarget, triggered_spell_id, true);
            return;
        }
        case 34822: // Arcane Flurry (Normal)
        {
            // Cast Arcane Flurry's ranged component
            if (m_caster->GetTypeId() != TYPEID_UNIT)
                return;
            if (Unit* pTarget =
                    ((Creature*)m_caster)
                        ->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0))
                m_caster->CastSpell(pTarget, 34824, true);
            return;
        }
        case 34830: // Triangulation Point One
        case 34857: // Triangulation Point Two
        {
            if (unitTarget)
                unitTarget->SetFacingTo(
                    unitTarget->GetAngle(m_targets.m_destX, m_targets.m_destY));
            return;
        }
        case 35686: // Electro-Shock
        {
            if (!unitTarget || unitTarget->GetTypeId() != TYPEID_UNIT)
                return;

            uint32 spawnedEntry;
            switch (unitTarget->GetEntry())
            {
            case 20501:
                spawnedEntry = 20806;
                break;
            case 20778:
                spawnedEntry = 20805;
                break;
            default:
                return;
            }

            float x, y, z, o = 0.0f;
            unitTarget->GetPosition(x, y, z);
            for (int i = 0; i < 9; ++i)
            {
                if (Creature* c = unitTarget->SummonCreature(spawnedEntry, x, y,
                        z, o, TEMPSUMMON_MANUAL_DESPAWN, 60000))
                {
                    auto pos = unitTarget->GetPoint(
                        frand(0, 2 * M_PI_F), frand(2.5f, 8.0f));
                    c->movement_gens.push(new movement::PointMovementGenerator(
                        1, pos.x, pos.y, pos.z, true, true));
                }
            }

            unitTarget->MonsterTextEmote(
                "%s breaks down into globules!", nullptr);
            unitTarget->Kill(unitTarget);
            return;
        }
        case 35745: // Socrethar's Stone
        {
            uint32 spell_id;
            switch (m_caster->GetAreaId())
            {
            case 3900:
                spell_id = 35743;
                break; // Socrethar Portal
            case 3742:
                spell_id = 35744;
                break; // Socrethar Portal
            default:
                return;
            }

            m_caster->CastSpell(m_caster, spell_id, true);
            return;
        }
        case 35770: // Felfire Line Up
        {
            if (unitTarget)
            {
                float dist = m_caster->GetDistance(unitTarget);
                auto unit = unitTarget;
                unit->queue_action(100 * dist, [unit]()
                    {
                        unit->CastSpell(unit, 35769, true);
                    });
            }
            return;
        }
        case 36478: // Magic Disruption
        {
            // Remove Kael'thas' Mind Control
            if (unitTarget)
                unitTarget->remove_auras(36797);
            return;
        }
        case 36652: // Tuber Whistle
        {
            // Reaction to movepoint id 100 is implemented in SmartAI of mob
            // with entry 21195.
            // The reaction takes care of spawning a tuber GO and despawning the
            // tuber mound
            if (unitTarget && unitTarget->GetTypeId() == TYPEID_UNIT &&
                unitTarget->GetEntry() == 21195 &&
                unitTarget->movement_gens.top_id() !=
                    movement::gen::point && // Don't affect already "tagged"
                                            // boars
                !unitTarget->isInCombat()) // Don't take mobs fighting something
            {
                unitTarget->movement_gens.push(
                    new movement::PointMovementGenerator(100, m_caster->GetX(),
                        m_caster->GetY(), m_caster->GetZ(), true, true));
            }
            return;
        }
        case 36677: // Chaos Breath
        {
            if (!unitTarget)
                return;

            uint32 spellId = 0;
            switch (urand(1, 8))
            {
            case 1:
                spellId = 36694;
                break; // Corrosive Poison
            case 2:
                spellId = 36695;
                break; // Fevered Fatigue
            case 3:
                spellId = 36700;
                break; // Hex
            case 4:
                spellId = 36693;
                break; // Necrotic Poison
            case 5:
                spellId = 36698;
                break; // Piercing Shadow
            case 6:
                spellId = 36697;
                break; // Shrink
            case 7:
                spellId = 36699;
                break; // Wavering Will
            case 8:
                spellId = 36696;
                break; // Withered Touch
            }
            m_caster->CastSpell(unitTarget, spellId, true);
            return;
        }
        case 37269: // Arcane Flurry (Heroic)
        {
            // Cast Arcane Flurry's ranged component
            if (m_caster->GetTypeId() != TYPEID_UNIT)
                return;
            if (Unit* pTarget =
                    ((Creature*)m_caster)
                        ->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0))
                m_caster->CastSpell(pTarget, 37271, true);
            return;
        }
        case 37545: // Phantom Leotheras
        {
            if (unitTarget)
                unitTarget->SummonCreature(21812, unitTarget->GetX(),
                    unitTarget->GetY(), unitTarget->GetZ(), unitTarget->GetO(),
                    TEMPSUMMON_TIMED_DESPAWN, 3000);
            return;
        }
        case 37573: // Temporal Phase Modulator
        {
            if (!unitTarget)
                return;

            TemporarySummon* summon =
                dynamic_cast<TemporarySummon*>(unitTarget);
            if (!summon)
                return;

            // AE visual (TODO: This might be wrong)
            summon->CastSpell(summon, 35426, true);

            // Delay executing the actual event a second
            summon->queue_action(1000, [summon]()
                {
                    std::vector<uint32> entries{21821, 21820, 21817};
                    entries.erase(std::remove(entries.begin(), entries.end(),
                                      summon->GetEntry()),
                        entries.end());

                    uint32 entry;
                    // 2% chance to get Nihil
                    if (summon->GetEntry() != 21823 && rand_norm_f() < 0.02f)
                        entry = 21823;
                    else
                        entry = entries[urand(0, entries.size() - 1)];

                    Creature* c = summon->SummonCreature(entry, summon->GetX(),
                        summon->GetY(), summon->GetZ(), summon->GetO(),
                        TEMPSUMMON_TIMED_OR_DEAD_DESPAWN, 180000);
                    if (c)
                    {
                        c->SetHealth(summon->GetHealth());
                        if (entry != 21823 && c->AI() && summon->getVictim())
                            c->AI()->AttackStart(summon->getVictim());
                    }

                    static_cast<TemporarySummon*>(summon)->UnSummon();
                });

            return;
        }
        case 37674: // Chaos Blast
        {
            if (unitTarget)
                m_caster->CastSpell(unitTarget, 37675, true);
            return;
        }
        case 39189: // Sha'tari Torch
        {
            if (!unitTarget || unitTarget->GetTypeId() != TYPEID_UNIT ||
                m_caster->GetTypeId() != TYPEID_PLAYER)
                return;

            // Flames
            if (unitTarget->has_aura(39199))
                return;

            unitTarget->CastSpell(unitTarget, 39199, true);
            ((Player*)m_caster)
                ->KilledMonsterCredit(
                    unitTarget->GetEntry(), unitTarget->GetObjectGuid());
            ((Creature*)unitTarget)->ForcedDespawn(10000);
            return;
        }
        case 39246: // Fumping
        {
            // Despawn clefthoof
            if (unitTarget && unitTarget->GetTypeId() == TYPEID_UNIT &&
                unitTarget->GetEntry() == 22105)
                static_cast<Creature*>(unitTarget)->ForcedDespawn();
            return;
        }
        case 39371: // Prayer Beads
        {
            if (unitTarget)
            {
                if (unitTarget->GetEntry() ==
                    22431) // If casted on Barada trigger "Heal Barada"
                    m_caster->CastSpell(unitTarget, 39322, true);
                else if (unitTarget->GetEntry() == 22507) // If casted on
                                                          // Darkness Released
                                                          // trigger "Holy Fire"
                    m_caster->CastSpell(unitTarget, 39323, true);
            }
            return;
        }
        case 40802: // Mingo's Fortune Generator (Mingo's Fortune Giblets)
        {
            // selecting one from Bloodstained Fortune item
            uint32 newitemid;
            switch (urand(1, 20))
            {
            case 1:
                newitemid = 32688;
                break;
            case 2:
                newitemid = 32689;
                break;
            case 3:
                newitemid = 32690;
                break;
            case 4:
                newitemid = 32691;
                break;
            case 5:
                newitemid = 32692;
                break;
            case 6:
                newitemid = 32693;
                break;
            case 7:
                newitemid = 32700;
                break;
            case 8:
                newitemid = 32701;
                break;
            case 9:
                newitemid = 32702;
                break;
            case 10:
                newitemid = 32703;
                break;
            case 11:
                newitemid = 32704;
                break;
            case 12:
                newitemid = 32705;
                break;
            case 13:
                newitemid = 32706;
                break;
            case 14:
                newitemid = 32707;
                break;
            case 15:
                newitemid = 32708;
                break;
            case 16:
                newitemid = 32709;
                break;
            case 17:
                newitemid = 32710;
                break;
            case 18:
                newitemid = 32711;
                break;
            case 19:
                newitemid = 32712;
                break;
            case 20:
                newitemid = 32713;
                break;
            default:
                return;
            }

            DoCreateItem(eff_idx, newitemid);
            return;
        }
        case 40962: // Blade's Edge Terrace Demon Boss Summon Branch
        {
            if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
                return;

            uint32 spell_id = 0;
            switch (urand(1, 4))
            {
            case 1:
                spell_id = 40957;
                break; // Blade's Edge Terrace Demon Boss Summon 1
            case 2:
                spell_id = 40959;
                break; // Blade's Edge Terrace Demon Boss Summon 2
            case 3:
                spell_id = 40960;
                break; // Blade's Edge Terrace Demon Boss Summon 3
            case 4:
                spell_id = 40961;
                break; // Blade's Edge Terrace Demon Boss Summon 4
            }
            unitTarget->CastSpell(unitTarget, spell_id, true);
            return;
        }
        case 42287: // Salvage Wreckage
        {
            if (m_caster->GetTypeId() != TYPEID_PLAYER)
                return;

            if (roll_chance_i(66))
                m_caster->CastSpell(m_caster, 42289, true, m_CastItem);
            else
                m_caster->CastSpell(m_caster, 42288, true);

            return;
        }
        case 43498: // Siphon Soul
        {
            // This spell should cast the next spell only for one
            // (player)target, however it should hit multiple targets, hence
            // this kind of implementation
            if (!unitTarget ||
                m_UniqueTargetInfo.rbegin()->targetGUID !=
                    unitTarget->GetObjectGuid())
                return;

            std::vector<Unit*> possibleTargets;
            possibleTargets.reserve(m_UniqueTargetInfo.size());
            for (TargetList::const_iterator itr = m_UniqueTargetInfo.begin();
                 itr != m_UniqueTargetInfo.end(); ++itr)
            {
                // Skip Non-Players
                if (!itr->targetGUID.IsPlayer())
                    continue;

                if (Unit* target =
                        m_caster->GetMap()->GetPlayer(itr->targetGUID))
                    possibleTargets.push_back(target);
            }

            // Cast Siphon Soul channeling spell
            if (!possibleTargets.empty())
                m_caster->CastSpell(
                    possibleTargets[urand(0, possibleTargets.size() - 1)],
                    43501, false);

            return;
        }
        // Demon Broiled Surprise
        case 43723:
        {
            if (m_caster->GetTypeId() != TYPEID_PLAYER)
                return;

            ((Player*)m_caster)
                ->CastSpell(unitTarget, 43753, true, m_CastItem, nullptr,
                    m_originalCasterGUID, m_spellInfo);
            return;
        }
        case 44845: // Spectral Realm
        {
            if (!unitTarget)
                return;

            // teleport all targets which have the spectral realm aura
            if (unitTarget->has_aura(46021))
            {
                unitTarget->remove_auras(46021);
                unitTarget->CastSpell(unitTarget, 46020, true);
                unitTarget->CastSpell(unitTarget, 44867, true);
            }

            return;
        }
        case 44869: // Spectral Blast
        {
            if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
                return;

            // If target has spectral exhaustion or spectral realm aura return
            if (unitTarget->has_aura(44867) || unitTarget->has_aura(46021))
                return;

            // Cast the spectral realm effect spell, visual spell and spectral
            // blast rift summoning
            unitTarget->CastSpell(unitTarget, 44866, true, nullptr, nullptr,
                m_caster->GetObjectGuid());
            unitTarget->CastSpell(unitTarget, 46648, true, nullptr, nullptr,
                m_caster->GetObjectGuid());
            unitTarget->CastSpell(unitTarget, 44811, true);
            return;
        }
        case 44875: // Complete Raptor Capture
        {
            if (!unitTarget || unitTarget->GetTypeId() != TYPEID_UNIT)
                return;

            Creature* creatureTarget = (Creature*)unitTarget;

            creatureTarget->ForcedDespawn();

            // cast spell Raptor Capture Credit
            m_caster->CastSpell(m_caster, 42337, true, nullptr);
            return;
        }
        case 44997: // Converting Sentry
        {
            // Converted Sentry Credit
            m_caster->CastSpell(m_caster, 45009, true);
            return;
        }
        case 45030: // Impale Emissary
        {
            // Emissary of Hate Credit
            m_caster->CastSpell(m_caster, 45088, true);
            return;
        }
        case 45976: // Open Portal
        case 46177: // Open All Portals
        {
            if (!unitTarget)
                return;

            // portal visual
            unitTarget->CastSpell(unitTarget, 45977, true);

            // break in case additional procressing in scripting library
            // required
            break;
        }
        case 45989: // Summon Void Sentinel Summoner Visual
        {
            if (!unitTarget)
                return;

            // summon void sentinel
            unitTarget->CastSpell(unitTarget, 45988, true);

            return;
        }
        case 49357: // Brewfest Mount Transformation
        {
            if (m_caster->GetTypeId() != TYPEID_PLAYER)
                return;

            if (!m_caster->HasAuraType(SPELL_AURA_MOUNTED))
                return;

            m_caster->remove_auras(SPELL_AURA_MOUNTED);

            // Ram for Alliance, Kodo for Horde
            if (((Player*)m_caster)->GetTeam() == ALLIANCE)
            {
                if (m_caster->GetSpeedRate(MOVE_RUN) >= 2.0f)
                    // 100% Ram
                    m_caster->CastSpell(m_caster, 43900, true);
                else
                    // 60% Ram
                    m_caster->CastSpell(m_caster, 43899, true);
            }
            else
            {
                if (((Player*)m_caster)->GetSpeedRate(MOVE_RUN) >= 2.0f)
                    // 100% Kodo
                    m_caster->CastSpell(m_caster, 49379, true);
                else
                    // 60% Kodo
                    m_caster->CastSpell(m_caster, 49378, true);
            }
            return;
        }
        case 50243: // Teach Language
        {
            if (m_caster->GetTypeId() != TYPEID_PLAYER)
                return;

            // spell has a 1/3 chance to trigger one of the below
            if (roll_chance_i(66))
                return;

            if (((Player*)m_caster)->GetTeam() == ALLIANCE)
            {
                // 1000001 - gnomish binary
                m_caster->CastSpell(m_caster, 50242, true);
            }
            else
            {
                // 01001000 - goblin binary
                m_caster->CastSpell(m_caster, 50246, true);
            }

            return;
        }
        case 51582: // Rocket Boots Engaged (Rocket Boots Xtreme and Rocket
                    // Boots Xtreme Lite)
            {
                if (m_caster->GetTypeId() != TYPEID_PLAYER)
                    return;

                if (BattleGround* bg = ((Player*)m_caster)->GetBattleGround())
                    bg->EventPlayerDroppedFlag((Player*)m_caster);

                // One in four chance to fail (TODO: No confirmed source on this
                // fail ratio)
                if (urand(0, 3) < 1)
                {
                    m_caster->CastSpell(m_caster, 51581, true);
                }
                else
                {
                    m_caster->CastSpell(m_caster, 30452, true);
                }
                return;
            }
        case 52845: // Brewfest Mount Transformation (Faction Swap)
        {
            if (m_caster->GetTypeId() != TYPEID_PLAYER)
                return;

            if (!m_caster->HasAuraType(SPELL_AURA_MOUNTED))
                return;

            m_caster->remove_auras(SPELL_AURA_MOUNTED);

            // Ram for Horde, Kodo for Alliance
            if (((Player*)m_caster)->GetTeam() == HORDE)
            {
                if (m_caster->GetSpeedRate(MOVE_RUN) >= 2.0f)
                    // Swift Brewfest Ram, 100% Ram
                    m_caster->CastSpell(m_caster, 43900, true);
                else
                    // Brewfest Ram, 60% Ram
                    m_caster->CastSpell(m_caster, 43899, true);
            }
            else
            {
                if (((Player*)m_caster)->GetSpeedRate(MOVE_RUN) >= 2.0f)
                    // Great Brewfest Kodo, 100% Kodo
                    m_caster->CastSpell(m_caster, 49379, true);
                else
                    // Brewfest Riding Kodo, 60% Kodo
                    m_caster->CastSpell(m_caster, 49378, true);
            }
            return;
        }
        case 150004: // Fungal Decay Dummy
        {
            if (!unitTarget || unitTarget->has_aura(150004))
                return;

            if (AuraHolder* holder = unitTarget->get_aura(32065))
            {
                int amount = holder->GetStackAmount();
                if (--amount >= 1)
                    holder->SetStackAmount(amount);
            }
            return;
        }
        case 150010: // Death Touch Dispel
        {
            if (unitTarget)
            {
                // Remove Death Count
                unitTarget->remove_auras(36657);
                unitTarget->remove_auras(38818);
            }
            return;
        }
        case 150013: // Skeletal Usher Magnetic Pull (Karazhan)
        {
            if (!unitTarget)
                return;
            m_caster->CastSpell(unitTarget, 29661, true); // Magnetic Pull
            // Queue IceTomb + aggro switch:
            auto caster = m_caster;
            m_caster->queue_action(2000, [caster]()
                {
                    // Ice Tomb
                    if (!caster->getVictim())
                        return;

                    caster->CastSpell(caster->getVictim(), 29670, false);

                    // Swap threat with second on threat
                    const ThreatList& tl =
                        caster->getThreatManager().getThreatList();
                    auto secondThreat = tl.begin();
                    if (tl.size() >= 2)
                    {
                        std::advance(secondThreat, 1);
                        Unit* first = caster->GetMap()->GetUnit(
                            (*tl.begin())->getUnitGuid());
                        Unit* second = caster->GetMap()->GetUnit(
                            (*secondThreat)->getUnitGuid());
                        if (first && second)
                        {
                            float t1 =
                                caster->getThreatManager().getThreat(first);
                            float t2 =
                                caster->getThreatManager().getThreat(second);
                            if (t1 && t2)
                            {
                                caster->getThreatManager().addThreatDirectly(
                                    first,
                                    t2 - t1); // Will result in subtraction
                                              // (unless t1 == t2)
                                caster->getThreatManager().addThreatDirectly(
                                    second, t1 - t2); // Will result in addition
                                                      // (unless t1 == t2)
                            }
                        }
                    }
                });
            return;
        }
        case 150019: // Refreshing Mist Primer
        {
            if (unitTarget)
            {
                float x, y, z;
                unitTarget->GetPosition(x, y, z);
                // Summon Mist Effect
                unitTarget->SummonCreature(
                    22335, x, y, z, 0, TEMPSUMMON_TIMED_DESPAWN, 20000);
                // Summon Blue Mushrooms (5 of them)
                for (int i = 0; i < 5; ++i)
                {
                    auto pos = unitTarget->GetPoint(
                        frand(0, 2 * M_PI_F), frand(1.0f, 4.0f));
                    unitTarget->SummonGameObject(185199, pos.x, pos.y, pos.z,
                        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 20);
                }
            }
            return;
        }
        case 150036: // Target Dummies' Material Return
        {
            if (!unitTarget || unitTarget->GetTypeId() != TYPEID_UNIT)
                return;

            Creature* creature = static_cast<Creature*>(unitTarget);
            if (!creature->GetLootDistributor())
                return;

            if (!unitTarget->GetOwner() ||
                unitTarget->GetOwner()->GetTypeId() != TYPEID_PLAYER)
                return;
            Player* owner = static_cast<Player*>(unitTarget->GetOwner());

            // Their loot table decides the materials the player is able to loot
            // back
            creature->GetLootDistributor()->recipient_mgr()->reset();
            creature->GetLootDistributor()->recipient_mgr()->add_solo_tap(
                owner); // Group looting rules do not apply to this
            return;
        }
        case 150037: // Gnomish Flame Turret Handler
        {
            // Face a random, alive & visible, hostile target within 10 yards
            // range & cast "Gnomish Flame Turret"

            auto targets = maps::visitors::yield_set<Unit, Player, Creature,
                Pet, SpecialVisCreature, TemporarySummon>{}(m_caster, 10.0f,
                [this](Unit* u)
                {
                    return u->isAlive() && m_caster->IsHostileTo(u) &&
                           u->can_be_seen_by(m_caster, m_caster, false);
                });

            if (targets.empty())
                return;

            Unit* target = targets[urand(0, targets.size() - 1)];
            m_caster->SetTargetGuid(target->GetObjectGuid());
            m_caster->SetFacingToObject(target);
            m_caster->CastSpell(m_caster, 43050, true);

            return;
        }
        case 150041: // Fury of the Frostwolf
        case 150042: // Stormpike's Salvation
        {
            auto& pls = m_caster->GetMap()->GetPlayers();
            for (auto& ref : pls)
            {
                Player* pl = ref.getSource();
                if (pl &&
                    pl->GetBGTeam() ==
                        ((m_spellInfo->Id == 150041) ? HORDE : ALLIANCE))
                    pl->CastSpell(pl, m_spellInfo->Id == 150041 ? 22751 : 23693,
                        true, nullptr, nullptr, m_caster->GetObjectGuid());
            }
            return;
        }
        case 150047: // Fire at will! - Summon
        {
            // Summon an unstable fel-imp or void hound
            bool hound = rand_norm_f() < 0.025f; // 2.5% chance for hound
            if (Creature* c = m_caster->SummonCreature(hound ? 22500 : 22474,
                    m_caster->GetX(), m_caster->GetY(), m_caster->GetZ(),
                    m_caster->GetO(), TEMPSUMMON_CORPSE_TIMED_DESPAWN,
                    5 * IN_MILLISECONDS, true, false))
            {
                c->ForcedDespawn(5 * MINUTE * IN_MILLISECONDS);
            }
            return;
        }
        case 150058: // Doomguard Threat
        {
            // Only target those with Doomguard Mark
            maps::visitors::simple<Player>{}(m_caster, 15.0f, [this](Player* p)
                {
                    if (p->has_aura(150057))
                    {
                        m_caster->AddThreat(p);
                        m_caster->SetInCombatWith(p);
                        p->SetInCombatWith(m_caster);
                    }
                });
            return;
        }
        case 150064: // Skeram Earth Shock Check
        {
            if (!unitTarget || (m_caster->CanReachWithMeleeAttack(unitTarget) &&
                                   m_caster->getAttackers().find(unitTarget) !=
                                       m_caster->getAttackers().end()))
                return;
            m_caster->CastSpell(unitTarget, 26194, true);
            return;
        }
        }

        // All IconID Check in there
        switch (m_spellInfo->SpellIconID)
        {
        // Berserking (troll racial traits)
        case 1661:
        {
            if (m_caster->GetMaxHealth() == 0)
                return;

            float missing_perc =
                float(m_caster->GetMaxHealth() - m_caster->GetHealth()) /
                m_caster->GetMaxHealth();
            int32 haste = 10.0f + 33.0f * missing_perc;
            if (haste > 30)
                haste = 30;

            // FIXME: custom spell required this aura state by some unknown
            // reason, we not need remove it anyway
            m_caster->ModifyAuraState(AURA_STATE_BERSERKING, true);
            m_caster->CastCustomSpell(
                m_caster, 26635, &haste, &haste, &haste, true, nullptr);
            return;
        }
        }
        break;
    }
    case SPELLFAMILY_MAGE:
    {
        switch (m_spellInfo->Id)
        {
        case 11189: // Frost Warding
        case 28332:
        {
            if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
                return;

            // increase reflection chance (effect 1) of Frost Ward, removed in
            // aura boosts
            auto mod = new SpellModifier(SPELLMOD_EFFECT2, SPELLMOD_FLAT,
                damage, m_spellInfo->Id, UI64LIT(0x0000000000000100));
            ((Player*)unitTarget)->AddSpellMod(&mod, true);
            break;
        }
        case 11094: // Molten Shields
        case 13043:
        {
            if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
                return;

            // increase reflection chance (effect 1) of Fire Ward, removed in
            // aura boosts
            auto mod = new SpellModifier(SPELLMOD_EFFECT2, SPELLMOD_FLAT,
                damage, m_spellInfo->Id, UI64LIT(0x0000000000000008));
            ((Player*)unitTarget)->AddSpellMod(&mod, true);
            break;
        }
        case 11958: // Cold Snap
        {
            if (m_caster->GetTypeId() != TYPEID_PLAYER)
                return;

            std::vector<uint32> cooldowns;

            // immediately finishes the cooldown on Frost spells
            const SpellCooldowns& cm =
                ((Player*)m_caster)->GetSpellCooldownMap();
            for (const auto& elem : cm)
            {
                SpellEntry const* spellInfo =
                    sSpellStore.LookupEntry(elem.first);

                if (spellInfo &&
                    spellInfo->SpellFamilyName == SPELLFAMILY_MAGE &&
                    (GetSpellSchoolMask(spellInfo) & SPELL_SCHOOL_MASK_FROST) &&
                    spellInfo->Id != 11958 &&
                    GetSpellRecoveryTime(spellInfo) > 0)
                    cooldowns.push_back(elem.first);
            }

            for (auto id : cooldowns)
                static_cast<Player*>(m_caster)->RemoveSpellCooldown(id, true);

            return;
        }
        case 32826: // Polymorph Cast Visual
        {
            if (unitTarget && unitTarget->GetTypeId() == TYPEID_UNIT)
            {
                // Polymorph Cast Visual Rank 1
                const uint32 spell_list[6] = {
                    32813, // Squirrel Form
                    32816, // Giraffe Form
                    32817, // Serpent Form
                    32818, // Dragonhawk Form
                    32819, // Worgen Form
                    32820  // Sheep Form
                };
                unitTarget->CastSpell(
                    unitTarget, spell_list[urand(0, 5)], true);
            }
            return;
        }
        case 38194: // Blink
        {
            // Blink
            if (unitTarget)
                m_caster->CastSpell(unitTarget, 38203, true);

            return;
        }
        }
        break;
    }
    case SPELLFAMILY_WARRIOR:
    {
        // Charge
        if ((m_spellInfo->SpellFamilyFlags & UI64LIT(0x1)) &&
            m_spellInfo->SpellVisual == 867)
        {
            int32 chargeBasePoints0 = damage;
            m_caster->CastCustomSpell(
                m_caster, 34846, &chargeBasePoints0, nullptr, nullptr, true);
            return;
        }
        // Execute
        if (m_spellInfo->SpellFamilyFlags & UI64LIT(0x20000000))
        {
            if (!unitTarget)
                return;

            int32 basePoints0 =
                damage + int32(m_caster->GetPower(POWER_RAGE) *
                               m_spellInfo->DmgMultiplier[eff_idx]);
            m_caster->CastCustomSpell(unitTarget, 20647, &basePoints0, nullptr,
                nullptr,
                TRIGGER_TYPE_TRIGGERED | TRIGGER_TYPE_SHOW_IN_COMBAT_LOG |
                    TRIGGER_TYPE_BYPASS_SPELL_QUEUE);
            m_caster->SetPower(POWER_RAGE, 0);
            return;
        }
        // Warrior's Wrath
        if (m_spellInfo->Id == 21977)
        {
            if (!unitTarget)
                return;

            m_caster->CastSpell(unitTarget, 21887, true); // spell mod
            return;
        }

        /* Karazhan Spells. They're classified as SPELLFAMILY_WARRIOR, which is
         * kind of odd, but... */
        // Chess Piece: Move
        if (m_spellInfo->Id == 37153 || m_spellInfo->Id == 37148 ||
            m_spellInfo->Id == 37144)
        {
            if (!unitTarget)
                return;
            // 80001 == Chess Empty Space
            // 80004 == Chess Move Visual
            float spelldist = (m_spellInfo->Id == 37153) ?
                                  8.0f :
                                  (m_spellInfo->Id == 37148) ?
                                  20.0f :
                                  (m_spellInfo->Id == 37144) ? 15.0f : 0.0f;
            if (unitTarget->GetTypeId() == TYPEID_UNIT &&
                unitTarget->GetEntry() == 80001 &&
                m_caster->GetDistance(unitTarget->GetX(), unitTarget->GetY(),
                    unitTarget->GetZ()) < spelldist)
            {
                m_caster->SummonCreature(80004, unitTarget->GetX(),
                    unitTarget->GetY(), unitTarget->GetZ(), unitTarget->GetO(),
                    TEMPSUMMON_TIMED_DESPAWN, 4000);
                m_caster->SummonCreature(80001, m_caster->GetX(),
                    m_caster->GetY(), m_caster->GetZ(), m_caster->GetO(),
                    TEMPSUMMON_MANUAL_DESPAWN, 0);

                m_caster->movement_gens.push(
                    new movement::StoppedMovementGenerator(1000), 0,
                    movement::get_default_priority(movement::gen::point) + 1);

                switch (m_caster->GetEntry())
                {
                case 21682:
                    m_caster->HandleEmote(EMOTE_ONESHOT_BOW);
                    break;
                case 21683:
                case 21684:
                case 21750:
                    m_caster->HandleEmote(EMOTE_ONESHOT_YES);
                    break;
                case 17211:
                case 17469:
                    m_caster->HandleEmote(EMOTE_ONESHOT_SALUTE);
                    break;
                case 21752:
                    m_caster->HandleEmote(EMOTE_ONESHOT_ROAR);
                    break;
                }

                auto angle = m_caster->GetAngle(unitTarget);

                m_caster->movement_gens.push(
                    new movement::PointMovementGenerator(0, unitTarget->GetX(),
                        unitTarget->GetY(), unitTarget->GetZ(), false, false));

                // Update X,Y,Z,O of their idle
                if (auto idle = dynamic_cast<movement::IdleMovementGenerator*>(
                        m_caster->movement_gens.get(movement::gen::idle)))
                {
                    idle->x_ = unitTarget->GetX();
                    idle->y_ = unitTarget->GetY();
                    idle->z_ = unitTarget->GetZ();

                    if (std::abs(angle - 0.626f) - 0.01f <= M_PI_F / 16)
                        angle = 0.626f;
                    else if (std::abs(angle - 2.197f) - 0.01f <= M_PI_F / 16)
                        angle = 2.197f;
                    else if (std::abs(angle - 3.768f) - 0.01f <= M_PI_F / 16)
                        angle = 3.768f;
                    else if (std::abs(angle - 5.339f) - 0.01f <= M_PI_F / 16)
                        angle = 5.339f;
                    else
                        angle = idle->o_;

                    idle->o_ = angle;
                }

                ((Creature*)unitTarget)->ForcedDespawn();
                m_caster->CastSpell(
                    m_caster, 30543, true); // Chess Cooldown: Move
            }
            return;
        }

        // Chess Piece: Change Facing
        if (m_spellInfo->Id == 30284)
        {
            if (!(m_caster->hasUnitState(UNIT_STAT_ROAMING_MOVE) ||
                    m_caster->hasUnitState(UNIT_STAT_ROAMING)))
            {
                auto angle =
                    m_caster->GetAngle(m_targets.m_destX, m_targets.m_destY);

                // North on the board is an angle of 0.626, pieces will revert
                // back to looking north, east, south or west as seen from the
                // board.
                float persistent_angle = 0;
                if (std::abs(angle - 0.626f) - 0.01f <= M_PI_F / 4)
                    persistent_angle = 0.626f;
                else if (std::abs(angle - 2.197f) - 0.01f <= M_PI_F / 4)
                    persistent_angle = 2.197f;
                else if (std::abs(angle - 3.768f) - 0.01f <= M_PI_F / 4)
                    persistent_angle = 3.768f;
                else
                    persistent_angle = 5.339f;

                m_caster->movement_gens.push(
                    new movement::StoppedMovementGenerator(500));
                m_caster->SetFacingTo(angle);
                // Update O of their idle
                if (auto idle = dynamic_cast<movement::IdleMovementGenerator*>(
                        m_caster->movement_gens.get(movement::gen::idle)))
                {
                    idle->o_ = persistent_angle;
                }
            }
            return;
        }
        break;
    }
    case SPELLFAMILY_WARLOCK:
    {
        // Life Tap
        if (m_spellInfo->SpellFamilyFlags & UI64LIT(0x0000000000040000))
        {
            float cost = m_currentBasePoints[EFFECT_INDEX_0];

            if (Player* modOwner = m_caster->GetSpellModOwner())
                modOwner->ApplySpellMod(
                    m_spellInfo->Id, SPELLMOD_COST, cost, this);

            int32 dmg = m_caster->SpellDamageBonusDone(m_caster, m_spellInfo,
                uint32(cost > 0 ? cost : 0), SPELL_DIRECT_DAMAGE);
            dmg = m_caster->SpellDamageBonusTaken(
                m_caster, m_spellInfo, dmg, SPELL_DIRECT_DAMAGE);

            if (int32(m_caster->GetHealth()) > dmg)
            {
                // Shouldn't Appear in Combat Log
                m_caster->ModifyHealth(-dmg);

                int32 mana = dmg;

                // Improved Life Tap mod
                auto& auraDummy = m_caster->GetAurasByType(SPELL_AURA_DUMMY);
                for (const auto& elem : auraDummy)
                    if ((elem)->GetSpellProto()->SpellFamilyName ==
                            SPELLFAMILY_WARLOCK &&
                        (elem)->GetSpellProto()->SpellIconID == 208)
                        mana = ((elem)->GetModifier()->m_amount + 100) * mana /
                               100;

                m_caster->CastCustomSpell(
                    m_caster, 31818, &mana, nullptr, nullptr, true);

                // Mana Feed
                int32 manaFeedVal = m_caster->CalculateSpellDamage(
                    m_caster, m_spellInfo, EFFECT_INDEX_1);
                manaFeedVal = manaFeedVal * mana / 100;
                if (manaFeedVal > 0)
                    m_caster->CastCustomSpell(m_caster, 32553, &manaFeedVal,
                        nullptr, nullptr, true, nullptr);
            }
            else
                SendCastResult(SPELL_FAILED_FIZZLE);

            return;
        }
        break;
    }
    case SPELLFAMILY_PRIEST:
    {
        switch (m_spellInfo->Id)
        {
        case 28598: // Touch of Weakness triggered spell
        {
            if (!unitTarget || !m_triggeredByAuraSpell)
                return;

            uint32 spellid = 0;
            switch (m_triggeredByAuraSpell->Id)
            {
            case 2652:
                spellid = 2943;
                break; // Rank 1
            case 19261:
                spellid = 19249;
                break; // Rank 2
            case 19262:
                spellid = 19251;
                break; // Rank 3
            case 19264:
                spellid = 19252;
                break; // Rank 4
            case 19265:
                spellid = 19253;
                break; // Rank 5
            case 19266:
                spellid = 19254;
                break; // Rank 6
            case 25461:
                spellid = 25460;
                break; // Rank 7
            default:
                logging.error(
                    "Spell::EffectDummy: Spell 28598 triggered by unhandeled "
                    "spell %u",
                    m_triggeredByAuraSpell->Id);
                return;
            }
            m_caster->CastSpell(unitTarget, spellid, true, nullptr);
            return;
        }
        case 34222: // Sunseeker Blessing (Botanica)
        {
            // Casts a buff (34173) on "minions" within ~10 yards
            std::vector<Creature*> target_list;
            // Get 19511 mobs
            {
                target_list = maps::visitors::yield_set<Creature>{}(
                    m_caster, 10.0f, maps::checks::entry_guid{19511, 0});
            }
            // Cast the spell
            for (auto& elem : target_list)
                (elem)->AddAuraThroughNewHolder(34173, m_caster);
            // Get 19512 mobs
            {
                target_list = maps::visitors::yield_set<Creature>{}(
                    m_caster, 10.0f, maps::checks::entry_guid{19512, 0});
            }
            // Cast the spell
            for (auto& elem : target_list)
                (elem)->AddAuraThroughNewHolder(34173, m_caster);
            return;
        }
        }
        break;
    }
    case SPELLFAMILY_DRUID:
    {
        switch (m_spellInfo->Id)
        {
        case 5420: // Tree of Life passive
        {
            // Tree of Life area effect
            int32 health_mod = int32(m_caster->GetStat(STAT_SPIRIT) / 4);
            if (m_caster->has_aura(39926)) // Idol of the Raven Goddess Bonus
                health_mod += 44;
            m_caster->CastCustomSpell(
                m_caster, 34123, &health_mod, nullptr, nullptr, true, nullptr);
            return;
        }
        }
        break;
    }
    case SPELLFAMILY_ROGUE:
    {
        switch (m_spellInfo->Id)
        {
        case 5938: // Shiv
        {
            if (m_caster->GetTypeId() != TYPEID_PLAYER)
                return;

            Player* pCaster = ((Player*)m_caster);

            Item* item = pCaster->GetWeaponForAttack(OFF_ATTACK);
            if (!item)
                return;

            // all poison enchantments is temporary
            uint32 enchant_id = item->GetEnchantmentId(TEMP_ENCHANTMENT_SLOT);
            if (enchant_id)
            {
                SpellItemEnchantmentEntry const* pEnchant =
                    sSpellItemEnchantmentStore.LookupEntry(enchant_id);
                if (!pEnchant)
                    return;

                for (int s = 0; s < 3; ++s)
                {
                    if (pEnchant->type[s] != ITEM_ENCHANTMENT_TYPE_COMBAT_SPELL)
                        continue;

                    SpellEntry const* combatEntry =
                        sSpellStore.LookupEntry(pEnchant->spellid[s]);
                    if (!combatEntry || combatEntry->Dispel != DISPEL_POISON)
                        continue;

                    m_caster->CastSpell(unitTarget, combatEntry,
                        TRIGGER_TYPE_TRIGGERED |
                            TRIGGER_TYPE_BYPASS_SPELL_QUEUE,
                        item);
                }
            }

            // Shiv still does its damagin attack even if we have no poison
            m_caster->CastSpell(unitTarget, 5940,
                TRIGGER_TYPE_TRIGGERED | TRIGGER_TYPE_BYPASS_SPELL_QUEUE);
            return;
        }
        case 31231: // Cheat Death
        {
            // Cheating Death
            m_caster->CastSpell(m_caster, 45182, true);
            return;
        }
        }
        break;
    }
    case SPELLFAMILY_HUNTER:
    {
        // Steady Shot
        if (m_spellInfo->SpellFamilyFlags & UI64LIT(0x100000000))
        {
            if (!unitTarget || !unitTarget->isAlive())
                return;

            bool found = false;

            // check dazed affect
            auto& decSpeedList =
                unitTarget->GetAurasByType(SPELL_AURA_MOD_DECREASE_SPEED);
            for (const auto& elem : decSpeedList)
            {
                if ((elem)->GetSpellProto()->SpellIconID == 15 &&
                    (elem)->GetSpellProto()->Dispel == 0)
                {
                    found = true;
                    break;
                }
            }

            if (found)
                m_damage += damage;
            return;
        }
        // Kill command
        if (m_spellInfo->SpellFamilyFlags & UI64LIT(0x00080000000000))
        {
            if (m_caster->getClass() != CLASS_HUNTER)
                return;

            // clear hunter crit aura state
            m_caster->ModifyAuraState(AURA_STATE_HUNTER_CRIT_STRIKE, false);

            // additional damage from pet to pet target
            Pet* pet = m_caster->GetPet();
            if (!pet || !pet->getVictim())
                return;

            uint32 spell_id = 0;
            switch (m_spellInfo->Id)
            {
            case 34026:
                spell_id = 34027;
                break; // rank 1
            default:
                logging.error("Spell::EffectDummy: Spell %u not handled in KC",
                    m_spellInfo->Id);
                return;
            }

            pet->CastSpell(pet->getVictim(), spell_id, true);
            return;
        }

        switch (m_spellInfo->Id)
        {
        case 23989: // Readiness talent
        {
            if (m_caster->GetTypeId() != TYPEID_PLAYER)
                return;

            // immediately finishes the cooldown for hunter abilities
            const SpellCooldowns& cm =
                ((Player*)m_caster)->GetSpellCooldownMap();
            for (auto itr = cm.begin(); itr != cm.end();)
            {
                SpellEntry const* spellInfo =
                    sSpellStore.LookupEntry(itr->first);

                if (spellInfo->SpellFamilyName == SPELLFAMILY_HUNTER &&
                    spellInfo->Id != 23989 &&
                    GetSpellRecoveryTime(spellInfo) > 0)
                    ((Player*)m_caster)
                        ->RemoveSpellCooldown((itr++)->first, true);
                else
                    ++itr;
            }
            return;
        }
        case 37506: // Scatter Shot
        {
            if (m_caster->GetTypeId() != TYPEID_PLAYER)
                return;

            // break Auto Shot and autohit
            m_caster->InterruptSpell(CURRENT_AUTOREPEAT_SPELL);
            m_caster->AttackStop();
            ((Player*)m_caster)->SendAttackSwingCancelAttack();
            return;
        }
        }
        break;
    }
    case SPELLFAMILY_PALADIN:
    {
        switch (m_spellInfo->SpellIconID)
        {
        case 156: // Holy Shock
        {
            if (!unitTarget)
                return;

            int hurt = 0;
            int heal = 0;

            switch (m_spellInfo->Id)
            {
            case 20473:
                hurt = 25912;
                heal = 25914;
                break;
            case 20929:
                hurt = 25911;
                heal = 25913;
                break;
            case 20930:
                hurt = 25902;
                heal = 25903;
                break;
            case 27174:
                hurt = 27176;
                heal = 27175;
                break;
            case 33072:
                hurt = 33073;
                heal = 33074;
                break;
            default:
                logging.error("Spell::EffectDummy: Spell %u not handled in HS",
                    m_spellInfo->Id);
                return;
            }

            if (m_caster->IsFriendlyTo(unitTarget))
                m_caster->CastSpell(unitTarget, heal, true);
            else
                m_caster->CastSpell(unitTarget, hurt, true);

            return;
        }
        case 561: // Judgement of command
        {
            if (!unitTarget)
                return;

            uint32 spell_id = m_currentBasePoints[eff_idx];
            SpellEntry const* spell_proto = sSpellStore.LookupEntry(spell_id);
            if (!spell_proto)
                return;

            if (!unitTarget->hasUnitState(UNIT_STAT_STUNNED) &&
                m_caster->GetTypeId() == TYPEID_PLAYER)
            {
                // decreased damage (/2) for non-stunned target.
                auto mod = new SpellModifier(SPELLMOD_EFFECT1, SPELLMOD_PCT,
                    -50, m_spellInfo->Id, UI64LIT(0x0000020000000000));

                ((Player*)m_caster)->AddSpellMod(&mod, true);
                m_caster->CastSpell(unitTarget, spell_proto,
                    TRIGGER_TYPE_TRIGGERED | TRIGGER_TYPE_TRIGGER_PROCS |
                        TRIGGER_TYPE_BYPASS_SPELL_QUEUE,
                    nullptr);
                // mod deleted
                ((Player*)m_caster)->AddSpellMod(&mod, false);
            }
            else
                m_caster->CastSpell(unitTarget, spell_proto,
                    TRIGGER_TYPE_TRIGGERED | TRIGGER_TYPE_TRIGGER_PROCS |
                        TRIGGER_TYPE_BYPASS_SPELL_QUEUE,
                    nullptr);

            return;
        }
        }

        switch (m_spellInfo->Id)
        {
        case 31789: // Righteous Defense
        {
            if (m_caster->GetTypeId() != TYPEID_PLAYER || !unitTarget)
                return;

            std::vector<Creature*> attackers;
            for (const auto& elem : unitTarget->getAttackers())
            {
                if ((elem)->GetTypeId() == TYPEID_PLAYER ||
                    !static_cast<Creature*>(elem)->CanStartAttacking(m_caster))
                    continue;
                attackers.push_back(static_cast<Creature*>(elem));
            }

            std::size_t max_targets =
                std::min<std::size_t>(3, attackers.size());
            for (std::size_t i = 0; i < max_targets; ++i)
            {
                int index = urand(0, attackers.size() - 1);

                Creature* target = attackers[index];
                m_caster->CastSpell(target, 31790, true);

                attackers.erase(attackers.begin() + index);
            }

            return;
        }
        case 37877: // Blessing of Faith
        {
            if (!unitTarget)
                return;

            uint32 spell_id = 0;
            switch (unitTarget->getClass())
            {
            case CLASS_DRUID:
                spell_id = 37878;
                break;
            case CLASS_PALADIN:
                spell_id = 37879;
                break;
            case CLASS_PRIEST:
                spell_id = 37880;
                break;
            case CLASS_SHAMAN:
                spell_id = 37881;
                break;
            default:
                return; // ignore for not healing classes
            }

            m_caster->CastSpell(m_caster, spell_id, true);
            return;
        }
        }
        break;
    }
    case SPELLFAMILY_SHAMAN:
    {
        // Rockbiter Weapon
        if (m_spellInfo->SpellFamilyFlags & UI64LIT(0x400000))
        {
            uint32 spell_id = 0;
            switch (m_spellInfo->Id)
            {
            case 8017:
                spell_id = 36494;
                break; // Rank 1
            case 8018:
                spell_id = 36750;
                break; // Rank 2
            case 8019:
                spell_id = 36755;
                break; // Rank 3
            case 10399:
                spell_id = 36759;
                break; // Rank 4
            case 16314:
                spell_id = 36763;
                break; // Rank 5
            case 16315:
                spell_id = 36766;
                break; // Rank 6
            case 16316:
                spell_id = 36771;
                break; // Rank 7
            case 25479:
                spell_id = 36775;
                break; // Rank 8
            case 25485:
                spell_id = 36499;
                break; // Rank 9
            default:
                logging.error("Spell::EffectDummy: Spell %u not handled in RW",
                    m_spellInfo->Id);
                return;
            }

            SpellEntry const* spellInfo = sSpellStore.LookupEntry(spell_id);

            if (!spellInfo)
            {
                logging.error("WORLD: unknown spell id %i", spell_id);
                return;
            }

            if (m_caster->GetTypeId() != TYPEID_PLAYER)
                return;

            for (int j = BASE_ATTACK; j <= OFF_ATTACK; ++j)
            {
                if (Item* item = ((Player*)m_caster)
                                     ->GetWeaponForAttack(WeaponAttackType(j)))
                {
                    if (item->IsFitToSpellRequirements(m_spellInfo))
                    {
                        auto spell = new Spell(m_caster, spellInfo, true);

                        // enchanting spell selected by calculated
                        // damage-per-sec in enchanting effect
                        // at calculation applied affect from Elemental Weapons
                        // talent
                        // real enchantment damage
                        spell->m_currentBasePoints[1] = damage;

                        SpellCastTargets targets;
                        targets.setItemTarget(item);
                        spell->prepare(&targets);
                    }
                }
            }
            return;
        }
        // Flametongue Weapon Proc, Ranks
        if (m_spellInfo->SpellFamilyFlags & UI64LIT(0x0000000000200000))
        {
            if (!m_CastItem)
            {
                logging.error("Spell::EffectDummy: spell %i requires cast Item",
                    m_spellInfo->Id);
                return;
            }

            int32 spell_damage =
                m_caster->SpellBaseDamageBonusDone(
                    GetSpellSchoolMask(m_spellInfo), m_spellInfo->Id) +
                unitTarget->SpellBaseDamageBonusTaken(
                    GetSpellSchoolMask(m_spellInfo));

            // TODO: The weaponspeed is, although not confirmed, probably not
            // normalized for this attack (based on the tooltip explicitly
            // stating weapon speed)
            float weapon_speed =
                (1.0f / IN_MILLISECONDS) * m_CastItem->GetProto()->Delay;

            // Factor is based on the tooltip of flametongue weapon
            float speed_factor;
            switch (m_spellInfo->Id)
            {
            case 8026:
                speed_factor = 3.0f;
                break;
            case 8028:
                speed_factor = 5.0f;
                break;
            case 8029:
                speed_factor = 8.0f;
                break;
            case 10445:
                speed_factor = 12.0f;
                break;
            case 16343:
                speed_factor = 21.0f;
                break;
            case 16344:
                speed_factor = 28.0f;
                break;
            case 25488:
                speed_factor = 35.0f;
                break;
            default:
                logging.error(
                    "Spell::EffectDummy: Unrecognized rank for Flametongue "
                    "Weapon Proc (Id: %d)",
                    m_spellInfo->Id);
                return;
            }

            int32 total_damage =
                spell_damage * 0.1f + speed_factor * weapon_speed;

            m_caster->CastCustomSpell(unitTarget, 10444, &total_damage, nullptr,
                nullptr, true, m_CastItem);
            return;
        }
        // Flametongue Totem Proc
        if (m_spellInfo->SpellFamilyFlags & UI64LIT(0x0000000400000000))
        {
            if (!m_CastItem)
            {
                logging.error("Spell::EffectDummy: spell %i requires cast Item",
                    m_spellInfo->Id);
                return;
            }
            int32 spellDamage = m_caster->SpellBaseDamageBonusDone(
                GetSpellSchoolMask(m_spellInfo), m_spellInfo->Id);
            float weaponSpeed =
                (1.0f / IN_MILLISECONDS) * m_CastItem->GetProto()->Delay;
            int32 totalDamage =
                int32((damage + 3.85f * spellDamage) * 0.01 * weaponSpeed);

            m_caster->CastCustomSpell(unitTarget, 10444, &totalDamage, nullptr,
                nullptr, true, m_CastItem);
            break;
        }

        if (m_spellInfo->Id == 39610) // Mana Tide Totem effect
        {
            if (!unitTarget || unitTarget->getPowerType() != POWER_MANA)
                return;

            // Regenerate 6% of Total Mana Every 3 secs
            int32 EffectBasePoints0 =
                unitTarget->GetMaxPower(POWER_MANA) * damage / 100;
            m_caster->CastCustomSpell(unitTarget, 39609, &EffectBasePoints0,
                nullptr, nullptr, true, nullptr, nullptr, m_originalCasterGUID);
            return;
        }

        break;
    }
    }

    // pet auras
    if (PetAura const* petSpell =
            sSpellMgr::Instance()->GetPetAura(m_spellInfo->Id))
    {
        m_caster->AddPetAura(petSpell);
        return;
    }

    // Script based implementation. Must be used only for not good for
    // implementation in core spell effects
    // So called only for not processed cases
    if (gameObjTarget)
        sScriptMgr::Instance()->OnEffectDummy(
            m_caster, m_spellInfo->Id, eff_idx, gameObjTarget);
    else if (unitTarget && unitTarget->GetTypeId() == TYPEID_UNIT)
        sScriptMgr::Instance()->OnEffectDummy(
            m_caster, m_spellInfo->Id, eff_idx, (Creature*)unitTarget);
    else if (itemTarget)
        sScriptMgr::Instance()->OnEffectDummy(
            m_caster, m_spellInfo->Id, eff_idx, itemTarget);
}

void Spell::EffectTriggerSpellWithValue(SpellEffectIndex eff_idx)
{
    uint32 triggered_spell_id = m_spellInfo->EffectTriggerSpell[eff_idx];

    // normal case
    SpellEntry const* spellInfo = sSpellStore.LookupEntry(triggered_spell_id);

    if (!spellInfo)
    {
        logging.error(
            "EffectTriggerSpellWithValue of spell %u: triggering unknown spell "
            "id %i",
            m_spellInfo->Id, triggered_spell_id);
        return;
    }

    int32 bp = damage;
    m_caster->CastCustomSpell(unitTarget, triggered_spell_id, &bp, &bp, &bp,
        TRIGGER_TYPE_TRIGGERED | TRIGGER_TYPE_IGNORE_CD, nullptr, nullptr,
        m_originalCasterGUID, m_spellInfo);
}

void Spell::EffectTriggerRitualOfSummoning(SpellEffectIndex eff_idx)
{
    uint32 triggered_spell_id = m_spellInfo->EffectTriggerSpell[eff_idx];
    SpellEntry const* spellInfo = sSpellStore.LookupEntry(triggered_spell_id);

    if (!spellInfo)
    {
        logging.error(
            "EffectTriggerRitualOfSummoning of spell %u: triggering unknown "
            "spell id %i",
            m_spellInfo->Id, triggered_spell_id);
        return;
    }

    finish();

    m_caster->CastSpell(unitTarget, spellInfo, false);
}

void Spell::EffectForceCast(SpellEffectIndex eff_idx)
{
    if (!unitTarget)
        return;

    uint32 triggered_spell_id = m_spellInfo->EffectTriggerSpell[eff_idx];

    // normal case
    SpellEntry const* spellInfo = sSpellStore.LookupEntry(triggered_spell_id);

    if (!spellInfo)
    {
        logging.error(
            "EffectForceCast of spell %u: triggering unknown spell id %i",
            m_spellInfo->Id, triggered_spell_id);
        return;
    }

    unitTarget->CastSpell(
        unitTarget, spellInfo, true, nullptr, nullptr, m_originalCasterGUID);
}

void Spell::EffectTriggerSpell(SpellEffectIndex effIndex)
{
    // only unit case known
    if (!unitTarget)
    {
        if (gameObjTarget || itemTarget)
            logging.error(
                "Spell::EffectTriggerSpell (Spell: %u): Unsupported non-unit "
                "case!",
                m_spellInfo->Id);
        return;
    }

    uint32 triggered_spell_id = m_spellInfo->EffectTriggerSpell[effIndex];

    // special cases
    switch (triggered_spell_id)
    {
    // Vanish (root/snare removal handled by aura itself)
    case 18461:
    {
        // Don't trigger stealth part of vanish inside of hunter's flare;
        // instead show a red
        // immune message (still consumes cooldowns, breaks roots, etc)
        auto& auras = unitTarget->GetAurasByType(SPELL_AURA_DISPEL_IMMUNITY);
        for (auto aura : auras)
            if (aura->GetMiscValue() == DISPEL_STEALTH)
            {
                SendCastResult(SPELL_FAILED_IMMUNE); // shows red immune
                                                     // messsage in interface
                break;
            }

        // if this spell is given to NPC it must handle rest by it's own AI
        if (unitTarget->GetTypeId() != TYPEID_PLAYER)
            break;

        // get highest rank of the Stealth spell
        uint32 spellId = 0;
        const PlayerSpellMap& sp_list = ((Player*)unitTarget)->GetSpellMap();
        for (const auto& elem : sp_list)
        {
            // only highest rank is shown in spell book, so simply check if
            // shown in spell book
            if (!elem.second.active || elem.second.disabled ||
                elem.second.state == PLAYERSPELL_REMOVED)
                continue;

            SpellEntry const* spellInfo = sSpellStore.LookupEntry(elem.first);
            if (!spellInfo)
                continue;

            if (spellInfo->IsFitToFamily(
                    SPELLFAMILY_ROGUE, UI64LIT(0x0000000000400000)))
            {
                spellId = spellInfo->Id;
                break;
            }
        }

        // no Stealth spell found
        if (!spellId)
            break;

        // if stealth is already applied, don't apply again
        if (unitTarget->has_aura(spellId))
            break;

        unitTarget->CastSpell(unitTarget, spellId,
            TRIGGER_TYPE_TRIGGERED | TRIGGER_TYPE_IGNORE_CD);

        break;
    }
    // just skip
    case 23770: // Sayge's Dark Fortune of *
        // not exist, common cooldown can be implemented in scripts if need.
        return;
    // Brittle Armor - (need add max stack of 24575 Brittle Armor)
    case 29284:
        m_caster->CastSpell(
            unitTarget, 24575, true, m_CastItem, nullptr, m_originalCasterGUID);
        return;
    // Mercurial Shield - (need add max stack of 26464 Mercurial Shield)
    case 29286:
        m_caster->CastSpell(
            unitTarget, 26464, true, m_CastItem, nullptr, m_originalCasterGUID);
        return;
    // Cloak of Shadows
    case 35729:
    {
        unitTarget->remove_auras_if([](AuraHolder* holder)
            {
                // Remove all harmful spells on you except
                // positive/passive/physical auras
                if (!holder->IsPositive() && !holder->IsPassive() &&
                    !holder->IsDeathPersistent() &&
                    (GetSpellSchoolMask(holder->GetSpellProto()) &
                        SPELL_SCHOOL_MASK_NORMAL) == 0)
                {
                    return true;
                }
                return false;
            });
        return;
    }
    case 44949:
        // triggered spell have same category
        if (m_caster->GetTypeId() == TYPEID_PLAYER)
            ((Player*)m_caster)->RemoveSpellCooldown(triggered_spell_id);
        break;
    }

    // normal case
    SpellEntry const* spellInfo = sSpellStore.LookupEntry(triggered_spell_id);
    if (!spellInfo)
    {
        logging.error(
            "EffectTriggerSpell of spell %u: triggering unknown spell id %i",
            m_spellInfo->Id, triggered_spell_id);
        return;
    }

    // select formal caster for triggered spell
    Unit* caster = m_caster;

    // some triggered spells require specific equipment
    if (spellInfo->EquippedItemClass >= 0 &&
        m_caster->GetTypeId() == TYPEID_PLAYER)
    {
        // main hand weapon required
        if (spellInfo->AttributesEx3 & SPELL_ATTR_EX3_MAIN_HAND)
        {
            Item* item = ((Player*)m_caster)
                             ->GetWeaponForAttack(BASE_ATTACK, true, false);

            // skip spell if no weapon in slot or broken
            if (!item)
                return;

            // skip spell if weapon not fit to triggered spell
            if (!item->IsFitToSpellRequirements(spellInfo))
                return;
        }

        // offhand hand weapon required
        if (spellInfo->AttributesEx3 & SPELL_ATTR_EX3_REQ_OFFHAND)
        {
            Item* item = ((Player*)m_caster)
                             ->GetWeaponForAttack(OFF_ATTACK, true, false);

            // skip spell if no weapon in slot or broken
            if (!item)
                return;

            // skip spell if weapon not fit to triggered spell
            if (!item->IsFitToSpellRequirements(spellInfo))
                return;
        }
    }
    else
    {
        // Note: not exist spells with weapon req. and
        // IsSpellHaveCasterSourceTargets == true
        // so this just for speedup places in else
        caster = IsSpellWithCasterSourceTargetsOnly(spellInfo) ? unitTarget :
                                                                 m_caster;
    }

    caster->CastSpell(unitTarget, spellInfo,
        TRIGGER_TYPE_TRIGGERED | TRIGGER_TYPE_IGNORE_CD, nullptr, nullptr,
        m_originalCasterGUID, m_spellInfo);
}

void Spell::EffectTriggerMissileSpell(SpellEffectIndex effect_idx)
{
    uint32 triggered_spell_id = m_spellInfo->EffectTriggerSpell[effect_idx];

    // normal case
    SpellEntry const* spellInfo = sSpellStore.LookupEntry(triggered_spell_id);

    if (!spellInfo)
    {
        logging.error(
            "EffectTriggerMissileSpell of spell %u (eff: %u): triggering "
            "unknown spell id %u",
            m_spellInfo->Id, effect_idx, triggered_spell_id);
        return;
    }

    m_caster->CastSpell(m_targets.m_destX, m_targets.m_destY, m_targets.m_destZ,
        spellInfo, true, m_CastItem, nullptr, m_originalCasterGUID,
        m_spellInfo);
}

void Spell::EffectTeleportUnits(SpellEffectIndex eff_idx)
{
    if (!unitTarget || unitTarget->IsTaxiFlying())
        return;

    // Target dependend on TargetB, if there is none provided, decide dependend
    // on A
    uint32 targetType = m_spellInfo->EffectImplicitTargetB[eff_idx];
    if (!targetType)
        targetType = m_spellInfo->EffectImplicitTargetA[eff_idx];

    auto drop_combat = [this](Unit* target)
    {
        if (!m_spellInfo->HasAttribute(
                SPELL_ATTR_CUSTOM_TELEPORT_NO_LEAVE_COMBAT))
        {
            target->CombatStopWithPets(true);
            target->getHostileRefManager().deleteReferences();
        }
    };

    switch (targetType)
    {
    case TARGET_INNKEEPER_COORDINATES:
    {
        // Only players can teleport to innkeeper
        if (unitTarget->GetTypeId() != TYPEID_PLAYER)
            return;

        drop_combat(unitTarget);
        ((Player*)unitTarget)
            ->TeleportToHomebind(unitTarget == m_caster ? TELE_TO_SPELL : 0);
        return;
    }
    case TARGET_AREAEFFECT_INSTANT: // in all cases first
                                    // TARGET_TABLE_X_Y_Z_COORDINATES
    case TARGET_TABLE_X_Y_Z_COORDINATES:
    {
        SpellTargetPosition const* st =
            sSpellMgr::Instance()->GetSpellTargetPosition(m_spellInfo->Id);
        if (!st)
        {
            logging.error(
                "Spell::EffectTeleportUnits - unknown Teleport coordinates for "
                "spell ID %u",
                m_spellInfo->Id);
            return;
        }

        auto target_map = st->target_mapId;
        auto source_map = unitTarget->GetMapId();

        if (target_map == source_map &&
            unitTarget->GetTypeId() != TYPEID_PLAYER)
        {
            if (!unitTarget->GetMap()->IsDungeon())
                drop_combat(unitTarget); // dont drop combat for pve encounters
            unitTarget->NearTeleportTo(st->target_X, st->target_Y, st->target_Z,
                st->target_Orientation, unitTarget == m_caster);
        }
        else if (unitTarget->GetTypeId() == TYPEID_PLAYER)
        {
            if (!(target_map == source_map &&
                    unitTarget->GetMap()->IsDungeon()))
                drop_combat(unitTarget); // dont drop combat for pve encounters
            auto opts = (unitTarget == m_caster ? TELE_TO_SPELL : 0) |
                        TELE_TO_NOT_LEAVE_COMBAT;
            ((Player*)unitTarget)
                ->TeleportTo(target_map, st->target_X, st->target_Y,
                    st->target_Z, st->target_Orientation, opts);
        }
        break;
    }
    case TARGET_EFFECT_SELECT:
    {
        // m_destN filled, but sometimes for wrong dest and does not have
        // TARGET_FLAG_DEST_LOCATION

        float x = unitTarget->GetX();
        float y = unitTarget->GetY();
        float z = unitTarget->GetZ();
        float orientation = m_caster->GetO();

        drop_combat(m_caster);
        m_caster->NearTeleportTo(x, y, z, orientation, unitTarget == m_caster);
        return;
    }
    case TARGET_BEHIND_VICTIM:
    {
        Unit* pTarget = nullptr;

        // explicit cast data from client or server-side cast
        // some spell at client send caster
        if (m_targets.getUnitTarget() &&
            m_targets.getUnitTarget() != unitTarget)
            pTarget = m_targets.getUnitTarget();
        else if (unitTarget->getVictim())
            pTarget = unitTarget->getVictim();
        else if (unitTarget->GetTypeId() == TYPEID_PLAYER)
            pTarget = unitTarget->GetMap()->GetUnit(
                ((Player*)unitTarget)->GetSelectionGuid());

        // Init dest coordinates
        float x = m_targets.m_destX;
        float y = m_targets.m_destY;
        float z = m_targets.m_destZ;
        float orientation = pTarget ? pTarget->GetO() : unitTarget->GetO();
        unitTarget->NearTeleportTo(
            x, y, z, orientation, unitTarget == m_caster);
        return;
    }
    default:
    {
        // If not exist data for dest location - return
        if (!(m_targets.m_targetMask & TARGET_FLAG_DEST_LOCATION))
        {
            logging.error(
                "Spell::EffectTeleportUnits - unknown "
                "EffectImplicitTargetB[%u] = %u for spell ID %u",
                eff_idx, m_spellInfo->EffectImplicitTargetB[eff_idx],
                m_spellInfo->Id);
            return;
        }
        // Init dest coordinates
        float x = m_targets.m_destX;
        float y = m_targets.m_destY;
        float z = m_targets.m_destZ;
        float orientation = unitTarget->GetO();
        // Teleport
        drop_combat(unitTarget);
        unitTarget->NearTeleportTo(
            x, y, z, orientation, unitTarget == m_caster);
        return;
    }
    }

    // post effects for TARGET_TABLE_X_Y_Z_COORDINATES
    switch (m_spellInfo->Id)
    {
    // Dimensional Ripper - Everlook
    case 23442:
    {
        if (m_caster->GetTypeId() != TYPEID_PLAYER)
            return;

        if (roll_chance_i(50))
        {
            switch (urand(0, 3))
            {
            case 0: // Evil Twin
                m_caster->CastSpell(m_caster, 23445, true);
                break;
            case 1: // Set on fire
                m_caster->CastSpell(m_caster, 23449, true);
                break;
            case 2: // Polymorph
                m_caster->CastSpell(m_caster, 23444, true);
                break;
            case 3: // Fall to death
                // TODO: These are made up coordinates, find the real ones,
                // and there should probably be a spell available as well
                static_cast<Player*>(m_caster)->TeleportTo(
                    1, 6670, -4560, 860, 5.5);
                break;
            }
        }
        return;
    }
    case 36941: // Ultrasafe Transporter: Toshley's Station
    case 36890: // Dimensional Ripper - Area 52
    {
        // Source:
        // http://www.wowhead.com/item=30544/ultrasafe-transporter-toshleys-station#comments:id=44628

        if (m_caster->GetTypeId() != TYPEID_PLAYER)
            return;

        if (roll_chance_i(50)) // 50% success
        {
            switch (urand(0, 7))
            {
            case 0: // soul split - evil
                m_caster->CastSpell(m_caster, 36900, true);
                break;
            case 1: // soul split - good
                m_caster->CastSpell(m_caster, 36901, true);
                break;
            case 2: // increase size
                m_caster->CastSpell(m_caster, 36895, true);
                break;
            case 3: // decrease size
                m_caster->CastSpell(m_caster, 36893, true);
                break;
            case 4: // transform into previous user
                // The transform is NOT team based but random
                if (urand(0, 1))
                    m_caster->CastSpell(m_caster, 36899, true);
                else
                    m_caster->CastSpell(m_caster, 36897, true);
                break;
            case 5: // teleport
                // TODO: does a spell exist for this failure?
                if (m_spellInfo->Id == 36941)
                    static_cast<Player*>(m_caster)->TeleportTo(530, 1900, 5615,
                        460, 5); // Toshley's Station (TODO: It's possible this
                                 // one does not have a failed telport)
                else
                    static_cast<Player*>(m_caster)->TeleportTo(
                        530, 2710, 3727, 489.7, 6); // Area 52
                break;
            case 6: // transform into black chicken
                m_caster->CastSpell(m_caster, 36940, true);
                break;
            case 7: // evil twin
                m_caster->CastSpell(m_caster, 23445, true);
                break;
            }
        }
        return;
    }
    }
}

void Spell::EffectApplyAura(SpellEffectIndex eff_idx)
{
    if (!unitTarget)
        return;

    // ghost spell check, allow apply any auras at player loading in ghost mode
    // (will be cleanup after load)
    if ((!unitTarget->isAlive() &&
            !(IsDeathOnlySpell(m_spellInfo) ||
                IsDeathPersistentSpell(m_spellInfo))) &&
        (unitTarget->GetTypeId() != TYPEID_PLAYER ||
            !((Player*)unitTarget)->GetSession()->PlayerLoading()))
        return;

    Unit* caster = GetAffectiveCaster();
    if (!caster)
    {
        // FIXME: currently we can't have auras applied explicitly by
        // gameobjects
        // so for auras from wild gameobjects (no owner) target used
        if (m_originalCasterGUID.IsGameObject())
            caster = unitTarget;
        else
            return;
    }

    // Do not allow CC on players flying on a mount in the air
    if (unitTarget->GetTypeId() == TYPEID_PLAYER &&
        unitTarget->m_movementInfo.HasMovementFlag(MOVEFLAG_FLYING2) &&
        unitTarget->HasAuraType(SPELL_AURA_MOUNTED) &&
        (m_spellInfo->HasApplyAuraName(SPELL_AURA_MOD_STUN) ||
            m_spellInfo->HasApplyAuraName(SPELL_AURA_MOD_CONFUSE) ||
            m_spellInfo->HasApplyAuraName(SPELL_AURA_MOD_CHARM) ||
            m_spellInfo->HasApplyAuraName(SPELL_AURA_MOD_FEAR) ||
            m_spellInfo->HasApplyAuraName(SPELL_AURA_MOD_ROOT)))
    {
        return;
    }

    if ((m_spellInfo->HasApplyAuraName(SPELL_AURA_MOD_POSSESS) ||
            m_spellInfo->HasApplyAuraName(SPELL_AURA_MOD_CHARM)) &&
        (unitTarget->HasAuraType(SPELL_AURA_MOD_FLIGHT_SPEED_MOUNTED) ||
            unitTarget->HasAuraType(SPELL_AURA_FLY)))
        return;

    auto aura = m_spellInfo->EffectApplyAuraName[eff_idx];

    // Mod stat percentage does not work on some NPCs; however it still causes
    // on-hit procs, which means it needs to be blocked out here.
    if (unitTarget->GetTypeId() == TYPEID_UNIT &&
        static_cast<Creature*>(unitTarget)->GetCreatureInfo()->flags_extra &
            CREATURE_FLAG_EXTRA_NO_NEGATIVE_STAT_MODS &&
        m_spellInfo->EffectBasePoints[eff_idx] < 0 &&
        (aura == SPELL_AURA_MOD_STAT ||
            aura == SPELL_AURA_MOD_TOTAL_STAT_PERCENTAGE))
    {
        m_caster->SendSpellMiss(unitTarget, m_spellInfo->Id, SPELL_MISS_IMMUNE);
        return;
    }

    LOG_DEBUG(logging, "Spell: Aura is: %u", aura);

    Aura* aur = CreateAura(m_spellInfo, eff_idx, &m_currentBasePoints[eff_idx],
        m_spellAuraHolder, unitTarget, caster, m_CastItem);
    m_spellAuraHolder->AddAura(aur, eff_idx);
}

void Spell::EffectUnlearnSpecialization(SpellEffectIndex eff_idx)
{
    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
        return;

    Player* _player = (Player*)unitTarget;
    uint32 spellToUnlearn = m_spellInfo->EffectTriggerSpell[eff_idx];

    _player->removeSpell(spellToUnlearn);

    LOG_DEBUG(logging,
        "Spell: Player %u has unlearned spell %u from NpcGUID: %u",
        _player->GetGUIDLow(), spellToUnlearn, m_caster->GetGUIDLow());
}

void Spell::EffectPowerDrain(SpellEffectIndex eff_idx)
{
    if (m_spellInfo->EffectMiscValue[eff_idx] < 0 ||
        m_spellInfo->EffectMiscValue[eff_idx] >= MAX_POWERS)
        return;

    Powers drain_power = Powers(m_spellInfo->EffectMiscValue[eff_idx]);

    if (!unitTarget)
        return;
    if (!unitTarget->isAlive())
        return;
    if (unitTarget->getPowerType() != drain_power &&
        !(drain_power == POWER_HAPPINESS &&
            unitTarget->GetTypeId() == TYPEID_UNIT &&
            static_cast<Creature*>(unitTarget)->IsPet() &&
            static_cast<Pet*>(unitTarget)->getPetType() == HUNTER_PET))
        return;
    if (damage < 0)
        return;

    uint32 curPower = unitTarget->GetPower(drain_power);

    // add spell damage bonus
    damage = m_caster->SpellDamageBonusDone(
        unitTarget, m_spellInfo, uint32(damage), SPELL_DIRECT_DAMAGE);
    damage = unitTarget->SpellDamageBonusTaken(
        m_caster, m_spellInfo, uint32(damage), SPELL_DIRECT_DAMAGE);

    // resilience reduce mana draining effect at spell crit damage reduction
    // (added in 2.4)
    uint32 power = damage;
    if (drain_power == POWER_MANA && unitTarget->GetTypeId() == TYPEID_PLAYER)
        power -= ((Player*)unitTarget)->GetSpellCritDamageReduction(power);

    int32 new_damage;
    if (curPower < power)
        new_damage = curPower;
    else
        new_damage = power;

    unitTarget->ModifyPower(drain_power, -new_damage);

    // Don`t restore from self drain (or BE racial: mana tap)
    if (drain_power == POWER_MANA && m_caster != unitTarget &&
        m_spellInfo->Id != 28734)
    {
        float manaMultiplier = m_spellInfo->EffectMultipleValue[eff_idx];
        if (manaMultiplier == 0)
            manaMultiplier = 1;

        if (Player* modOwner = m_caster->GetSpellModOwner())
            modOwner->ApplySpellMod(
                m_spellInfo->Id, SPELLMOD_MULTIPLE_VALUE, manaMultiplier);

        int32 gain = int32(new_damage * manaMultiplier);

        m_caster->EnergizeBySpell(m_caster, m_spellInfo->Id, gain, POWER_MANA);

        // Mana drain breaks CC the same way direct damage does
        if (new_damage > 0 &&
            !(m_spellInfo->AttributesEx4 &
                SPELL_ATTR_EX4_NO_PUSHBACK_OR_CC_BREAKAGE))
            unitTarget->remove_auras_on_event(
                AURA_INTERRUPT_FLAG_DAMAGE, m_spellInfo->Id);
    }
}

void Spell::EffectSendEvent(SpellEffectIndex effectIndex)
{
    /*
    we do not handle a flag dropping or clicking on flag in battleground by
    sendevent system
    */
    LOG_DEBUG(logging,
        "Spell ScriptStart %u for spellid %u in EffectSendEvent ",
        m_spellInfo->EffectMiscValue[effectIndex], m_spellInfo->Id);

    if (!sScriptMgr::Instance()->OnProcessEvent(
            m_spellInfo->EffectMiscValue[effectIndex], m_caster, focusObject,
            true))
        m_caster->GetMap()->ScriptsStart(sEventScripts,
            m_spellInfo->EffectMiscValue[effectIndex], m_caster, focusObject);
}

void Spell::EffectPowerBurn(SpellEffectIndex eff_idx)
{
    if (m_spellInfo->EffectMiscValue[eff_idx] < 0 ||
        m_spellInfo->EffectMiscValue[eff_idx] >= MAX_POWERS)
        return;

    Powers powertype = Powers(m_spellInfo->EffectMiscValue[eff_idx]);

    if (!unitTarget)
        return;
    if (!unitTarget->isAlive())
        return;
    if (unitTarget->getPowerType() != powertype)
        return;
    if (damage < 0)
        return;

    int32 curPower = int32(unitTarget->GetPower(powertype));

    // resilience reduce mana draining effect at spell crit damage reduction
    // (added in 2.4)
    int32 power = damage;
    if (powertype == POWER_MANA && unitTarget->GetTypeId() == TYPEID_PLAYER)
        power -= ((Player*)unitTarget)->GetSpellCritDamageReduction(power);

    int32 new_damage = (curPower < power) ? curPower : power;

    if (new_damage > 0 &&
        !(m_spellInfo->AttributesEx4 &
            SPELL_ATTR_EX4_NO_PUSHBACK_OR_CC_BREAKAGE))
    {
        unitTarget->remove_auras_on_event(
            AURA_INTERRUPT_FLAG_DAMAGE, m_spellInfo->Id);
        unitTarget->RemoveSpellbyDamageTaken(
            SPELL_AURA_MOD_ROOT, new_damage, false);
        unitTarget->RemoveSpellbyDamageTaken(
            SPELL_AURA_MOD_FEAR, new_damage, false);
    }

    unitTarget->ModifyPower(powertype, -new_damage);
    float multiplier = m_spellInfo->EffectMultipleValue[eff_idx];

    if (Player* modOwner = m_caster->GetSpellModOwner())
        modOwner->ApplySpellMod(
            m_spellInfo->Id, SPELLMOD_MULTIPLE_VALUE, multiplier);

    new_damage = int32(new_damage * multiplier);
    m_damage += new_damage;
}

void Spell::EffectHeal(SpellEffectIndex /*eff_idx*/)
{
    if (unitTarget && unitTarget->isAlive() && damage >= 0)
    {
        // Try to get original caster
        Unit* caster = GetAffectiveCaster();
        if (!caster)
            return;

        int32 addhealth = damage;

        // Vessel of the Naaru (Vial of the Sunwell trinket)
        if (m_spellInfo->Id == 45064)
        {
            // Amount of heal - depends from stacked Holy Energy
            int damageAmount = 0;
            auto& dummyAuras = m_caster->GetAurasByType(SPELL_AURA_DUMMY);
            for (const auto& dummyAura : dummyAuras)
                if ((dummyAura)->GetId() == 45062)
                    damageAmount += (dummyAura)->GetModifier()->m_amount;
            if (damageAmount)
                m_caster->remove_auras(45062);

            addhealth += damageAmount;
        }
        // Swiftmend - consumes Regrowth or Rejuvenation
        else if (m_spellInfo->TargetAuraState == AURA_STATE_SWIFTMEND &&
                 unitTarget->HasAuraState(AURA_STATE_SWIFTMEND))
        {
            auto& RejorRegr =
                unitTarget->GetAurasByType(SPELL_AURA_PERIODIC_HEAL);
            // find most short by duration
            Aura* targetAura = nullptr;
            for (const auto& elem : RejorRegr)
            {
                if ((elem)->GetSpellProto()->SpellFamilyName ==
                        SPELLFAMILY_DRUID &&
                    // Regrowth or Rejuvenation 0x40 | 0x10
                    ((elem)->GetSpellProto()->SpellFamilyFlags &
                        UI64LIT(0x0000000000000050)))
                {
                    if (!targetAura ||
                        (elem)->GetAuraDuration() <
                            targetAura->GetAuraDuration())
                        targetAura = elem;
                }
            }

            if (!targetAura)
            {
                logging.error(
                    "Target (GUID: %u TypeId: %u) has aurastate "
                    "AURA_STATE_SWIFTMEND but no matching aura.",
                    unitTarget->GetGUIDLow(), unitTarget->GetTypeId());
                return;
            }
            int idx = 0;
            while (idx < 3)
            {
                if (targetAura->GetSpellProto()->EffectApplyAuraName[idx] ==
                    SPELL_AURA_PERIODIC_HEAL)
                    break;
                idx++;
            }

            int32 tickheal = targetAura->GetModifier()->m_amount;
            int32 tickcount = targetAura->GetSpellProto()->SpellFamilyFlags &
                                      UI64LIT(0x0000000000000010) ?
                                  4 :
                                  6;

            unitTarget->remove_auras(targetAura->GetId());

            addhealth += tickheal * tickcount;
        }
        // Enchant Weapon Crusader - Reduced effect for level 60+
        else if (m_spellInfo->Id == 20007)
        {
            uint32 level = unitTarget->getLevel();
            if (level > 60)
                addhealth *= (1 - 0.04 * (level - 60));
        }

        if (m_triggeredBySpellInfo)
        {
            addhealth = caster->SpellHealingBonusDone(
                unitTarget, m_triggeredBySpellInfo, addhealth, HEAL);
            addhealth = unitTarget->SpellHealingBonusTaken(
                caster, m_triggeredBySpellInfo, addhealth, HEAL);
        }
        else
        {
            addhealth = caster->SpellHealingBonusDone(
                unitTarget, m_spellInfo, addhealth, HEAL);
            addhealth = unitTarget->SpellHealingBonusTaken(
                caster, m_spellInfo, addhealth, HEAL);
        }

        m_healing += addhealth;
    }
}

void Spell::EffectHealPct(SpellEffectIndex /*eff_idx*/)
{
    if (unitTarget && unitTarget->isAlive() && damage >= 0)
    {
        // Try to get original caster
        Unit* caster = GetAffectiveCaster();
        if (!caster)
            return;

        uint32 addhealth = unitTarget->GetMaxHealth() * damage / 100;

        addhealth = caster->SpellHealingBonusDone(
            unitTarget, m_spellInfo, addhealth, HEAL);
        addhealth = unitTarget->SpellHealingBonusTaken(
            caster, m_spellInfo, addhealth, HEAL);

        int32 gain = caster->DealHeal(unitTarget, addhealth, m_spellInfo);
        unitTarget->getHostileRefManager().threatAssist(caster,
            float(gain) * 0.5f *
                sSpellMgr::Instance()->GetSpellThreatMultiplier(m_spellInfo),
            m_spellInfo);
    }
}

void Spell::EffectHealMechanical(SpellEffectIndex /*eff_idx*/)
{
    // Mechanic creature type should be correctly checked by targetCreatureType
    // field
    if (unitTarget && unitTarget->isAlive() && damage >= 0)
    {
        // Try to get original caster
        Unit* caster = GetAffectiveCaster();
        if (!caster)
            return;

        uint32 addhealth = caster->SpellHealingBonusDone(
            unitTarget, m_spellInfo, damage, HEAL);
        addhealth = unitTarget->SpellHealingBonusTaken(
            caster, m_spellInfo, addhealth, HEAL);

        caster->DealHeal(unitTarget, addhealth, m_spellInfo);
    }
}

void Spell::EffectHealthLeech(SpellEffectIndex eff_idx)
{
    if (!unitTarget)
        return;
    if (!unitTarget->isAlive())
        return;

    if (damage <= 0)
        return;

    health_leech_multiplier_ = m_spellInfo->EffectMultipleValue[eff_idx];

    // NOTE: Health Leech is mutually exclusive to all other damage calc
    // effects (no spell exists that has this effect + anothe damage effect)
    m_damage = damage;
}

void Spell::DoCreateItem(SpellEffectIndex /*eff_idx*/, uint32 itemtype)
{
    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
        return;

    Player* player = (Player*)unitTarget;

    uint32 newitemid = itemtype;
    ItemPrototype const* pProto = ObjectMgr::GetItemPrototype(newitemid);
    if (!pProto)
    {
        player->SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, nullptr, nullptr);
        return;
    }

    uint32 num_to_add = damage;

    if (num_to_add < 1)
        num_to_add = 1;
    if (num_to_add > pProto->Stackable)
        num_to_add = pProto->Stackable;

    // init items_count to 1, since 1 item will be created regardless of
    // specialization
    int items_count = 1;
    // the chance to create additional items
    float additionalCreateChance = 0.0f;
    // the maximum number of created additional items
    uint8 additionalMaxNum = 0;
    // get the chance and maximum number for creating extra items
    if (canCreateExtraItems(
            player, m_spellInfo->Id, additionalCreateChance, additionalMaxNum))
    {
        // roll with this chance till we roll not to create or we create the max
        // num
        while (roll_chance_f(additionalCreateChance) &&
               items_count <= additionalMaxNum)
            ++items_count;
    }

    // really will be created more items
    num_to_add *= items_count;

    // can the player store the new item?
    /* XXX */
    if (!num_to_add)
        return;

    Item* pItem = nullptr; // Can be NULL if item was added onto a stack

    inventory::transaction trans(true, inventory::transaction::send_party,
        inventory::transaction::add_craft);
    trans.add(newitemid, num_to_add);
    if (!player->storage().finalize(trans))
    {
        // XXX: SendEquipError: EQUIP_ERR_INVENTORY_FULL || msg ==
        // EQUIP_ERR_CANT_CARRY_MORE_OF_THIS
        if (trans.add_failures()[0] == num_to_add)
            return;
        num_to_add -= trans.add_failures()[0];
        trans = inventory::transaction();
        trans.add(newitemid, num_to_add);
        if (!player->storage().finalize(trans))
            return;
        if (trans.added_items().size() > 0)
            pItem = trans.added_items()[0];
    }
    else if (trans.added_items().size() > 0)
        pItem = trans.added_items()[0];

    // Set crafted by property
    if (pItem && pItem->GetProto()->Class != ITEM_CLASS_CONSUMABLE &&
        pItem->GetProto()->Class != ITEM_CLASS_QUEST &&
        pItem->GetProto()->Stackable <= 1)
        pItem->SetGuidValue(ITEM_FIELD_CREATOR, player->GetObjectGuid());

    // We succeeded in creating at least one item, so a levelup is possible
    player->UpdateCraftSkill(m_spellInfo->Id);
}

void Spell::EffectCreateItem(SpellEffectIndex eff_idx)
{
    DoCreateItem(eff_idx, m_spellInfo->EffectItemType[eff_idx]);
}

void Spell::EffectPersistentAA(SpellEffectIndex eff_idx)
{
    Unit* pCaster = GetAffectiveCaster();
    // FIXME: in case wild GO will used wrong affective caster (target in fact)
    // as dynobject owner
    if (!pCaster)
        pCaster = m_caster;

    float radius = GetSpellRadius(
        sSpellRadiusStore.LookupEntry(m_spellInfo->EffectRadiusIndex[eff_idx]));

    if (Player* modOwner = pCaster->GetSpellModOwner())
        modOwner->ApplySpellMod(m_spellInfo->Id, SPELLMOD_RADIUS, radius);

    auto dynObj = new DynamicObject;
    if (!dynObj->Create(
            pCaster->GetMap()->GenerateLocalLowGuid(HIGHGUID_DYNAMICOBJECT),
            pCaster, m_spellInfo->Id, eff_idx, m_targets.m_destX,
            m_targets.m_destY, m_targets.m_destZ, m_duration, radius,
            DYNAMIC_OBJECT_AREA_SPELL))
    {
        delete dynObj;
        return;
    }

    if (!pCaster->GetMap()->insert(dynObj))
    {
        delete dynObj;
        return;
    }

    auto caster_guid = pCaster->GetObjectGuid();
    dynObj->queue_action(0, [dynObj, caster_guid]()
        {
            if (auto caster = dynObj->GetMap()->GetUnit(caster_guid))
                caster->AddDynObject(dynObj);
            dynObj->Update(0, 0); // avoid missing tick, no diff
        });
}

void Spell::EffectEnergize(SpellEffectIndex eff_idx)
{
    if (!unitTarget)
        return;
    if (!unitTarget->isAlive())
        return;

    if (m_spellInfo->EffectMiscValue[eff_idx] < 0 ||
        m_spellInfo->EffectMiscValue[eff_idx] >= MAX_POWERS)
        return;

    Powers power = Powers(m_spellInfo->EffectMiscValue[eff_idx]);

    if (unitTarget->GetTypeId() == TYPEID_PLAYER &&
        unitTarget->getPowerType() != power)
        return;

    if (unitTarget->GetMaxPower(power) == 0)
        return;

    // Some level depends spells
    int level_multiplier = 0;
    int level_diff = 0;
    switch (m_spellInfo->Id)
    {
    case 9512: // Restore Energy
        level_diff = m_caster->getLevel() - 40;
        level_multiplier = 2;
        break;
    case 24571: // Blood Fury
        level_diff = m_caster->getLevel() - 60;
        level_multiplier = 10;
        break;
    case 24532: // Burst of Energy
        level_diff = m_caster->getLevel() - 60;
        level_multiplier = 4;
        break;
    default:
        break;
    }

    if (level_diff > 0)
        damage -= level_multiplier * level_diff;

    if (damage < 0)
        return;

    // Alchemist Stone Bonus +40%
    if (m_spellInfo->SpellFamilyName == SPELLFAMILY_POTION &&
        power == POWER_MANA && unitTarget->has_aura(17619))
        damage *= 1.4f;

    m_caster->EnergizeBySpell(unitTarget, m_spellInfo->Id, damage, power);

    // Mage SSC Trinket
    if (m_spellInfo->IsFitToFamily(
            SPELLFAMILY_MAGE, UI64LIT(0x0000010000000000)) &&
        m_caster->has_aura(37447))
        m_caster->CastSpell(m_caster, 37445, true);

    // Mad Alchemist's Potion
    if (m_spellInfo->Id == 45051)
    {
        // find elixirs on target
        uint32 elixir_mask = 0;
        unitTarget->loop_auras([&elixir_mask](AuraHolder* holder)
            {
                if (uint32 mask = sSpellMgr::Instance()->GetSpellElixirMask(
                        holder->GetId()))
                    elixir_mask |= mask;
                return true; // continue
            });

        // get available elixir mask any not active type from battle/guardian
        // (and flask if no any)
        elixir_mask = (elixir_mask & ELIXIR_FLASK_MASK) ^ ELIXIR_FLASK_MASK;

        // get all available elixirs by mask and spell level
        std::vector<uint32> elixirs;
        SpellElixirMap const& m_spellElixirs =
            sSpellMgr::Instance()->GetSpellElixirMap();
        for (const auto& m_spellElixir : m_spellElixirs)
        {
            if (m_spellElixir.second & elixir_mask)
            {
                if (m_spellElixir.second &
                    (ELIXIR_UNSTABLE_MASK | ELIXIR_SHATTRATH_MASK))
                    continue;

                SpellEntry const* spellInfo =
                    sSpellStore.LookupEntry(m_spellElixir.first);
                if (spellInfo &&
                    (spellInfo->spellLevel < m_spellInfo->spellLevel ||
                        spellInfo->spellLevel > unitTarget->getLevel()))
                    continue;

                elixirs.push_back(m_spellElixir.first);
            }
        }

        if (!elixirs.empty())
        {
            // cast random elixir on target
            uint32 rand_spell = urand(0, elixirs.size() - 1);
            m_caster->CastSpell(
                unitTarget, elixirs[rand_spell], true, m_CastItem);
        }
    }

    // Consume magic also needs to consume a buff. The fact that we have a buff
    // is checked in Spell::CheckCast
    AuraHolder* holder;
    if (m_spellInfo->Id == 32676 &&
        (holder = Spell::consume_magic_buff(m_caster)) != nullptr)
        m_caster->RemoveAuraHolder(holder, AURA_REMOVE_BY_DISPEL);
}

void Spell::EffectEnergisePct(SpellEffectIndex eff_idx)
{
    if (!unitTarget)
        return;
    if (!unitTarget->isAlive())
        return;

    if (m_spellInfo->EffectMiscValue[eff_idx] < 0 ||
        m_spellInfo->EffectMiscValue[eff_idx] >= MAX_POWERS)
        return;

    Powers power = Powers(m_spellInfo->EffectMiscValue[eff_idx]);

    if (unitTarget->GetTypeId() == TYPEID_PLAYER &&
        unitTarget->getPowerType() != power)
        return;

    uint32 maxPower = unitTarget->GetMaxPower(power);
    if (maxPower == 0)
        return;

    uint32 gain = damage * maxPower / 100;
    m_caster->EnergizeBySpell(unitTarget, m_spellInfo->Id, gain, power);
}

void Spell::SendLoot(ObjectGuid guid, LootType loottype, LockType lockType)
{
    if (gameObjTarget)
    {
        switch (gameObjTarget->GetGoType())
        {
        case GAMEOBJECT_TYPE_DOOR:
        case GAMEOBJECT_TYPE_BUTTON:
        case GAMEOBJECT_TYPE_QUESTGIVER:
        case GAMEOBJECT_TYPE_SPELL_FOCUS:
        case GAMEOBJECT_TYPE_GOOBER:
            gameObjTarget->Use(m_caster);
            return;

        case GAMEOBJECT_TYPE_CHEST:
            gameObjTarget->Use(m_caster);
            // Don't return, let loots been taken
            break;

        case GAMEOBJECT_TYPE_TRAP:
            if (lockType == LOCKTYPE_DISARM_TRAP)
            {
                gameObjTarget->SetLootState(GO_JUST_DEACTIVATED);
                return;
            }
            logging.error(
                "Spell::SendLoot unhandled locktype %u for GameObject trap "
                "(entry %u) for spell %u.",
                lockType, gameObjTarget->GetEntry(), m_spellInfo->Id);
            return;
        default:
            logging.error(
                "Spell::SendLoot unhandled GameObject type %u (entry %u).",
                gameObjTarget->GetGoType(), gameObjTarget->GetEntry());
            return;
        }
    }

    if (m_caster->GetTypeId() != TYPEID_PLAYER)
        return;

    // Send loot
    ((Player*)m_caster)->SendLoot(guid, loottype);
}

void Spell::EffectOpenLock(SpellEffectIndex eff_idx)
{
    if (!m_caster || m_caster->GetTypeId() != TYPEID_PLAYER)
    {
        LOG_DEBUG(logging, "WORLD: Open Lock - No Player Caster!");
        return;
    }

    Player* player = (Player*)m_caster;

    uint32 lockId = 0;
    ObjectGuid guid;

    // Get lockId
    if (gameObjTarget)
    {
        GameObjectInfo const* goInfo = gameObjTarget->GetGOInfo();
        // Arathi Basin banner opening !
        if ((goInfo->type == GAMEOBJECT_TYPE_BUTTON &&
                goInfo->button.noDamageImmune) ||
            (goInfo->type == GAMEOBJECT_TYPE_GOOBER && goInfo->goober.losOK))
        {
            // CanUseBattleGroundObject() already called in CheckCast()
            // in battleground check
            if (BattleGround* bg = player->GetBattleGround())
            {
                // check if it's correct bg
                if (bg->GetTypeID() == BATTLEGROUND_AB ||
                    bg->GetTypeID() == BATTLEGROUND_AV)
                    bg->EventPlayerClickedOnFlag(player, gameObjTarget);
                return;
            }
        }
        else if (goInfo->type == GAMEOBJECT_TYPE_FLAGSTAND)
        {
            // CanUseBattleGroundObject() already called in CheckCast()
            // in battleground check
            if (BattleGround* bg = player->GetBattleGround())
            {
                if (bg->GetTypeID() == BATTLEGROUND_EY)
                    bg->EventPlayerClickedOnFlag(player, gameObjTarget);
                return;
            }
        }
        lockId = goInfo->GetLockId();
        guid = gameObjTarget->GetObjectGuid();
    }
    else if (itemTarget)
    {
        lockId = itemTarget->GetProto()->LockID;
        guid = itemTarget->GetObjectGuid();
    }
    else
    {
        LOG_DEBUG(logging, "WORLD: Open Lock - No GameObject/Item Target!");
        return;
    }

    SkillType skillId = SKILL_NONE;
    int32 reqSkillValue = 0;
    int32 skillValue;

    SpellCastResult res =
        CanOpenLock(eff_idx, lockId, skillId, reqSkillValue, skillValue);
    if (res != SPELL_CAST_OK)
    {
        SendCastResult(res);
        return;
    }

    // mark item as unlocked
    if (itemTarget)
        itemTarget->SetFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_UNLOCKED);
    else if (gameObjTarget)
    {
        if (gameObjTarget->HasFlag(GAMEOBJECT_FLAGS, GO_FLAG_LOCKED))
            gameObjTarget->RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_LOCKED);

        // We need to remove the cast item before sending the loot, or the loot
        // display will be closed once the item is removed
        // NOTE: This nulls out m_CastItem so there's no possiblity of
        // "charging" the player twice
        TakeCastItem();

        SendLoot(guid, LOOT_SKINNING,
            static_cast<LockType>(m_spellInfo->EffectMiscValue[eff_idx]));
    }

    // not allow use skill grow at item base open
    if (!m_CastItem && skillId != SKILL_NONE)
    {
        // update skill if really known
        if (uint32 pureSkillValue = player->GetPureSkillValue(skillId))
        {
            if (gameObjTarget)
            {
                // Allow one skill-up until respawned
                if (!gameObjTarget->IsInSkillupList(player) &&
                    player->UpdateGatherSkill(
                        skillId, pureSkillValue, reqSkillValue))
                    gameObjTarget->AddToSkillupList(player);
            }
            else if (itemTarget)
            {
                // Do one skill-up
                player->UpdateGatherSkill(
                    skillId, pureSkillValue, reqSkillValue);
            }
        }
    }
}

void Spell::EffectSummonChangeItem(SpellEffectIndex eff_idx)
{
    if (m_caster->GetTypeId() != TYPEID_PLAYER)
        return;

    Player* player = static_cast<Player*>(m_caster);

    // Effect always targets the item we cast the spell with
    if (!m_CastItem)
        return;

    // The item must be in our inventory
    if (!m_CastItem->slot().equipment() && !m_CastItem->slot().backpack() &&
        !m_CastItem->slot().extra_bag())
        return;
    if (player->storage().get(m_CastItem->slot()) != m_CastItem)
        return;

    uint32 new_item_id = m_spellInfo->EffectItemType[eff_idx];
    if (!new_item_id)
        return;

    Item* old_item = m_CastItem;

    // The casting item is about to go missing; remove it from the spell's data
    ClearCastItem();

    std::unique_ptr<Item> new_item(Item::CreateItem(
        new_item_id, 1, player, old_item->GetItemRandomPropertyId()));
    if (!new_item)
        return;

    // Copy enchantments to the new item
    for (int i = PERM_ENCHANTMENT_SLOT; i <= TEMP_ENCHANTMENT_SLOT; ++i)
    {
        EnchantmentSlot s = static_cast<EnchantmentSlot>(i);
        if (uint32 ench_id = old_item->GetEnchantmentId(s))
            new_item->SetEnchantment(s, ench_id,
                old_item->GetEnchantmentDuration(s),
                old_item->GetEnchantmentCharges(s));
    }

    // Copy durability
    if (old_item->GetUInt32Value(ITEM_FIELD_MAXDURABILITY) > 0)
    {
        double dura_pct = old_item->GetUInt32Value(ITEM_FIELD_DURABILITY) /
                          static_cast<double>(old_item->GetUInt32Value(
                              ITEM_FIELD_MAXDURABILITY));
        player->durability(new_item.get(), true, dura_pct);
    }

    // Try to complete the transformation (FIXME: This should probably not
    // unequip the item?)
    inventory::transaction trans(false, inventory::transaction::send_self,
        inventory::transaction::add_craft);
    trans.destroy(old_item);
    trans.add(new_item.get());
    if (!player->storage().finalize(trans))
    {
        player->SendEquipError(
            static_cast<InventoryResult>(trans.error()), old_item);
        return;
    }

    new_item.release(); // Item got stored, we're no longer
                        // responsible for the resources
}

void Spell::EffectProficiency(SpellEffectIndex /*eff_idx*/)
{
    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
        return;
    Player* p_target = (Player*)unitTarget;

    uint32 subClassMask = m_spellInfo->EquippedItemSubClassMask;
    if (m_spellInfo->EquippedItemClass == ITEM_CLASS_WEAPON &&
        !(p_target->GetWeaponProficiency() & subClassMask))
    {
        p_target->AddWeaponProficiency(subClassMask);
        p_target->SendProficiency(
            ITEM_CLASS_WEAPON, p_target->GetWeaponProficiency());
    }
    if (m_spellInfo->EquippedItemClass == ITEM_CLASS_ARMOR &&
        !(p_target->GetArmorProficiency() & subClassMask))
    {
        p_target->AddArmorProficiency(subClassMask);
        p_target->SendProficiency(
            ITEM_CLASS_ARMOR, p_target->GetArmorProficiency());
    }
}

void Spell::EffectApplyAreaAura(SpellEffectIndex eff_idx)
{
    if (!unitTarget)
        return;
    if (!unitTarget->isAlive())
        return;

    auto Aur = new AreaAura(m_spellInfo, eff_idx, &m_currentBasePoints[eff_idx],
        m_spellAuraHolder, unitTarget, m_caster, m_CastItem);
    m_spellAuraHolder->AddAura(Aur, eff_idx);
}

void Spell::EffectSummonType(SpellEffectIndex eff_idx)
{
    uint32 prop_id = m_spellInfo->EffectMiscValueB[eff_idx];
    SummonPropertiesEntry const* summon_prop =
        sSummonPropertiesStore.LookupEntry(prop_id);
    if (!summon_prop)
    {
        logging.error("EffectSummonType: Unhandled summon type %u", prop_id);
        return;
    }
    switch (summon_prop->Group)
    {
    // faction handled later on, or loaded from template
    case SUMMON_PROP_GROUP_WILD:
    case SUMMON_PROP_GROUP_FRIENDLY:
    {
        switch (summon_prop->Title)
        {
        case UNITNAME_SUMMON_TITLE_NONE:
        {
            // 121 are all totems. However, retail testing shows they're not
            // owned by their summoner.
            if (prop_id == 121)
                DoSummonTotem(eff_idx);
            else
                DoSummonWild(eff_idx, summon_prop->FactionId);
            break;
        }
        case UNITNAME_SUMMON_TITLE_PET:
            DoSummonGuardian(eff_idx, summon_prop->FactionId);
            break;
        case UNITNAME_SUMMON_TITLE_GUARDIAN:
        {
            if (prop_id == 61) // mixed guardians, totems, statues
            {
                // * Stone Statue, etc  -- fits much better totem AI
                if (m_spellInfo->SpellIconID == 2056)
                    DoSummonTotem(eff_idx);
                else
                {
                    // possible sort totems/guardians only by summon creature
                    // type
                    CreatureInfo const* cInfo = ObjectMgr::GetCreatureTemplate(
                        m_spellInfo->EffectMiscValue[eff_idx]);

                    if (!cInfo)
                        return;

                    // FIXME: not all totems and similar cases seelcted by this
                    // check...
                    if (cInfo->type == CREATURE_TYPE_TOTEM)
                        DoSummonTotem(eff_idx);
                    else
                        DoSummonGuardian(eff_idx, summon_prop->FactionId);
                }
            }
            else
                DoSummonGuardian(eff_idx, summon_prop->FactionId);
            break;
        }
        case UNITNAME_SUMMON_TITLE_TOTEM:
        {
            DoSummonTotem(eff_idx, summon_prop->Slot);
            break;
        }
        case UNITNAME_SUMMON_TITLE_COMPANION:
        {
            DoSummonCritter(eff_idx, summon_prop->FactionId);
            break;
        }
        default:
            logging.error("EffectSummonType: Unhandled summon title %u",
                summon_prop->Title);
            break;
        }
        break;
    }
    case SUMMON_PROP_GROUP_PETS:
    {
        DoSummon(eff_idx);
        break;
    }
    case SUMMON_PROP_GROUP_CONTROLLABLE:
    {
        // no type here
        // maybe wrong - but thats the handler currently used for those
        DoSummonGuardian(eff_idx, summon_prop->FactionId);
        break;
    }
    default:
        logging.error("EffectSummonType: Unhandled summon group type %u",
            summon_prop->Group);
        break;
    }
}

void Spell::DoSummon(SpellEffectIndex eff_idx)
{
    // The check if we can summon happens in Spell::CheckCast
    // if we get here with an active pet already, just desummon it
    if (Pet* pet = m_caster->GetPet())
        pet->Unsummon(PET_SAVE_NOT_IN_SLOT);

    if (!unitTarget)
        return;

    uint32 pet_entry = m_spellInfo->EffectMiscValue[eff_idx];
    if (!pet_entry)
        return;

    CreatureInfo const* cInfo =
        sCreatureStorage.LookupEntry<CreatureInfo>(pet_entry);
    if (!cInfo)
    {
        logging.error(
            "Spell::DoSummon: creature entry %u not found for spell %u.",
            pet_entry, m_spellInfo->Id);
        return;
    }

    uint32 level = m_caster->getLevel();
    auto spawnCreature = new Pet(SUMMON_PET);

    if (m_caster->GetTypeId() == TYPEID_PLAYER &&
        spawnCreature->LoadPetFromDB((Player*)m_caster, pet_entry))
    {
        // Summon in dest location (Was Relocate() before, which makes it run
        // really fast to the location)
        if (m_targets.m_targetMask & TARGET_FLAG_DEST_LOCATION)
            spawnCreature->NearTeleportTo(m_targets.m_destX, m_targets.m_destY,
                m_targets.m_destZ, -m_caster->GetO());

        // set timer for unsummon
        if (m_duration > 0)
            spawnCreature->SetDuration(m_duration);

        summoned_target_ = spawnCreature->GetObjectGuid();

        return;
    }

    // Summon in dest location if flag dest location is set
    CreatureCreatePos pos(m_caster->GetMap(), m_targets.m_destX,
        m_targets.m_destY, m_targets.m_destZ, -m_caster->GetO());

    // Otherwise use caster as center, and radius of effect
    if (!(m_targets.m_targetMask & TARGET_FLAG_DEST_LOCATION))
    {
        float radius = GetSpellRadius(sSpellRadiusStore.LookupEntry(
            m_spellInfo->EffectRadiusIndex[eff_idx]));
        pos = CreatureCreatePos(
            m_caster, -m_caster->GetO(), radius, PET_FOLLOW_ANGLE);
    }

    Map* map = m_caster->GetMap();
    uint32 pet_number = sObjectMgr::Instance()->GeneratePetNumber();
    if (!spawnCreature->Create(
            map->GenerateLocalLowGuid(HIGHGUID_PET), pos, cInfo, pet_number))
    {
        logging.error(
            "Spell::EffectSummon: can't create creature with entry %u for "
            "spell %u",
            cInfo->Entry, m_spellInfo->Id);
        delete spawnCreature;
        return;
    }

    summoned_target_ = spawnCreature->GetObjectGuid();

    spawnCreature->SetSummonPoint(pos);

    // set timer for unsummon
    if (m_duration > 0)
        spawnCreature->SetDuration(m_duration);

    spawnCreature->SetOwnerGuid(m_caster->GetObjectGuid());
    spawnCreature->SetUInt32Value(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_NONE);
    spawnCreature->setPowerType(POWER_MANA);
    spawnCreature->setFaction(m_caster->getFaction());
    spawnCreature->SetUInt32Value(UNIT_FIELD_PET_NAME_TIMESTAMP, 0);
    spawnCreature->SetUInt32Value(UNIT_FIELD_PETEXPERIENCE, 0);
    spawnCreature->SetUInt32Value(UNIT_FIELD_PETNEXTLEVELEXP, 1000);
    spawnCreature->SetCreatorGuid(m_caster->GetObjectGuid());
    spawnCreature->SetUInt32Value(UNIT_CREATED_BY_SPELL, m_spellInfo->Id);

    if (!map->insert(spawnCreature))
    {
        delete spawnCreature;
        return;
    }

    auto caster_guid = m_caster->GetObjectGuid();
    auto orig_guid = m_originalCasterGUID;
    auto target_guid = m_targets.getUnitTargetGuid();
    auto id = m_spellInfo->Id;
    spawnCreature->queue_action(0, [caster_guid, orig_guid, target_guid, id,
                                       spawnCreature, level, pet_number]()
        {
            auto caster = spawnCreature->GetMap()->GetUnit(caster_guid);
            auto original_caster = spawnCreature->GetMap()->GetUnit(orig_guid);
            auto target = spawnCreature->GetMap()->GetUnit(target_guid);

            spawnCreature->AIM_Initialize();
            if (caster)
                caster->SetPet(spawnCreature);

            spawnCreature->InitStatsForLevel(level, caster);

            spawnCreature->GetCharmInfo()->SetPetNumber(pet_number, false);

            spawnCreature->InitPetCreateSpells();
            spawnCreature->SetHealth(spawnCreature->GetMaxHealth());
            spawnCreature->SetPower(
                POWER_MANA, spawnCreature->GetMaxPower(POWER_MANA));

            spawnCreature->init_pet_template_data();

            if (caster->GetTypeId() == TYPEID_PLAYER)
            {
                spawnCreature->SavePetToDB(PET_SAVE_AS_CURRENT);
                ((Player*)caster)->PetSpellInitialize();
            }

            if (caster && caster->GetTypeId() == TYPEID_UNIT &&
                ((Creature*)caster)->AI())
                ((Creature*)caster)
                    ->AI()
                    ->JustSummoned((Creature*)spawnCreature);
            if (original_caster && original_caster != caster &&
                original_caster->GetTypeId() == TYPEID_UNIT &&
                ((Creature*)original_caster)->AI())
                ((Creature*)original_caster)
                    ->AI()
                    ->JustSummoned((Creature*)spawnCreature);
            if (caster && spawnCreature->AI())
            {
                spawnCreature->AI()->SummonedBy(caster);
                if (original_caster && original_caster != caster)
                    spawnCreature->AI()->SummonedBy(caster);
            }

            // Shadowfiend starts attacking its FIRST target despite being
            // defensive by
            // default
            if (spawnCreature->GetEntry() == 19668 && target &&
                spawnCreature->behavior())
                spawnCreature->behavior()->attempt_attack(target);

            // Summon Water Elemental: Clear cooldown on Freeze
            if (caster && id == 31687 && caster->GetTypeId() == TYPEID_PLAYER)
                static_cast<Player*>(caster)->RemoveSpellCooldown(33395, true);
        });
}

void Spell::EffectLearnSpell(SpellEffectIndex eff_idx)
{
    if (!unitTarget)
        return;

    if (unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        if (m_caster->GetTypeId() == TYPEID_PLAYER)
            EffectLearnPetSpell(eff_idx);

        return;
    }

    Player* player = (Player*)unitTarget;

    uint32 spellToLearn = (m_spellInfo->Id == SPELL_ID_GENERIC_LEARN) ?
                              damage :
                              m_spellInfo->EffectTriggerSpell[eff_idx];
    player->learnSpell(spellToLearn, false);

    LOG_DEBUG(logging, "Spell: Player %u has learned spell %u from NpcGUID=%u",
        player->GetGUIDLow(), spellToLearn, m_caster->GetGUIDLow());
}

void Spell::EffectDispel(SpellEffectIndex eff_idx)
{
    if (!unitTarget)
        return;

    // Dispel should trigger combat
    if (unitTarget->IsHostileTo(m_caster))
    {
        m_caster->SetInCombatWith(unitTarget);
        unitTarget->SetInCombatWith(m_caster);
    }
    else
        m_caster->AdoptUnitCombatState(unitTarget);

    // Hostile Mass Dispel has SPELL_ATTR_UNAFFECTED_BY_INVULNERABILITY added to
    // it,
    // so that the trigger effect properly happens even if it was immuned
    if (m_spellInfo->Id == 32592 &&
        unitTarget->IsImmuneToSpell(
            m_spellInfo)) // IsImmuneToSpell does not check
                          // SPELL_ATTR_UNAFFECTED_BY_INVULNERABILITY
    {
        m_caster->SendSpellMiss(unitTarget, m_spellInfo->Id, SPELL_MISS_IMMUNE);
        return;
    }

    // Check for immunity to dispel
    auto& li = unitTarget->GetAurasByType(SPELL_AURA_SCHOOL_IMMUNITY);
    for (const auto& elem : li)
    {
        // MECHANIC_SHIELD is immune to hostile dispels
        if (!unitTarget->IsFriendlyTo(m_caster) &&
            (elem)->GetSpellProto()->Mechanic == MECHANIC_IMMUNE_SHIELD &&
            (elem)->GetMiscValue() & m_spellInfo->SchoolMask)
        {
            m_caster->SendSpellMiss(
                unitTarget, m_spellInfo->Id, SPELL_MISS_IMMUNE);
            return;
        }

        // MECHANIC_BANISH is immune to all dispels
        if ((elem)->GetSpellProto()->Mechanic == MECHANIC_BANISH)
        {
            m_caster->SendSpellMiss(
                unitTarget, m_spellInfo->Id, SPELL_MISS_IMMUNE);
            return;
        }
    }

    // Alarm-o-Bot intruder warning
    if (m_spellInfo->Id == 23002)
        m_caster->CastSpell(m_caster, 150038, true);

    DispelHelper(eff_idx, false);
}

void Spell::EffectDualWield(SpellEffectIndex /*eff_idx*/)
{
    if (unitTarget && unitTarget->GetTypeId() == TYPEID_PLAYER)
        ((Player*)unitTarget)->SetCanDualWield(true);
}

void Spell::EffectPull(SpellEffectIndex /*eff_idx*/)
{
    // TODO: create a proper pull towards distract spell center for distract
    LOG_DEBUG(logging, "WORLD: Spell Effect DUMMY");
}

void Spell::EffectDistract(SpellEffectIndex /*eff_idx*/)
{
    // Check for possible target
    if (!unitTarget || unitTarget->isInCombat())
        return;

    // target must be OK to do this
    if (unitTarget->hasUnitState(UNIT_STAT_CAN_NOT_REACT))
        return;

    unitTarget->clearUnitState(UNIT_STAT_MOVING);

    if (unitTarget->GetTypeId() == TYPEID_UNIT)
    {
        unitTarget->movement_gens.push(
            new movement::DistractMovementGenerator(damage * IN_MILLISECONDS),
            movement::EVENT_ENTER_COMBAT);
        if (auto grp = static_cast<Creature*>(unitTarget)->GetGroup())
        {
            unitTarget->GetMap()->GetCreatureGroupMgr().ProcessGroupEvent(
                grp->GetId(), CREATURE_GROUP_EVENT_MOVEMENT_PAUSE, nullptr,
                damage * IN_MILLISECONDS);
        }
    }

    unitTarget->SetFacingTo(
        unitTarget->GetAngle(m_targets.m_destX, m_targets.m_destY));
}

void Spell::EffectPickPocket(SpellEffectIndex /*eff_idx*/)
{
    if (m_caster->GetTypeId() != TYPEID_PLAYER)
        return;

    // victim must be creature and attackable
    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_UNIT ||
        m_caster->IsFriendlyTo(unitTarget))
        return;

    ((Player*)m_caster)
        ->SendLoot(unitTarget->GetObjectGuid(), LOOT_PICKPOCKETING);
}

void Spell::EffectAddFarsight(SpellEffectIndex eff_idx)
{
    if (m_caster->GetTypeId() != TYPEID_PLAYER)
        return;

    int32 duration = GetSpellDuration(m_spellInfo);
    auto dynObj = new DynamicObject;

    // set radius to 0: spell not expected to work as persistent aura
    if (!dynObj->Create(
            m_caster->GetMap()->GenerateLocalLowGuid(HIGHGUID_DYNAMICOBJECT),
            m_caster, m_spellInfo->Id, eff_idx, m_targets.m_destX,
            m_targets.m_destY, m_targets.m_destZ, duration, 0,
            DYNAMIC_OBJECT_FARSIGHT_FOCUS))
    {
        delete dynObj;
        return;
    }

    dynObj->SetActiveObjectState(true);

    if (!m_caster->GetMap()->insert(dynObj))
    {
        delete dynObj;
        return;
    }

    auto caster_guid = m_caster->GetObjectGuid();
    dynObj->queue_action(0, [dynObj, caster_guid]()
        {
            if (auto caster = dynObj->GetMap()->GetPlayer(caster_guid))
            {
                caster->AddDynObject(dynObj);
                ((Player*)caster)->GetCamera().SetView(dynObj);
            }
        });
}

void Spell::DoSummonWild(SpellEffectIndex eff_idx, uint32 forceFaction)
{
    uint32 creature_entry = m_spellInfo->EffectMiscValue[eff_idx];
    if (!creature_entry)
        return;

    int level;
    if (m_spellInfo->EffectMiscValueB[eff_idx] == 64)
        level = -1;
    else
        level = (int)m_caster->getLevel();

    // level of creature summoned using engineering item based at engineering
    // skill level
    /*if (m_caster->GetTypeId()==TYPEID_PLAYER && m_CastItem)
    {
        ItemPrototype const *proto = m_CastItem->GetProto();
        if (proto && proto->RequiredSkill == SKILL_ENGINEERING)
        {
            uint16 skill202 =
    ((Player*)m_caster)->GetSkillValue(SKILL_ENGINEERING);
            if (skill202)
                level = skill202/5;
        }
    }*/

    // select center of summon position
    float center_x = m_targets.m_destX;
    float center_y = m_targets.m_destY;
    float center_z = m_targets.m_destZ;

    float radius = GetSpellRadius(
        sSpellRadiusStore.LookupEntry(m_spellInfo->EffectRadiusIndex[eff_idx]));
    int32 duration = GetSpellDuration(m_spellInfo);
    if (duration < 0)
        duration = 0;
    TempSummonType summonType =
        (duration == 0) ? TEMPSUMMON_DEAD_DESPAWN : TEMPSUMMON_TIMED_DEATH;

    if (summonType == TEMPSUMMON_TIMED_DEATH)
    {
        // Don't kill unspecified types, mostly dummy invisible NPCs
        auto cinfo = ObjectMgr::GetCreatureTemplate(creature_entry);
        if (cinfo && cinfo->type == CREATURE_TYPE_NOT_SPECIFIED)
            summonType = TEMPSUMMON_TIMED_DESPAWN;
    }

    int32 amount = damage > 0 ? damage : 1;

    for (int32 count = 0; count < amount; ++count)
    {
        float px, py, pz;
        // If dest location if present
        if (m_targets.m_targetMask & TARGET_FLAG_DEST_LOCATION)
        {
            // Summon 1 unit in dest location
            if (count == 0)
            {
                px = m_targets.m_destX;
                py = m_targets.m_destY;
                pz = m_targets.m_destZ;
            }
            // Summon in random point all other units if location present
            else
            {
                auto pos = m_caster->GetPointXYZ(
                    G3D::Vector3(center_x, center_y, center_z),
                    2 * M_PI_F * rand_norm_f(), radius * rand_norm_f());
                px = pos.x;
                py = pos.y;
                pz = pos.z;
            }
        }
        // Summon if dest location not present near caster
        else
        {
            if (radius > 0.0f)
            {
                // not using bounding radius of caster here
                auto pos = m_caster->GetPoint(0.0f, radius, true);
                px = pos.x;
                py = pos.y;
                pz = pos.z;
            }
            else
            {
                // EffectRadiusIndex 0 or 36
                px = m_caster->GetX();
                py = m_caster->GetY();
                pz = m_caster->GetZ();
            }
        }

        if (Creature* summon = m_caster->SummonCreature(creature_entry, px, py,
                pz, m_caster->GetO(), summonType, duration, 0, level))
        {
            summon->SetUInt32Value(UNIT_CREATED_BY_SPELL, m_spellInfo->Id);

            // UNIT_FIELD_CREATEDBY are not set for these kind of spells.
            // Does exceptions exist? If so, what are they?
            // summon->SetCreatorGuid(m_caster->GetObjectGuid());

            if (forceFaction)
                summon->setFaction(forceFaction);

            auto id = m_spellInfo->Id;
            auto guid = m_caster->GetObjectGuid();
            auto orig_guid = m_originalCasterGUID;
            if (!(m_originalCaster && m_originalCaster != m_caster &&
                    m_originalCaster->GetTypeId() == TYPEID_UNIT))
                orig_guid.Clear();

            summon->queue_action(0, [id, summon, guid, orig_guid]()
                {
                    if (auto orig_caster =
                            summon->GetMap()->GetAnyTypeCreature(orig_guid))
                        if (orig_caster->AI())
                            orig_caster->AI()->JustSummoned(summon);
                });

            summoned_target_ = summon->GetObjectGuid();
        }
    }
}

void Spell::DoSummonGuardian(SpellEffectIndex eff_idx, uint32 forceFaction)
{
    uint32 pet_entry = m_spellInfo->EffectMiscValue[eff_idx];
    if (!pet_entry)
        return;

    CreatureInfo const* cInfo =
        sCreatureStorage.LookupEntry<CreatureInfo>(pet_entry);
    if (!cInfo)
    {
        logging.error(
            "Spell::DoSummonGuardian: creature entry %u not found for spell "
            "%u.",
            pet_entry, m_spellInfo->Id);
        return;
    }

    // in another case summon new
    uint32 level = m_caster->getLevel();

    // level of pet summoned using engineering item based at engineering skill
    // level
    if (m_caster->GetTypeId() == TYPEID_PLAYER && m_CastItem)
    {
        ItemPrototype const* proto = m_CastItem->GetProto();
        if (proto && proto->RequiredSkill == SKILL_ENGINEERING)
        {
            uint16 skill202 =
                ((Player*)m_caster)->GetSkillValue(SKILL_ENGINEERING);
            if (skill202)
            {
                level = skill202 / 5;
            }
        }
    }

    // select center of summon position
    float center_x = m_targets.m_destX;
    float center_y = m_targets.m_destY;
    float center_z = m_targets.m_destZ;

    float radius = GetSpellRadius(
        sSpellRadiusStore.LookupEntry(m_spellInfo->EffectRadiusIndex[eff_idx]));

    int32 amount = damage > 0 ? damage : 1;

    for (int32 count = 0; count < amount; ++count)
    {
        auto spawnCreature = new Pet(GUARDIAN_PET);

        // If dest location if present
        // Summon 1 unit in dest location
        CreatureCreatePos pos(m_caster->GetMap(), m_targets.m_destX,
            m_targets.m_destY, m_targets.m_destZ, -m_caster->GetO());

        if (m_targets.m_targetMask & TARGET_FLAG_DEST_LOCATION)
        {
            // Summon in random point all other units if location present
            if (count > 0)
            {
                auto p = m_caster->GetPointXYZ(
                    G3D::Vector3(center_x, center_y, center_z),
                    2 * M_PI_F * rand_norm_f(), radius * rand_norm_f());
                pos = CreatureCreatePos(
                    m_caster->GetMap(), p.x, p.y, p.z, m_caster->GetO());
            }
        }
        // Summon if dest location not present near caster
        else
            pos = CreatureCreatePos(m_caster, m_caster->GetO());

        Map* map = m_caster->GetMap();
        uint32 pet_number = sObjectMgr::Instance()->GeneratePetNumber();
        if (!spawnCreature->Create(map->GenerateLocalLowGuid(HIGHGUID_PET), pos,
                cInfo, pet_number))
        {
            logging.error(
                "Spell::DoSummonGuardian: can't create creature entry %u for "
                "spell %u.",
                pet_entry, m_spellInfo->Id);
            delete spawnCreature;
            return;
        }

        summoned_target_ = spawnCreature->GetObjectGuid();

        spawnCreature->SetSummonPoint(pos);

        if (m_duration > 0)
            spawnCreature->SetDuration(m_duration);

        // spawnCreature->SetName("");                       // generated by
        // client
        spawnCreature->SetOwnerGuid(m_caster->GetObjectGuid());
        spawnCreature->setPowerType(POWER_MANA);
        spawnCreature->SetUInt32Value(
            UNIT_NPC_FLAGS, spawnCreature->GetCreatureInfo()->npcflag);
        spawnCreature->setFaction(
            forceFaction ? forceFaction : m_caster->getFaction());
        spawnCreature->SetUInt32Value(UNIT_FIELD_PET_NAME_TIMESTAMP, 0);
        spawnCreature->SetCreatorGuid(m_caster->GetObjectGuid());
        spawnCreature->SetUInt32Value(UNIT_CREATED_BY_SPELL, m_spellInfo->Id);
        if (m_caster->GetCharmerOrOwnerPlayerOrPlayerItself() != nullptr)
            spawnCreature->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PVP_ATTACKABLE);

        if (!map->insert(spawnCreature))
        {
            delete spawnCreature;
            return;
        }

        spawnCreature->InitStatsForLevel(level, m_caster);

        auto caster_guid = m_caster->GetObjectGuid();
        auto orig_guid = m_caster->GetObjectGuid();
        spawnCreature->queue_action(0,
            [spawnCreature, caster_guid, orig_guid, level, pet_number]()
            {
                auto caster = spawnCreature->GetMap()->GetUnit(caster_guid);
                auto original_caster =
                    spawnCreature->GetMap()->GetUnit(orig_guid);
                spawnCreature->AIM_Initialize();

                spawnCreature->GetCharmInfo()->SetPetNumber(pet_number, false);
                spawnCreature->InitPetCreateSpells();

                spawnCreature->init_pet_template_data();

                if (!caster)
                    return;

                caster->AddGuardian(spawnCreature);

                // Notify Summoner
                if (caster->GetTypeId() == TYPEID_UNIT &&
                    ((Creature*)caster)->AI())
                    ((Creature*)caster)->AI()->JustSummoned(spawnCreature);
                if (original_caster && original_caster != caster &&
                    original_caster->GetTypeId() == TYPEID_UNIT &&
                    ((Creature*)original_caster)->AI())
                    ((Creature*)original_caster)
                        ->AI()
                        ->JustSummoned(spawnCreature);
                if (spawnCreature->AI())
                {
                    spawnCreature->AI()->SummonedBy(caster);
                    if (original_caster && original_caster != caster)
                        spawnCreature->AI()->SummonedBy(caster);
                }

                // Add as combat summon
                if (caster->GetTypeId() == TYPEID_UNIT && caster->isInCombat())
                {
                    static_cast<Creature*>(caster)->combat_summons.push_back(
                        spawnCreature->GetObjectGuid());
                    if (auto target = caster->getVictim())
                    {
                        if (spawnCreature->CanStartAttacking(target))
                            spawnCreature->AI()->AttackedBy(target);
                    }
                }
            });
    }
}

void Spell::EffectTeleUnitsFaceCaster(SpellEffectIndex eff_idx)
{
    if (!unitTarget || unitTarget->IsTaxiFlying() || unitTarget == m_caster)
        return;

    float dis = GetSpellRadius(
        sSpellRadiusStore.LookupEntry(m_spellInfo->EffectRadiusIndex[eff_idx]));

    auto pos = m_caster->GetPoint(
        unitTarget, dis, unitTarget->GetTypeId() == TYPEID_PLAYER);
    float o = unitTarget->GetAngle(m_caster);

    unitTarget->NearTeleportTo(pos.x, pos.y, pos.z, o, false);
}

void Spell::EffectLearnSkill(SpellEffectIndex eff_idx)
{
    if (unitTarget->GetTypeId() != TYPEID_PLAYER)
        return;

    if (damage < 0)
        return;

    uint32 skillid = m_spellInfo->EffectMiscValue[eff_idx];
    uint16 skillval = ((Player*)unitTarget)->GetPureSkillValue(skillid);
    ((Player*)unitTarget)
        ->SetSkill(skillid, skillval ? skillval : 1, damage * 75, damage);
}

void Spell::EffectAddHonor(SpellEffectIndex /*eff_idx*/)
{
    if (unitTarget->GetTypeId() != TYPEID_PLAYER)
        return;

    // 2.4.3 honor-spells don't scale with level and won't be casted by an item
    // also we must use damage+1 (spelldescription says +25 honor but damage is
    // only 24)
    ((Player*)unitTarget)->RewardHonor(nullptr, 1, float(damage + 1));
    LOG_DEBUG(logging,
        "SpellEffect::AddHonor (spell_id %u) rewards %u honor points (non "
        "scale) for player: %u",
        m_spellInfo->Id, damage, ((Player*)unitTarget)->GetGUIDLow());
}

void Spell::EffectTradeSkill(SpellEffectIndex /*eff_idx*/)
{
    if (unitTarget->GetTypeId() != TYPEID_PLAYER)
        return;
    // uint32 skillid =  m_spellInfo->EffectMiscValue[i];
    // uint16 skillmax = ((Player*)unitTarget)->(skillid);
    // ((Player*)unitTarget)->SetSkill(skillid,skillval?skillval:1,skillmax+75);
}

void Spell::EffectEnchantItemPerm(SpellEffectIndex eff_idx)
{
    if (m_caster->GetTypeId() != TYPEID_PLAYER)
        return;
    if (!itemTarget)
        return;

    Player* p_caster = (Player*)m_caster;

    // not grow at item use at item case
    p_caster->UpdateCraftSkill(m_spellInfo->Id);

    uint32 enchant_id = m_spellInfo->EffectMiscValue[eff_idx];
    if (!enchant_id)
        return;

    SpellItemEnchantmentEntry const* pEnchant =
        sSpellItemEnchantmentStore.LookupEntry(enchant_id);
    if (!pEnchant)
        return;

    // item can be in trade slot and have owner diff. from caster
    Player* item_owner = itemTarget->GetOwner();
    if (!item_owner)
        return;

    if (item_owner != p_caster &&
        p_caster->GetSession()->GetSecurity() > SEC_PLAYER &&
        sWorld::Instance()->getConfig(CONFIG_BOOL_GM_LOG_TRADE))
    {
        gm_logger.info(
            "GM %s (Account: %u) enchanting(perm): %s (Entry: %d) for player: "
            "%s (Account: %u)",
            p_caster->GetName(), p_caster->GetSession()->GetAccountId(),
            itemTarget->GetProto()->Name1, itemTarget->GetEntry(),
            item_owner->GetName(), item_owner->GetSession()->GetAccountId());
    }

    // remove old enchanting before applying new if equipped
    if (itemTarget->IsEquipped())
        item_owner->ApplyEnchantment(
            itemTarget, PERM_ENCHANTMENT_SLOT, false, itemTarget->slot());

    itemTarget->SetEnchantment(PERM_ENCHANTMENT_SLOT, enchant_id, 0, 0);

    // add new enchanting if equipped
    if (itemTarget->IsEquipped())
        item_owner->ApplyEnchantment(
            itemTarget, PERM_ENCHANTMENT_SLOT, true, itemTarget->slot());
}

void Spell::EffectEnchantItemTmp(SpellEffectIndex eff_idx)
{
    if (m_caster->GetTypeId() != TYPEID_PLAYER)
        return;
    if (!itemTarget)
        return;

    Player* p_caster = (Player*)m_caster;

    uint32 enchant_id = m_spellInfo->EffectMiscValue[eff_idx];

    // Shaman Rockbiter Weapon
    if (eff_idx == EFFECT_INDEX_0 &&
        m_spellInfo->Effect[EFFECT_INDEX_1] == SPELL_EFFECT_DUMMY)
    {
        int32 enchanting_damage = m_currentBasePoints[EFFECT_INDEX_1];

        // enchanting id selected by calculated damage-per-sec stored in
        // Effect[1] base value
        // with already applied percent bonus from Elemental Weapons talent
        // Note: damage calculated (correctly) with rounding int32(float(v)) but
        // RW enchantments applied damage int32(float(v)+0.5), this create  0..1
        // difference sometime
        switch (enchanting_damage)
        {
        // Rank 1
        case 2:
            enchant_id = 29;
            break; //  0% [ 7% ==  2, 14% == 2, 20% == 2]
        // Rank 2
        case 4:
            enchant_id = 6;
            break; //  0% [ 7% ==  4, 14% == 4]
        case 5:
            enchant_id = 3025;
            break; // 20%
        // Rank 3
        case 6:
            enchant_id = 1;
            break; //  0% [ 7% ==  6, 14% == 6]
        case 7:
            enchant_id = 3027;
            break; // 20%
        // Rank 4
        case 9:
            enchant_id = 3032;
            break; //  0% [ 7% ==  6]
        case 10:
            enchant_id = 503;
            break; // 14%
        case 11:
            enchant_id = 3031;
            break; // 20%
        // Rank 5
        case 15:
            enchant_id = 3035;
            break; // 0%
        case 16:
            enchant_id = 1663;
            break; // 7%
        case 17:
            enchant_id = 3033;
            break; // 14%
        case 18:
            enchant_id = 3034;
            break; // 20%
        // Rank 6
        case 28:
            enchant_id = 3038;
            break; // 0%
        case 29:
            enchant_id = 683;
            break; // 7%
        case 31:
            enchant_id = 3036;
            break; // 14%
        case 33:
            enchant_id = 3037;
            break; // 20%
        // Rank 7
        case 40:
            enchant_id = 3041;
            break; // 0%
        case 42:
            enchant_id = 1664;
            break; // 7%
        case 45:
            enchant_id = 3039;
            break; // 14%
        case 48:
            enchant_id = 3040;
            break; // 20%
        // Rank 8
        case 49:
            enchant_id = 3044;
            break; // 0%
        case 52:
            enchant_id = 2632;
            break; // 7%
        case 55:
            enchant_id = 3042;
            break; // 14%
        case 58:
            enchant_id = 3043;
            break; // 20%
        // Rank 9
        case 62:
            enchant_id = 2633;
            break; // 0%
        case 66:
            enchant_id = 3018;
            break; // 7%
        case 70:
            enchant_id = 3019;
            break; // 14%
        case 74:
            enchant_id = 3020;
            break; // 20%
        default:
            logging.error(
                "Spell::EffectEnchantItemTmp: Damage %u not handled in S'RW",
                enchanting_damage);
            return;
        }
    }

    if (!enchant_id)
    {
        logging.error(
            "Spell %u Effect %u (SPELL_EFFECT_ENCHANT_ITEM_TEMPORARY) have 0 "
            "as enchanting id",
            m_spellInfo->Id, eff_idx);
        return;
    }

    SpellItemEnchantmentEntry const* pEnchant =
        sSpellItemEnchantmentStore.LookupEntry(enchant_id);
    if (!pEnchant)
    {
        logging.error(
            "Spell %u Effect %u (SPELL_EFFECT_ENCHANT_ITEM_TEMPORARY) have "
            "nonexistent enchanting id %u ",
            m_spellInfo->Id, eff_idx, enchant_id);
        return;
    }

    // select enchantment duration
    uint32 duration;

    // rogue family enchantments exception by duration
    if (m_spellInfo->Id == 38615)
        duration = 1800; // 30 mins
    // other rogue family enchantments always 1 hour (some have spell damage=0,
    // but some have wrong data in EffBasePoints)
    else if (m_spellInfo->SpellFamilyName == SPELLFAMILY_ROGUE)
        duration = 3600; // 1 hour
    // shaman family enchantments
    else if (m_spellInfo->SpellFamilyName == SPELLFAMILY_SHAMAN)
        duration = 1800; // 30 mins
    // other cases with this SpellVisual already selected
    else if (m_spellInfo->SpellVisual == 215)
        duration = 1800; // 30 mins
    // some fishing pole bonuses
    else if (m_spellInfo->SpellVisual == 563)
        duration = 600; // 10 mins
    // shaman rockbiter enchantments
    else if (m_spellInfo->SpellVisual == 0)
        duration = 1800; // 30 mins
    else if (m_spellInfo->Id == 29702)
        duration = 300; // 5 mins
    else if (m_spellInfo->Id == 37360)
        duration = 300; // 5 mins
    // default case
    else
        duration = 3600; // 1 hour

    // item can be in trade slot and have owner diff. from caster
    Player* item_owner = itemTarget->GetOwner();
    if (!item_owner)
        return;

    if (item_owner != p_caster &&
        p_caster->GetSession()->GetSecurity() > SEC_PLAYER &&
        sWorld::Instance()->getConfig(CONFIG_BOOL_GM_LOG_TRADE))
    {
        gm_logger.info(
            "GM %s (Account: %u) enchanting(temp): %s (Entry: %d) for player: "
            "%s (Account: %u)",
            p_caster->GetName(), p_caster->GetSession()->GetAccountId(),
            itemTarget->GetProto()->Name1, itemTarget->GetEntry(),
            item_owner->GetName(), item_owner->GetSession()->GetAccountId());
    }

    // remove old enchanting before applying new if equipped
    if (itemTarget->IsEquipped())
        item_owner->ApplyEnchantment(
            itemTarget, TEMP_ENCHANTMENT_SLOT, false, itemTarget->slot());

    itemTarget->SetEnchantment(
        TEMP_ENCHANTMENT_SLOT, enchant_id, duration * 1000, 0);

    // add new enchanting if equipped
    if (itemTarget->IsEquipped())
        item_owner->ApplyEnchantment(
            itemTarget, TEMP_ENCHANTMENT_SLOT, true, itemTarget->slot());
}

void Spell::EffectTameCreature(SpellEffectIndex /*eff_idx*/)
{
    auto caster = GetAffectiveCaster();
    if (!caster || caster->GetTypeId() != TYPEID_PLAYER)
        return;
    auto plr = static_cast<Player*>(caster);

    Creature* creatureTarget = (Creature*)unitTarget;

    // cast finish successfully
    // SendChannelUpdate(0);
    finish();

    // "kill" original creature
    creatureTarget->ForcedDespawn();

    auto pet = new Pet(HUNTER_PET);

    if (!pet->CreateBaseAtCreature(creatureTarget))
    {
        delete pet;
        return;
    }

    pet->SetOwnerGuid(plr->GetObjectGuid());
    pet->SetCreatorGuid(plr->GetObjectGuid());
    pet->setFaction(plr->getFaction());
    pet->SetUInt32Value(UNIT_CREATED_BY_SPELL, m_spellInfo->Id);

    if (!plr->GetMap()->insert(pet))
    {
        delete pet;
        return;
    }

    if (plr->IsPvP())
        pet->SetPvP(true);

    if (!pet->InitStatsForLevel(creatureTarget->getLevel()))
    {
        logging.error(
            "Pet::InitStatsForLevel() failed for creature (Entry: %u)!",
            creatureTarget->GetEntry());
        plr->GetMap()->erase(pet, true);
        return;
    }

    uint32 ctlevel = creatureTarget->getLevel();
    auto caster_guid = plr->GetObjectGuid();
    pet->queue_action(0, [pet, caster_guid, ctlevel]()
        {
            auto caster = pet->GetMap()->GetPlayer(caster_guid);
            if (!caster)
                return;

            pet->GetCharmInfo()->SetPetNumber(
                sObjectMgr::Instance()->GeneratePetNumber(), true);
            // this enables pet details window (Shift+P)
            pet->AIM_Initialize();
            pet->InitPetCreateSpells();
            pet->SetHealth(pet->GetMaxHealth());

            // prepare visual effect for levelup
            pet->SetUInt32Value(UNIT_FIELD_LEVEL, ctlevel - 1);

            // visual effect for levelup
            pet->SetUInt32Value(UNIT_FIELD_LEVEL, ctlevel);

            pet->init_pet_template_data();

            // caster have pet now
            caster->SetPet(pet);

            pet->SavePetToDB(PET_SAVE_AS_CURRENT);
            caster->PetSpellInitialize();
        });
}

void Spell::EffectSummonPet(SpellEffectIndex eff_idx)
{
    uint32 petentry = m_spellInfo->EffectMiscValue[eff_idx];

    if (m_caster->GetTypeId() == TYPEID_PLAYER && m_spellInfo->Id == 883 &&
        ((Player*)m_caster)->HasDeadPet())
    {
        SendCastResult(SPELL_FAILED_TARGETS_DEAD);
        return;
    }

    Pet* OldSummon = m_caster->GetPet();

    // Already in the process of summoning a pet
    if (m_caster->GetPetGuid() && !OldSummon)
    {
        SendCastResult(SPELL_FAILED_ALREADY_HAVE_SUMMON);
        return;
    }

    if (OldSummon)
    {
        // Player resummoning the same pet? Needs special handling
        if (m_caster->GetTypeId() == TYPEID_PLAYER &&
            (petentry == 0 || OldSummon->GetEntry() == petentry))
        {
            // Clear cooldowns; the new pet should have his CDs reset
            if (m_caster->GetMap()->IsBattleArena())
            {
                OldSummon->m_CreatureSpellCooldowns.clear();
                OldSummon->m_CreatureCategoryCooldowns.clear();
            }
            // Remove old instance of this pet
            OldSummon->Unsummon(PET_SAVE_AS_CURRENT);
            // Remove caster's demonic sacrifice
            m_caster->remove_auras(SPELL_AURA_OVERRIDE_CLASS_SCRIPTS,
                [](AuraHolder* holder)
                {
                    return holder->GetSpellProto()->EffectMiscValue
                               [holder->GetSpellProto()->GetIndexForAura(
                                   SPELL_AURA_OVERRIDE_CLASS_SCRIPTS)] == 2228;
                });

            // Summon a new instance of the same pet
            auto summon = new Pet;
            if (!summon->LoadPetFromDB((Player*)m_caster, petentry))
            {
                delete summon;
                return;
            }
            // All done
            return;
        }
        // Otherwise, just unsummon the old pet
        OldSummon->Unsummon(OldSummon->getPetType() == HUNTER_PET ?
                                PET_SAVE_AS_DELETED :
                                PET_SAVE_DISMISS_PET,
            m_caster);
        OldSummon = nullptr;
    }

    CreatureInfo const* cInfo =
        petentry ? sCreatureStorage.LookupEntry<CreatureInfo>(petentry) :
                   nullptr;

    // == 0 in case call current pet, check only real summon case
    if (petentry && !cInfo)
    {
        logging.error(
            "EffectSummonPet: creature entry %u not found for spell %u.",
            petentry, m_spellInfo->Id);
        return;
    }

    auto NewSummon = new Pet;

    if (m_caster->GetTypeId() == TYPEID_PLAYER &&
        NewSummon->LoadPetFromDB((Player*)m_caster, petentry))
    {
        // remove demonic sacrifice
        m_caster->remove_auras(SPELL_AURA_OVERRIDE_CLASS_SCRIPTS,
            [](AuraHolder* holder)
            {
                return holder->GetSpellProto()->EffectMiscValue
                           [holder->GetSpellProto()->GetIndexForAura(
                               SPELL_AURA_OVERRIDE_CLASS_SCRIPTS)] == 2228;
            });

        if (m_caster->GetMap()->IsBattleArena())
        {
            NewSummon->m_CreatureSpellCooldowns.clear();
            NewSummon->m_CreatureCategoryCooldowns.clear();
        }

        // Pet loaded successsfully from DB, we're now done with the summoning
        return;
    }

    // No pet entry exists in the database, everything that follows here is code
    // for creating a new pet:
    // (should be put in a seperate function, to be quite frank)

    // not error in case fail hunter call pet
    if (!petentry)
    {
        delete NewSummon;
        return;
    }

    CreatureCreatePos pos(
        m_caster, m_caster->GetO(), PET_FOLLOW_DIST, PET_FOLLOW_ANGLE);

    Map* map = m_caster->GetMap();
    uint32 pet_number = sObjectMgr::Instance()->GeneratePetNumber();
    if (!NewSummon->Create(
            map->GenerateLocalLowGuid(HIGHGUID_PET), pos, cInfo, pet_number))
    {
        delete NewSummon;
        return;
    }

    NewSummon->SetSummonPoint(pos);

    uint32 petlevel = m_caster->getLevel();
    NewSummon->setPetType(SUMMON_PET);

    uint32 faction = m_caster->getFaction();

    NewSummon->SetOwnerGuid(m_caster->GetObjectGuid());
    NewSummon->SetCreatorGuid(m_caster->GetObjectGuid());
    NewSummon->SetUInt32Value(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_NONE);
    NewSummon->setFaction(faction);
    NewSummon->SetUInt32Value(
        UNIT_FIELD_PET_NAME_TIMESTAMP, uint32(WorldTimer::time_no_syscall()));
    NewSummon->SetUInt32Value(UNIT_FIELD_PETEXPERIENCE, 0);
    NewSummon->SetUInt32Value(UNIT_FIELD_PETNEXTLEVELEXP, 1000);
    NewSummon->SetUInt32Value(UNIT_CREATED_BY_SPELL, m_spellInfo->Id);

    if (!map->insert(NewSummon))
    {
        delete NewSummon;
        return;
    }

    auto caster_guid = m_caster->GetObjectGuid();
    NewSummon->queue_action(0,
        [NewSummon, caster_guid, pet_number, petlevel, petentry]()
        {
            auto caster = NewSummon->GetMap()->GetUnit(caster_guid);
            if (!caster)
                return;

            NewSummon->GetCharmInfo()->SetPetNumber(pet_number, true);
            // this enables pet details window (Shift+P)

            if (caster->IsPvP())
                NewSummon->SetPvP(true);

            NewSummon->InitStatsForLevel(petlevel, caster);
            NewSummon->InitPetCreateSpells();

            if (NewSummon->getPetType() == SUMMON_PET)
            {
                // Remove Demonic Sacrifice auras (new pet)
                auto& auraClassScripts =
                    caster->GetAurasByType(SPELL_AURA_OVERRIDE_CLASS_SCRIPTS);
                for (auto itr = auraClassScripts.begin();
                     itr != auraClassScripts.end();)
                {
                    if ((*itr)->GetModifier()->m_miscvalue == 2228)
                    {
                        caster->remove_auras((*itr)->GetId());
                        itr = auraClassScripts.begin();
                    }
                    else
                        ++itr;
                }
            }

            if (caster->GetTypeId() == TYPEID_PLAYER &&
                NewSummon->getPetType() == SUMMON_PET)
            {
                // generate new name for summon pet
                std::string new_name =
                    sObjectMgr::Instance()->GeneratePetName(petentry);
                if (!new_name.empty())
                    NewSummon->SetName(new_name);
            }

            if (NewSummon->getPetType() == HUNTER_PET)
            {
                NewSummon->RemoveByteFlag(
                    UNIT_FIELD_BYTES_2, 2, UNIT_CAN_BE_RENAMED);
                NewSummon->SetByteFlag(
                    UNIT_FIELD_BYTES_2, 2, UNIT_CAN_BE_ABANDONED);
            }

            NewSummon->AIM_Initialize();
            NewSummon->SetHealth(NewSummon->GetMaxHealth());
            NewSummon->SetPower(POWER_MANA, NewSummon->GetMaxPower(POWER_MANA));

            NewSummon->init_pet_template_data();

            caster->SetPet(NewSummon);
            LOG_DEBUG(logging, "New Pet has guid %u", NewSummon->GetGUIDLow());

            if (caster->GetTypeId() == TYPEID_PLAYER)
            {
                NewSummon->SavePetToDB(PET_SAVE_AS_CURRENT);
                ((Player*)caster)->PetSpellInitialize();
            }
        });
}

void Spell::EffectLearnPetSpell(SpellEffectIndex eff_idx)
{
    if (m_caster->GetTypeId() != TYPEID_PLAYER)
        return;

    Player* _player = (Player*)m_caster;

    Pet* pet = _player->GetPet();
    if (!pet)
        return;
    if (!pet->isAlive())
        return;

    SpellEntry const* learn_spellproto =
        sSpellStore.LookupEntry(m_spellInfo->EffectTriggerSpell[eff_idx]);
    if (!learn_spellproto)
        return;

    pet->SetTP(
        pet->m_TrainingPoints - pet->GetTPForSpell(learn_spellproto->Id));
    pet->learnSpell(learn_spellproto->Id);

    pet->SavePetToDB(PET_SAVE_AS_CURRENT);
    _player->PetSpellInitialize();
}

void Spell::EffectTaunt(SpellEffectIndex /*eff_idx*/)
{
    if (!unitTarget)
        return;

    // this effect use before aura Taunt apply for prevent taunt already
    // attacking target
    // for spell as marked "non effective at already attacking target"
    if (unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        if (unitTarget->getVictim() == m_caster)
        {
            SendCastResult(SPELL_FAILED_DONT_REPORT);
            return;
        }
    }

    // We do a proper threat transfer & target switch for EffectTaunt
    unitTarget->getThreatManager().tauntTransferAggro(m_caster);
}

void Spell::EffectWeaponDmg(SpellEffectIndex eff_idx)
{
    if (!unitTarget)
        return;
    if (!unitTarget->isAlive())
        return;

    // multiple weapon dmg effect workaround
    // execute only the last weapon damage
    // and handle all effects at once
    for (int j = 0; j < MAX_EFFECT_INDEX; ++j)
    {
        switch (m_spellInfo->Effect[j])
        {
        case SPELL_EFFECT_WEAPON_DAMAGE:
        case SPELL_EFFECT_WEAPON_DAMAGE_NOSCHOOL:
        case SPELL_EFFECT_NORMALIZED_WEAPON_DMG:
        case SPELL_EFFECT_WEAPON_PERCENT_DAMAGE:
            if (j <
                int(eff_idx)) // we must calculate only at last weapon effect
                return;
            break;
        }
    }

    // some spell specific modifiers
    bool customBonusDamagePercentMod = false;
    float bonusDamagePercentMod = 1.0f;  // applied to fixed effect damage bonus
                                         // if set customBonusDamagePercentMod
    float weaponDamagePercentMod = 1.0f; // applied to weapon damage (and to
                                         // fixed effect damage bonus if
                                         // customBonusDamagePercentMod not set
    float totalDamagePercentMod = 1.0f;  // applied to final bonus+weapon damage
    float normalizedMultiplier =
        1.0f; // Multiplied to the result of normalized weapon damage
    bool normalized = false;
    int32 flat_bonus = 0; // applied after weapon mods, used for shaman's WF

    int32 spell_bonus = 0; // bonus specific for spell

    switch (m_spellInfo->SpellFamilyName)
    {
    case SPELLFAMILY_WARRIOR:
    {
        // Devastate
        if (m_spellInfo->SpellVisual == 671 && m_spellInfo->SpellIconID == 1508)
        {
            customBonusDamagePercentMod = true;
            normalizedMultiplier = 0.0f; // only applied if auras found

            // Sunder Armor
            AuraHolder* sunder = unitTarget->get_aura(SPELL_AURA_MOD_RESISTANCE,
                m_caster->GetObjectGuid(), [](AuraHolder* h)
                {
                    return h->GetSpellProto()->SpellFamilyName ==
                               SPELLFAMILY_WARRIOR &&
                           h->GetSpellProto()->SpellFamilyFlags & 0x4000;
                });

            // Devastate bonus damage
            if (sunder)
            {
                // We double for each found stack
                normalizedMultiplier = 1.0f * sunder->GetStackAmount();
            }

            // Get highest learned sunder armor
            uint32 sunderid = 0;
            if (m_caster->HasSpell(25225))
                sunderid = 25225;
            else if (m_caster->HasSpell(11597))
                sunderid = 11597;
            else if (m_caster->HasSpell(11596))
                sunderid = 11596;
            else if (m_caster->HasSpell(8380))
                sunderid = 8380;
            else if (m_caster->HasSpell(7405))
                sunderid = 7405;
            else if (m_caster->HasSpell(7386))
                sunderid = 7386;

            if (!sunderid)
                return;

            const SpellEntry* sunderProt = sSpellStore.LookupEntry(sunderid);
            if (sunderProt)
            {
                if (sunder)
                {
                    if (sunder->GetStackAmount() < sunderProt->StackAmount)
                        sunder->SetStackAmount(sunder->GetStackAmount() + 1);
                    else
                        sunder->RefreshHolder();
                }
                else
                    unitTarget->AddAuraThroughNewHolder(sunderid, m_caster);

                // Apply the threat from sunder armor as well (we do not get
                // that provided for us, since we only mod the aura)
                if (unitTarget->GetTypeId() == TYPEID_UNIT)
                {
                    if (const SpellThreatEntry* threat =
                            sSpellMgr::Instance()->GetSpellThreatEntry(
                                sunderid))
                        unitTarget->AddThreat(m_caster,
                            threat->threat * threat->multiplier, false,
                            SPELL_SCHOOL_MASK_NORMAL, sunderProt);
                }
            }
        }
        // Heroic Strike Rank 10 & 11
        else if (m_spellInfo->Id == 29707 || m_spellInfo->Id == 30324)
        {
            // "Causes {62,73} additional damage against Dazed targets."
            bool dazed = false;
            unitTarget->loop_auras([&dazed](AuraHolder* holder)
                {
                    if (holder->GetSpellProto()->HasAttribute(
                            SPELL_ATTR_CUSTOM_DAZE_EFFECT))
                        dazed = true;
                    return !dazed; // break when dazed is true
                });
            if (dazed)
                spell_bonus +=
                    m_spellInfo->Id == 29707 ? 62 : 73; // r10: +62, r11: +73
        }

        break;
    }
    case SPELLFAMILY_ROGUE:
    {
        // Ambush
        if (m_spellInfo->SpellFamilyFlags & UI64LIT(0x00000200))
        {
            customBonusDamagePercentMod = true;
            bonusDamagePercentMod = 2.5f; // 250%
        }
        // Mutilate (for each hand)
        else if (m_spellInfo->SpellFamilyFlags & UI64LIT(0x600000000))
        {
            bool found = false;
            // fast check
            if (unitTarget->HasAuraState(AURA_STATE_DEADLY_POISON))
                found = true;
            // full aura scan
            else
            {
                unitTarget->loop_auras([&found](AuraHolder* holder)
                    {
                        if (holder->GetSpellProto()->Dispel == DISPEL_POISON)
                            found = true;
                        return !found; // break when found is true
                    });
            }

            if (found)
                totalDamagePercentMod *= 1.5f; // 150% if poisoned
        }
        // Throw
        else if (m_spellInfo->Id == 2764)
        {
            if (m_caster->GetTypeId() == TYPEID_PLAYER)
                static_cast<Player*>(m_caster)->weap_dura_loss(RANGED_ATTACK);
        }
        break;
    }
    case SPELLFAMILY_PALADIN:
    {
        // Seal of Command - receive benefit from spellpower & holy power (which
        // exists more of if target is judged by crusader, e.g.)
        // NOTE: In patch 1.9 the following is done: "Seal of Command - Damage
        // bonus from +Holy damage items slightly increased."
        // SoC benefits 20% from spell power, and 29% from extra holy power
        if (m_spellInfo->SpellFamilyFlags & 0x2000000)
        {
            int32 sp = m_caster->SpellBaseDamageBonusDone(
                           SPELL_SCHOOL_MASK_HOLY, m_spellInfo->Id, false) +
                       unitTarget->SpellBaseDamageBonusTaken(
                           SPELL_SCHOOL_MASK_HOLY, false);
            int32 holy = m_caster->SpellBaseDamageBonusDone(
                             SPELL_SCHOOL_MASK_HOLY, m_spellInfo->Id, true) +
                         unitTarget->SpellBaseDamageBonusTaken(
                             SPELL_SCHOOL_MASK_HOLY, true);

            sp -= holy;
            if (sp < 0)
                sp = 0;

            spell_bonus += int32(0.2 * sp + 0.29 * holy);
        }

        // Crusader Strike: refresh duration of all judgements on the target
        if (m_spellInfo->SpellFamilyFlags & 0x800000000000)
        {
            unitTarget->loop_auras([this](AuraHolder* holder)
                {
                    const SpellEntry* info = holder->GetSpellProto();
                    if (info->AttributesEx3 & SPELL_ATTR_EX3_CANT_MISS &&
                        info->SpellFamilyName == SPELLFAMILY_PALADIN)
                        holder->RefreshHolder();
                    return true; // continue
                });
        }
        break;
    }
    case SPELLFAMILY_SHAMAN:
    {
        // Skyshatter Harness item set bonus
        // Stormstrike
        if (m_spellInfo->SpellFamilyFlags & UI64LIT(0x001000000000))
        {
            auto& m_OverrideClassScript =
                m_caster->GetAurasByType(SPELL_AURA_OVERRIDE_CLASS_SCRIPTS);
            for (const auto& elem : m_OverrideClassScript)
            {
                // Stormstrike AP Buff
                if ((elem)->GetModifier()->m_miscvalue == 5634)
                {
                    m_caster->CastSpell(m_caster, 38430, true, nullptr, elem);
                    break;
                }
            }
        }
        // Shaman's Windfury Weapon, apply AP bonus flatly (base points is AP
        // bonus, set in UnitAuraProcHandler.cpp)
        if (m_spellInfo->Id == 25504 || m_spellInfo->Id == 33750)
        {
            flat_bonus = m_currentBasePoints[0];
            m_currentBasePoints[0] = 0;
        }
        break;
    }
    }

    int32 fixed_bonus = 0;
    for (int j = 0; j < MAX_EFFECT_INDEX; ++j)
    {
        switch (m_spellInfo->Effect[j])
        {
        case SPELL_EFFECT_WEAPON_DAMAGE:
        case SPELL_EFFECT_WEAPON_DAMAGE_NOSCHOOL:
            fixed_bonus += CalculateDamage(SpellEffectIndex(j), unitTarget);
            break;
        case SPELL_EFFECT_NORMALIZED_WEAPON_DMG:
            fixed_bonus += normalizedMultiplier *
                           CalculateDamage(SpellEffectIndex(j), unitTarget);
            normalized = true;
            break;
        case SPELL_EFFECT_WEAPON_PERCENT_DAMAGE:
            weaponDamagePercentMod *=
                float(CalculateDamage(SpellEffectIndex(j), unitTarget)) /
                100.0f;

            // applied only to prev.effects fixed damage
            if (customBonusDamagePercentMod)
                fixed_bonus = int32(fixed_bonus * bonusDamagePercentMod);
            else
                fixed_bonus = int32(fixed_bonus * weaponDamagePercentMod);
            break;
        default:
            break; // not weapon damage effect, just skip
        }
    }

    // non-weapon damage
    int32 bonus = spell_bonus + fixed_bonus;

    // apply to non-weapon bonus weapon total pct effect, weapon total flat
    // effect included in weapon damage
    if (bonus)
    {
        UnitMods unitMod;
        switch (m_attackType)
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

        float weapon_total_pct = m_caster->GetModifierValue(unitMod, TOTAL_PCT);
        bonus = int32(bonus * weapon_total_pct);
    }

    bonus += flat_bonus;

    // + weapon damage with applied weapon% dmg to base weapon damage in call
    bonus += int32(m_caster->CalculateDamage(m_attackType, normalized) *
                   weaponDamagePercentMod);

    // total damage
    bonus = int32(bonus * totalDamagePercentMod);

    // prevent negative damage
    m_damage += uint32(bonus > 0 ? bonus : 0);

    // Hemorrhage
    if (m_spellInfo->IsFitToFamily(
            SPELLFAMILY_ROGUE, UI64LIT(0x0000000002000000)))
    {
        if (m_caster->GetTypeId() == TYPEID_PLAYER)
            ((Player*)m_caster)->AddComboPoints(unitTarget, 1);
    }
    // Mangle (Cat): CP
    else if (m_spellInfo->IsFitToFamily(
                 SPELLFAMILY_DRUID, UI64LIT(0x0000040000000000)))
    {
        if (m_caster->GetTypeId() == TYPEID_PLAYER)
            ((Player*)m_caster)->AddComboPoints(unitTarget, 1);
    }
    /* XXX*/
    // take ammo
    if (m_attackType == RANGED_ATTACK && m_caster->GetTypeId() == TYPEID_PLAYER)
    {
        Item* pItem =
            ((Player*)m_caster)->GetWeaponForAttack(RANGED_ATTACK, true, false);

        // wands don't have ammo
        if (!pItem || pItem->GetProto()->SubClass == ITEM_SUBCLASS_WEAPON_WAND)
            return;

        if (pItem->GetProto()->InventoryType == INVTYPE_THROWN)
        {
            if (pItem->GetMaxStackCount() > 1)
            {
                // decrease items amount for stackable throw weapon
                static_cast<Player*>(m_caster)->storage().remove_count(
                    pItem, 1);
            }
        }
        else if (uint32 ammo =
                     ((Player*)m_caster)->GetUInt32Value(PLAYER_AMMO_ID))
        {
            inventory::transaction trans(false);
            trans.destroy(ammo, 1);
            static_cast<Player*>(m_caster)->storage().finalize(trans);
        }
    }
}

void Spell::EffectThreat(SpellEffectIndex /*eff_idx*/)
{
    if (!unitTarget || !unitTarget->isAlive() || !m_caster->isAlive())
        return;

    if (!unitTarget->CanHaveThreatList())
        return;

    switch (m_spellInfo->Id)
    {
    // Growl should scale with ranged attack power
    case 2649:
    case 14916:
    case 14917:
    case 14918:
    case 14919:
    case 14920:
    case 14921:
    case 27047:
    {
        auto pet_lvl = m_caster->getLevel();

        // table:
        // pet level  |  threshold
        // 10            35
        // 20            210
        // 30            385
        // 40            560
        // 50            735
        // 60            910
        // 70            1953 (1085 * 1.8) <- this one could use more data
        //                                    in the lvl range [61,69]
        float threshold = 35 * (1 + (pet_lvl > 10 ? (pet_lvl - 10) / 2 : 0.0f));
        if (pet_lvl > 60)
            threshold *= 1.8f;

        float rap = 0.0f;
        if (auto owner = m_caster->GetOwner())
            rap = owner->GetTotalAttackPowerValue(RANGED_ATTACK);

        if (rap > threshold)
            damage += (rap - threshold) * 1.25f;

        break;
    }

    default:
        break;
    }

    unitTarget->AddThreat(m_caster, float(damage), false,
        GetSpellSchoolMask(m_spellInfo), m_spellInfo);
}

void Spell::EffectHealMaxHealth(SpellEffectIndex /*eff_idx*/)
{
    if (!unitTarget)
        return;
    if (!unitTarget->isAlive())
        return;

    uint32 heal = m_caster->GetMaxHealth();

    m_healing += heal;
}

void Spell::EffectInterruptCast(SpellEffectIndex eff_idx)
{
    if (!unitTarget)
        return;
    if (!unitTarget->isAlive())
        return;

    for (int i = 0; i < 2; ++i)
    {
        // Try interrupting spell in generic and channeled container
        auto type = i == 0 ? CURRENT_GENERIC_SPELL : CURRENT_CHANNELED_SPELL;
        Spell* spell = unitTarget->GetCurrentSpell(type);
        if (!spell)
            continue;
        auto info = spell->m_spellInfo;

        // Check so spell is interruptable
        if (spell->getState() != SPELL_STATE_CASTING &&
            !(spell->getState() == SPELL_STATE_PREPARING &&
                spell->GetCastTime() > 0.0f))
            continue;
        if (info->PreventionType != SPELL_PREVENTION_TYPE_SILENCE)
            continue;
        if (type == CURRENT_GENERIC_SPELL &&
            (info->InterruptFlags & SPELL_INTERRUPT_FLAG_INTERRUPT) == 0)
            continue;
        // TODO: CHANNEL_FLAG_MOVEMENT is probably wrongly named atm,
        // trinitycore calls it INTERRUPT instead of MOVEMENT
        if (type == CURRENT_CHANNELED_SPELL &&
            (info->ChannelInterruptFlags & CHANNEL_FLAG_MOVEMENT) == 0)
            continue;

        WorldPacket data(SMSG_SPELLLOGEXECUTE, (8 + 4 + 4 + 4 + 4 + 8));
        data << m_caster->GetPackGUID();
        data << uint32(m_spellInfo->Id);
        data << uint32(1);
        data << uint32(SPELL_EFFECT_INTERRUPT_CAST);
        data << uint32(m_spellInfo->SchoolMask);
        data << unitTarget->GetPackGUID();
        data << uint32(info->Id);
        m_caster->SendMessageToSet(&data, true);

        uint32 duration =
            GetSpellDuration(m_spellInfo) *
            unitTarget->GetInterruptAndSilenceModifier(info, eff_idx);

        unitTarget->ProhibitSpellSchool(GetSpellSchoolMask(info), duration);
        unitTarget->InterruptSpell(type, false);
    }
}

void Spell::EffectSummonObjectWild(SpellEffectIndex eff_idx)
{
    uint32 gameobject_id = m_spellInfo->EffectMiscValue[eff_idx];

    auto pGameObj = new GameObject;

    WorldObject* target = focusObject;
    if (!target)
        target = m_caster;

    float x, y, z;
    if (m_targets.m_targetMask & TARGET_FLAG_DEST_LOCATION)
    {
        x = m_targets.m_destX;
        y = m_targets.m_destY;
        z = m_targets.m_destZ;
    }
    else
        m_caster->GetPosition(x, y, z);

    Map* map = target->GetMap();

    if (!pGameObj->Create(map->GenerateLocalLowGuid(HIGHGUID_GAMEOBJECT),
            gameobject_id, map, x, y, z, target->GetO()))
    {
        delete pGameObj;
        return;
    }

    int32 duration = GetSpellDuration(m_spellInfo);

    pGameObj->SetRespawnTime(duration > 0 ? duration / IN_MILLISECONDS : 0);
    pGameObj->SetSpellId(m_spellInfo->Id);
    pGameObj->SetTemporary(true);

    // Wild object not have owner and check clickable by players
    if (!map->insert(pGameObj))
    {
        delete pGameObj;
        return;
    }

    // FIXME HACK: Remove this ENTIRE switch when we have PROPER game object
    // spell casting
    // Start remove @ proper GO casting
    switch (m_spellInfo->Id)
    {
    // Blaze (N & H) really need a proper owner, or it won't work as intended
    // (due to GOs not being able to cast spells)
    case 23971:
    case 30928:
    {
        if (Unit* caster = GetAffectiveCaster())
            pGameObj->SetOwnerGuid(caster->GetObjectGuid());
        break;
    }
    default:
        break;
    }
    // End remove @ proper GO casting

    if (pGameObj->GetGoType() == GAMEOBJECT_TYPE_FLAGDROP &&
        m_caster->GetTypeId() == TYPEID_PLAYER)
    {
        Player* pl = (Player*)m_caster;
        BattleGround* bg = ((Player*)m_caster)->GetBattleGround();

        switch (pGameObj->GetMapId())
        {
        case 489: // WS
        {
            if (bg && bg->GetTypeID() == BATTLEGROUND_WS &&
                bg->GetStatus() == STATUS_IN_PROGRESS)
            {
                Team team = pl->GetTeam() == ALLIANCE ? HORDE : ALLIANCE;

                ((BattleGroundWS*)bg)
                    ->SetDroppedFlagGuid(pGameObj->GetObjectGuid(), team);
            }
            break;
        }
        case 566: // EY
        {
            if (bg && bg->GetTypeID() == BATTLEGROUND_EY &&
                bg->GetStatus() == STATUS_IN_PROGRESS)
            {
                ((BattleGroundEY*)bg)
                    ->SetDroppedFlagGuid(pGameObj->GetObjectGuid());
            }
            break;
        }
        }
    }

    auto caster_guid = m_caster->GetObjectGuid();
    auto orig_guid = m_caster->GetObjectGuid();
    pGameObj->queue_action(0, [pGameObj, caster_guid, orig_guid]()
        {
            auto caster = pGameObj->GetMap()->GetUnit(caster_guid);
            auto orig_caster = pGameObj->GetMap()->GetUnit(orig_guid);

            pGameObj->SummonLinkedTrapIfAny();

            if (caster && caster->GetTypeId() == TYPEID_UNIT &&
                ((Creature*)caster)->AI())
                ((Creature*)caster)->AI()->JustSummoned(pGameObj);
            if (orig_caster && orig_caster != caster &&
                orig_caster->GetTypeId() == TYPEID_UNIT &&
                ((Creature*)orig_caster)->AI())
                ((Creature*)orig_caster)->AI()->JustSummoned(pGameObj);
        });
}

void Spell::EffectScriptEffect(SpellEffectIndex eff_idx)
{
    // TODO: we must implement hunter pet summon at login there (spell 6962)

    switch (m_spellInfo->SpellFamilyName)
    {
    case SPELLFAMILY_GENERIC:
    {
        switch (m_spellInfo->Id)
        {
        case 8856: // Bending Shinbone
        {
            if (!itemTarget && m_caster->GetTypeId() != TYPEID_PLAYER)
                return;

            uint32 spell_id = 0;
            switch (urand(1, 5))
            {
            case 1:
                spell_id = 8854;
                break;
            default:
                spell_id = 8855;
                break;
            }

            m_caster->CastSpell(m_caster, spell_id, true, nullptr);
            return;
        }
        case 17512: // Piccolo of the Flaming Fire
        {
            if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
                return;

            unitTarget->HandleEmoteCommand(EMOTE_STATE_DANCE);

            return;
        }
        case 22539: // Shadow Flame (All script effects, not just end ones to
        case 22972: // prevent player from dodging the last triggered spell)
        case 22975:
        case 22976:
        case 22977:
        case 22978:
        case 22979:
        case 22980:
        case 22981:
        case 22982:
        case 22983:
        case 22984:
        case 22985:
        {
            if (!unitTarget || !unitTarget->isAlive())
                return;

            // Onyxia Scale Cloak
            if (unitTarget->has_aura(22683, SPELL_AURA_DUMMY))
                return;

            // Shadow Flame
            m_caster->CastSpell(unitTarget, 22682, true);
            return;
        }
        case 23969: // Throw Liquid Fire
        {
            if (unitTarget)
                m_caster->CastSpell(unitTarget, 23970, true);
            return;
        }
        case 23970: // Throw Liquid Fire
        {
            if (unitTarget)
                unitTarget->CastSpell(unitTarget, 23971, true);
            return;
        }
        case 24194: // Uther's Tribute
        case 24195: // Grom's Tribute
        {
            if (m_caster->GetTypeId() != TYPEID_PLAYER)
                return;

            uint8 race = m_caster->getRace();
            uint32 spellId = 0;

            switch (m_spellInfo->Id)
            {
            case 24194:
                switch (race)
                {
                case RACE_HUMAN:
                    spellId = 24105;
                    break;
                case RACE_DWARF:
                    spellId = 24107;
                    break;
                case RACE_NIGHTELF:
                    spellId = 24108;
                    break;
                case RACE_GNOME:
                    spellId = 24106;
                    break;
                // next case not exist in 2.x officially (quest has been broken
                // for race until 3.x time)
                case RACE_DRAENEI:
                    spellId = 24108;
                    break;
                }
                break;
            case 24195:
                switch (race)
                {
                case RACE_ORC:
                    spellId = 24104;
                    break;
                case RACE_UNDEAD:
                    spellId = 24103;
                    break;
                case RACE_TAUREN:
                    spellId = 24102;
                    break;
                case RACE_TROLL:
                    spellId = 24101;
                    break;
                // next case not exist in 2.x officially (quest has been broken
                // for race until 3.x time)
                case RACE_BLOODELF:
                    spellId = 24101;
                    break;
                }
                break;
            }

            if (spellId)
                m_caster->CastSpell(m_caster, spellId, true);

            return;
        }
        case 24320: // Poisonous Blood
        {
            unitTarget->CastSpell(unitTarget, 24321, true);
            return;
        }
        case 24324: // Blood Siphon
        {
            if (unitTarget->has_aura(24321))
            {
                unitTarget->remove_auras(24321);
                unitTarget->CastSpell(unitTarget, 24323, true);
            }
            else
                unitTarget->CastSpell(unitTarget, 24322, true);
            return;
        }
        case 24458: // Shadow Shock
        {
            unitTarget->CastSpell(unitTarget, 24459, true);
            return;
        }
        case 24590: // Brittle Armor - need remove one 24575 Brittle Armor aura
            if (auto holder = unitTarget->get_aura(24575))
                if (holder->ModStackAmount(-1))
                    unitTarget->RemoveAuraHolder(holder);
            return;
        case 24714: // Trick
        {
            if (m_caster->GetTypeId() != TYPEID_PLAYER)
                return;

            if (roll_chance_i(14)) // Trick (can be different critter models).
                                   // 14% since below can have 1 of 6
                m_caster->CastSpell(m_caster, 24753, true);
            else // Random Costume, 6 different (plus add. for gender)
                m_caster->CastSpell(m_caster, 24720, true);

            return;
        }
        case 24717: // Pirate Costume
        {
            if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
                return;

            // Pirate Costume (male or female)
            m_caster->CastSpell(unitTarget,
                unitTarget->getGender() == GENDER_MALE ? 24708 : 24709, true);
            return;
        }
        case 24718: // Ninja Costume
        {
            if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
                return;

            // Ninja Costume (male or female)
            m_caster->CastSpell(unitTarget,
                unitTarget->getGender() == GENDER_MALE ? 24711 : 24710, true);
            return;
        }
        case 24719: // Leper Gnome Costume
        {
            if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
                return;

            // Leper Gnome Costume (male or female)
            m_caster->CastSpell(unitTarget,
                unitTarget->getGender() == GENDER_MALE ? 24712 : 24713, true);
            return;
        }
        case 24720: // Random Costume
        {
            if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
                return;

            uint32 spellId = 0;

            switch (urand(0, 6))
            {
            case 0:
                spellId =
                    unitTarget->getGender() == GENDER_MALE ? 24708 : 24709;
                break;
            case 1:
                spellId =
                    unitTarget->getGender() == GENDER_MALE ? 24711 : 24710;
                break;
            case 2:
                spellId =
                    unitTarget->getGender() == GENDER_MALE ? 24712 : 24713;
                break;
            case 3:
                spellId = 24723;
                break;
            case 4:
                spellId = 24732;
                break;
            case 5:
                spellId =
                    unitTarget->getGender() == GENDER_MALE ? 24735 : 24736;
                break;
            case 6:
                spellId = 24740;
                break;
            }

            m_caster->CastSpell(unitTarget, spellId, true);
            return;
        }
        case 24737: // Ghost Costume
        {
            if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
                return;

            // Ghost Costume (male or female)
            m_caster->CastSpell(unitTarget,
                unitTarget->getGender() == GENDER_MALE ? 24735 : 24736, true);
            return;
        }
        case 24751: // Trick or Treat
        {
            if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
                return;

            // Tricked or Treated
            unitTarget->CastSpell(unitTarget, 24755, true);

            // Treat / Trick
            unitTarget->CastSpell(
                unitTarget, roll_chance_i(50) ? 24714 : 24715, true);
            return;
        }
        case 25140: // Orb teleport spells
        case 25143:
        case 25650:
        case 25652:
        case 29128:
        case 29129:
        case 35376:
        case 35727:
        {
            if (!unitTarget)
                return;

            uint32 spellid;
            switch (m_spellInfo->Id)
            {
            case 25140:
                spellid = 32568;
                break;
            case 25143:
                spellid = 32572;
                break;
            case 25650:
                spellid = 30140;
                break;
            case 25652:
                spellid = 30141;
                break;
            case 29128:
                spellid = 32571;
                break;
            case 29129:
                spellid = 32569;
                break;
            case 35376:
                spellid = 25649;
                break;
            case 35727:
                spellid = 35730;
                break;
            default:
                return;
            }

            unitTarget->CastSpell(unitTarget, spellid, false);
            return;
        }
        case 18670: // Knock Away
        {
            // Puts current threat of target at * 0.50
            if (m_caster->CanHaveThreatList() && unitTarget)
                m_caster->getThreatManager().modifyThreatPercent(
                    unitTarget, -50);
            return;
        }
        case 18945: // Knock Away
        case 25778: // Knock Away
        {
            // Puts current threat of target at * 0.75
            if (m_caster->CanHaveThreatList() && unitTarget)
                m_caster->getThreatManager().modifyThreatPercent(
                    unitTarget, -25);
            return;
        }
        case 26004: // Mistletoe
        {
            if (!unitTarget)
                return;

            unitTarget->HandleEmote(EMOTE_ONESHOT_CHEER);
            return;
        }
        case 26137: // Rotate Trigger
        {
            if (unitTarget)
            {
                unitTarget->SetFacingTo(unitTarget->GetO());
                auto unit = unitTarget;
                unit->queue_action_ticks(0, [unit]()
                    {
                        unit->CastSpell(
                            unit, urand(0, 1) ? 26009 : 26136, true);
                    });
            }
            return;
        }
        case 26218: // Mistletoe
        {
            if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
                return;

            uint32 spells[3] = {26206, 26207, 45036};

            m_caster->CastSpell(unitTarget, spells[urand(0, 2)], true);
            return;
        }
        case 26275: // PX-238 Winter Wondervolt TRAP
        {
            uint32 spells[4] = {26272, 26157, 26273, 26274};

            // check presence
            for (auto& spell : spells)
                if (unitTarget->has_aura(spell))
                    return;

            // cast
            unitTarget->CastSpell(unitTarget, spells[urand(0, 3)], true);
            return;
        }
        case 26465: // Mercurial Shield - need remove one 26464 Mercurial Shield
                    // aura
            if (auto holder = unitTarget->get_aura(26464))
                if (holder->ModStackAmount(-1))
                    unitTarget->RemoveAuraHolder(holder);
            return;
        case 26552: // Nullify
        {
            unitTarget->SetHealth(1);
            return;
        }
        case 26584: // Summon Toxic Slime
        {
            if (unitTarget)
                unitTarget->CastSpell(unitTarget, 26577, true);
            return;
        }
        case 26656: // Summon Black Qiraji Battle Tank
        {
            if (!unitTarget)
                return;

            // Prevent stacking of mounts
            unitTarget->remove_auras(SPELL_AURA_MOUNTED);

            // Two separate mounts depending on area id (allows use both in and
            // out of specific instance)
            if (unitTarget->GetAreaId() == 3428)
                unitTarget->CastSpell(unitTarget, 25863, false);
            else
                unitTarget->CastSpell(unitTarget, 26655, false);

            return;
        }
        case 27687: // Summon Bone Minions
        {
            if (!unitTarget)
                return;

            // Spells 27690, 27691, 27692, 27693 are missing from DBC
            // So we need to summon creature 16119 manually
            float angle = unitTarget->GetO();
            for (uint8 i = 0; i < 4; ++i)
            {
                auto pos = unitTarget->GetPoint(angle + i * M_PI_F / 2, 5.0f);
                unitTarget->SummonCreature(16119, pos.x, pos.y, pos.z, angle,
                    TEMPSUMMON_TIMED_OR_DEAD_DESPAWN,
                    10 * MINUTE * IN_MILLISECONDS);
            }
            return;
        }
        case 27695: // Summon Bone Mages
        {
            if (!unitTarget)
                return;

            unitTarget->CastSpell(unitTarget, 27696, true);
            unitTarget->CastSpell(unitTarget, 27697, true);
            unitTarget->CastSpell(unitTarget, 27698, true);
            unitTarget->CastSpell(unitTarget, 27699, true);
            return;
        }
        case 28374: // Decimate (Naxxramas: Gluth)
        {
            if (!unitTarget)
                return;

            int32 damage =
                unitTarget->GetHealth() - unitTarget->GetMaxHealth() * 0.05f;
            if (damage > 0)
                m_caster->CastCustomSpell(
                    unitTarget, 28375, &damage, nullptr, nullptr, true);
            return;
        }
        case 28560: // Summon Blizzard
        {
            if (!unitTarget)
                return;

            m_caster->SummonCreature(16474, unitTarget->GetX(),
                unitTarget->GetY(), unitTarget->GetZ(), 0.0f,
                TEMPSUMMON_TIMED_DESPAWN, 30000);
            return;
        }
        case 29395: // Break Kaliri Egg
        {
            uint32 creature_id = 0;
            uint32 rand = urand(0, 99);

            if (rand < 10)
                creature_id = 17034;
            else if (rand < 60)
                creature_id = 17035;
            else
                creature_id = 17039;

            if (WorldObject* pSource = GetAffectiveCasterObject())
                pSource->SummonCreature(creature_id, 0.0f, 0.0f, 0.0f, 0.0f,
                    TEMPSUMMON_TIMED_OR_DEAD_DESPAWN, 120 * IN_MILLISECONDS);
            return;
        }
        case 29830: // Mirren's Drinking Hat
        {
            uint32 item = 0;
            switch (urand(1, 6))
            {
            case 1:
            case 2:
            case 3:
                item = 23584;
                break; // Loch Modan Lager
            case 4:
            case 5:
                item = 23585;
                break; // Stouthammer Lite
            case 6:
                item = 23586;
                break; // Aerie Peak Pale Ale
            }

            if (item)
                DoCreateItem(eff_idx, item);

            break;
        }
        case 30918: // Improved Sprint
        {
            if (!unitTarget)
                return;

            // Removes snares and roots.
            unitTarget->remove_auras_if([](AuraHolder* holder)
                {
                    return !holder->GetSpellProto()->HasAttribute(
                               SPELL_ATTR_UNAFFECTED_BY_INVULNERABILITY) &&
                           holder->HasMechanicMask(
                               IMMUNE_TO_ROOT_AND_SNARE_MASK) &&
                           !holder->IsPositive();
                });
            break;
        }
        case 32301: // Ping Shirrak
        {
            if (!unitTarget)
                return;

            // Cast Focus fire on caster
            unitTarget->CastSpell(m_caster, 32300, true);
            return;
        }
        case 33040: // Wrath of the Astromancer
        case 33049:
        {
            if (unitTarget && GetAffectiveCaster())
            {
                m_caster->CastSpell(unitTarget, 33045, true, nullptr, nullptr,
                    GetAffectiveCaster()->GetObjectGuid());
                m_caster->CastSpell(unitTarget, 33044, true, nullptr, nullptr,
                    GetAffectiveCaster()->GetObjectGuid());
            }
            return;
        }
        case 33091: // Determination
        {
            if (unitTarget)
                unitTarget->remove_auras(33129); // Remove Dark Decay
            return;
        }
        case 33525: // Ground Slam
        {
            if (unitTarget && unitTarget->GetTypeId() == TYPEID_PLAYER)
            {
                auto target = unitTarget;
                target->queue_action(urand(300, 600), [target]()
                    {
                        static_cast<Player*>(target)->KnockBack(
                            frand(0, 2 * M_PI_F), frand(10.0f, 25.0f),
                            frand(10.0f, 25.0f));
                    });
            }
            return;
        }
        case 33654: // Shatter
        {
            if (unitTarget)
            {
                unitTarget->remove_auras(33652); // Remove stoned
                // Shatter
                if (unitTarget->GetTypeId() == TYPEID_PLAYER)
                    unitTarget->CastSpell(unitTarget, 33671, true);
            }
            return;
        }
        case 35865: // Summon Nether Vapor
        {
            // Spawn 4 clouds for 30 seconds around us
            for (int i = 0; i < 4; ++i)
            {
                auto pos = m_caster->GetPoint(
                    frand(0.0f, 2 * M_PI_F), frand(3.0f, 5.0f));
                m_caster->SummonCreature(21002, pos.x, pos.y, pos.z,
                    m_caster->GetO(), TEMPSUMMON_TIMED_DESPAWN,
                    30 * IN_MILLISECONDS);
            }
            return;
        }
        case 35869: // Nether Beam
        {
            // Select 5 random players from our threat list and cast Nether Beam
            // on them
            if (!m_caster->CanHaveThreatList())
                return;
            std::vector<Player*> potential;
            potential.reserve(
                m_caster->getThreatManager().getThreatList().size());
            for (const auto& ref : m_caster->getThreatManager().getThreatList())
            {
                auto target = ref->getTarget();
                if (target->GetTypeId() == TYPEID_PLAYER)
                    potential.push_back(static_cast<Player*>(target));
            }
            for (int i = 0; i < 5 && !potential.empty(); ++i)
            {
                int j = urand(0, potential.size() - 1);
                m_caster->CastSpell(potential[j], 35873, true);
                potential.erase(potential.begin() + j);
            }
            return;
        }
        case 36153: // Netherstorm Ghost's Soulbind
        {
            if (unitTarget)
                unitTarget->CastSpell(m_caster, 36141, true);
            return;
        }
        case 36976: // Summon Weapons
        {
            // Trigger all weapon spells
            for (uint32 i = 36958; i <= 36964; ++i)
                m_caster->CastSpell(m_caster, i, true);
            return;
        }
        case 37750: // Clear Consuming Madness
        {
            if (unitTarget)
                unitTarget->remove_auras(37749);
            return;
        }
        case 38358: // Tidal Surge
        {
            if (!unitTarget)
                return;

            unitTarget->CastSpell(unitTarget, 38353, true, nullptr, nullptr,
                m_caster->GetObjectGuid());
            return;
        }
        case 38508: // Aggro Bleeding Hollow
        {
            if (unitTarget && unitTarget->GetTypeId() == TYPEID_UNIT &&
                static_cast<Creature*>(unitTarget)->AI())
                static_cast<Creature*>(unitTarget)->AI()->AttackStart(m_caster);
            return;
        }
        case 38573: // Spore Drop Effect
        {
            if (unitTarget)
                unitTarget->CastSpell(unitTarget, 38574, true, nullptr, nullptr,
                    m_caster->GetObjectGuid());
            return;
        }
        case 38650: // Rancid Mushroom Primer
        {
            if (unitTarget)
            {
                auto pos = unitTarget->GetPoint(frand(0, 2 * M_PI_F), 2.5f);
                unitTarget->SummonCreature(22250, pos.x, pos.y, pos.z, 0.0f,
                    TEMPSUMMON_MANUAL_DESPAWN, 0); // Script despawns them
            }
            return;
        }
        case 41055: // Copy Weapon
        {
            if (m_caster->GetTypeId() != TYPEID_UNIT || !unitTarget ||
                unitTarget->GetTypeId() != TYPEID_PLAYER)
                return;

            if (Item* pItem =
                    ((Player*)unitTarget)->GetWeaponForAttack(BASE_ATTACK))
            {
                ((Creature*)m_caster)
                    ->SetVirtualItem(VIRTUAL_ITEM_SLOT_0, pItem->GetEntry());

                // Unclear what this spell should do
                unitTarget->CastSpell(
                    m_caster, m_spellInfo->CalculateSimpleValue(eff_idx), true);
            }

            return;
        }
        case 41126: // Flame Crash
        {
            if (!unitTarget)
                return;

            unitTarget->CastSpell(unitTarget, 41131, true);
            break;
        }
        case 44876: // Force Cast - Portal Effect: Sunwell Isle
        {
            if (!unitTarget)
                return;

            unitTarget->CastSpell(unitTarget, 44870, true);
            break;
        }
        case 44811: // Spectral Realm
        {
            if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
                return;

            // If the player can't be teleported, send him a notification
            if (unitTarget->has_aura(44867))
            {
                ((Player*)unitTarget)
                    ->GetSession()
                    ->SendNotification(LANG_FAIL_ENTER_SPECTRAL_REALM);
                return;
            }

            // Teleport target to the spectral realm, add debuff and force
            // faction
            unitTarget->CastSpell(unitTarget, 44852, true);
            unitTarget->CastSpell(unitTarget, 46019, true);
            unitTarget->CastSpell(unitTarget, 46021, true);
            return;
        }
        case 45141: // Burn
        {
            if (!unitTarget)
                return;

            unitTarget->CastSpell(unitTarget, 46394, true, nullptr, nullptr,
                m_caster->GetObjectGuid());
            return;
        }
        case 45151: // Burn
        {
            if (!unitTarget || unitTarget->has_aura(46394))
                return;

            // Make the burn effect jump to another friendly target
            unitTarget->CastSpell(unitTarget, 46394, true);
            return;
        }
        case 45185: // Stomp
        {
            if (!unitTarget)
                return;

            // Remove the burn effect
            unitTarget->remove_auras(46394);
            return;
        }
        case 45206: // Copy Off-hand Weapon
        {
            if (m_caster->GetTypeId() != TYPEID_UNIT || !unitTarget ||
                unitTarget->GetTypeId() != TYPEID_PLAYER)
                return;

            if (Item* pItem =
                    ((Player*)unitTarget)->GetWeaponForAttack(OFF_ATTACK))
            {
                ((Creature*)m_caster)
                    ->SetVirtualItem(VIRTUAL_ITEM_SLOT_1, pItem->GetEntry());

                // Unclear what this spell should do
                unitTarget->CastSpell(
                    m_caster, m_spellInfo->CalculateSimpleValue(eff_idx), true);
            }

            return;
        }
        case 45262: // Using Steam Tonk Controller
        {
            if (Pet* tonk = m_caster->FindGuardianWithEntry(19405))
                m_caster->CastSpell(tonk, 33849, true);
            break;

            return;
        }
        case 46203: // Goblin Weather Machine
        {
            if (!unitTarget)
                return;

            uint32 spellId = 0;
            switch (urand(0, 3))
            {
            case 0:
                spellId = 46740;
                break;
            case 1:
                spellId = 46739;
                break;
            case 2:
                spellId = 46738;
                break;
            case 3:
                spellId = 46736;
                break;
            }
            unitTarget->CastSpell(unitTarget, spellId, true);
            break;
        }
        case 46642: // 5,000 Gold
        {
            if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
                return;

            inventory::transaction trans;
            trans.add(inventory::gold(5000));
            static_cast<Player*>(unitTarget)->storage().finalize(trans);
            break;
        }
        case 48917: // Who Are They: Cast from Questgiver
        {
            if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
                return;

            // Male Shadowy Disguise / Female Shadowy Disguise
            unitTarget->CastSpell(unitTarget,
                unitTarget->getGender() == GENDER_MALE ? 38080 : 38081, true);
            // Shadowy Disguise
            unitTarget->CastSpell(unitTarget, 32756, true);
            return;
        }
        case 150067: // Anubisath Sentinel Auras Selector
        {
            if (!unitTarget || unitTarget->GetTypeId() != TYPEID_UNIT ||
                !static_cast<Creature*>(unitTarget)->GetGroup())
                return;
            std::vector<uint32> spells{
                2147, 9347, 25777, 2148, 2834, 13022, 19595, 21737, 812};
            if (static_cast<Creature*>(unitTarget)
                    ->GetGroup()
                    ->GetMembers()
                    .size() > spells.size())
                return;
            for (auto member : *static_cast<Creature*>(unitTarget)->GetGroup())
            {
                auto index = urand(0, spells.size() - 1);
                member->CastSpell(member, spells[index], true);
                spells.erase(spells.begin() + index);
            }
            return;
        }
        }
        break;
    }
    case SPELLFAMILY_WARLOCK:
    {
        switch (m_spellInfo->Id)
        {
        case 6201: // Healthstone creating spells
        case 6202:
        case 5699:
        case 11729:
        case 11730:
        case 27230:
        {
            if (!unitTarget)
                return;

            uint32 itemtype;
            uint32 rank = 0;
            auto& dummyAuras = unitTarget->GetAurasByType(SPELL_AURA_DUMMY);
            for (const auto& dummyAura : dummyAuras)
            {
                if ((dummyAura)->GetId() == 18692)
                {
                    rank = 1;
                    break;
                }
                else if ((dummyAura)->GetId() == 18693)
                {
                    rank = 2;
                    break;
                }
            }

            static uint32 const itypes[6][3] = {
                {5512, 19004, 19005}, // Minor Healthstone
                {5511, 19006, 19007}, // Lesser Healthstone
                {5509, 19008, 19009}, // Healthstone
                {5510, 19010, 19011}, // Greater Healthstone
                {9421, 19012, 19013}, // Major Healthstone
                {22103, 22104, 22105} // Master Healthstone
            };

            switch (m_spellInfo->Id)
            {
            case 6201:
                itemtype = itypes[0][rank];
                break; // Minor Healthstone
            case 6202:
                itemtype = itypes[1][rank];
                break; // Lesser Healthstone
            case 5699:
                itemtype = itypes[2][rank];
                break; // Healthstone
            case 11729:
                itemtype = itypes[3][rank];
                break; // Greater Healthstone
            case 11730:
                itemtype = itypes[4][rank];
                break; // Major Healthstone
            case 27230:
                itemtype = itypes[5][rank];
                break; // Master Healthstone
            default:
                return;
            }
            DoCreateItem(eff_idx, itemtype);
            return;
        }
        }
        break;
    }
    case SPELLFAMILY_PALADIN:
    {
        if (m_spellInfo->SpellFamilyFlags & UI64LIT(0x0000000000800000))
        {
            if (!unitTarget || !unitTarget->isAlive())
                return;

            uint32 spellId2 = 0;

            // all seals have aura dummy
            auto& dummyAuras = m_caster->GetAurasByType(SPELL_AURA_DUMMY);
            for (const auto& dummyAura : dummyAuras)
            {
                SpellEntry const* spellInfo = (dummyAura)->GetSpellProto();

                // search seal (all seals have judgement's aura dummy spell id
                // in 2 effect
                if (!spellInfo || !IsSealSpell((dummyAura)->GetSpellProto()) ||
                    (dummyAura)->GetEffIndex() != 2)
                    continue;

                // must be calculated base at raw base points in spell proto,
                // GetModifier()->m_value for S.Righteousness modified by
                // SPELLMOD_DAMAGE
                spellId2 = (dummyAura)->GetSpellProto()->CalculateSimpleValue(
                    EFFECT_INDEX_2);

                if (spellId2 <= 1)
                    continue;

                // found, remove seal (NOTE: we break out of loop after this)
                m_caster->remove_auras((dummyAura)->GetId());

                // Sanctified Judgement
                auto& auras = m_caster->GetAurasByType(SPELL_AURA_DUMMY);
                for (const auto& aura : auras)
                {
                    if ((aura)->GetId() == 37188)
                        m_caster->CastSpell(m_caster, 43838, true);

                    if ((aura)->GetSpellProto()->SpellIconID == 205 &&
                        (aura)->GetSpellProto()->Attributes == UI64LIT(0x01D0))
                    {
                        int32 chance = (aura)->GetModifier()->m_amount;
                        if (roll_chance_i(chance))
                        {
                            int32 mana = spellInfo->manaCost;
                            if (Player* modOwner = m_caster->GetSpellModOwner())
                                modOwner->ApplySpellMod(
                                    spellInfo->Id, SPELLMOD_COST, mana);
                            mana = int32(mana * 0.8f);
                            m_caster->CastCustomSpell(m_caster, 31930, &mana,
                                nullptr, nullptr, true, nullptr, aura);
                        }
                        break;
                    }
                }

                break; // NOTE: itr state invalid, break out of loop
            }

            if (spellId2)
            {
                // NOTE: We've made it so Judgement of Light has extra healing
                // it does encoded in the spell
                switch (spellId2)
                {
                case 20185:
                case 20344:
                case 20345:
                case 20346:
                case 27162:
                {
                    int32 bp = 0;
                    if (m_caster->has_aura(28775))
                        bp += 20;
                    if (m_caster->has_aura(37182))
                        bp += 20;
                    m_caster->CastCustomSpell(unitTarget, spellId2, nullptr,
                        &bp, nullptr, TRIGGER_TYPE_TRIGGERED |
                                          TRIGGER_TYPE_SHOW_IN_COMBAT_LOG |
                                          TRIGGER_TYPE_TRIGGER_PROCS);
                    return;
                }
                default:
                    break;
                }

                m_caster->CastSpell(unitTarget, spellId2,
                    TRIGGER_TYPE_TRIGGERED | TRIGGER_TYPE_SHOW_IN_COMBAT_LOG |
                        TRIGGER_TYPE_TRIGGER_PROCS);
            }
            return;
        }
        break;
    }
    case SPELLFAMILY_POTION:
    {
        switch (m_spellInfo->Id)
        {
        case 28698: // Dreaming Glory
        {
            if (!unitTarget)
                return;

            unitTarget->CastSpell(unitTarget, 28694, true);
            break;
        }
        case 28702: // Netherbloom
        {
            if (!unitTarget)
                return;

            // 25% chance of casting a random buff
            if (roll_chance_i(75))
                return;

            // triggered spells are 28703 to 28707
            // Note: some sources say, that there was the possibility of
            //       receiving a debuff. However, this seems to be removed by a
            //       patch.
            const uint32 spellid = 28703;

            // don't overwrite an existing aura
            for (uint8 i = 0; i < 5; ++i)
                if (unitTarget->has_aura(spellid + i))
                    return;

            unitTarget->CastSpell(unitTarget, spellid + urand(0, 4), true);
            break;
        }
        case 28720: // Nightmare Vine
        {
            if (!unitTarget)
                return;

            // 25% chance of casting Nightmare Pollen
            if (roll_chance_i(75))
                return;

            unitTarget->CastSpell(unitTarget, 28721, true);
            break;
        }
        }
        break;
    }
    }

    // normal DB scripted effect
    if (!unitTarget)
        return;

    LOG_DEBUG(logging, "Spell ScriptStart spellid %u in EffectScriptEffect ",
        m_spellInfo->Id);
    m_caster->GetMap()->ScriptsStart(
        sSpellScripts, m_spellInfo->Id, m_caster, unitTarget);
}

void Spell::EffectSanctuary(SpellEffectIndex /*eff_idx*/)
{
    if (!unitTarget)
        return;

    unitTarget->CombatStop();
    unitTarget->getHostileRefManager().deleteReferences(); // stop all fighting

    // Select nearby units
    float dist = m_caster->GetMap()->GetVisibilityDistance();
    auto vec = maps::visitors::yield_set<Unit, Player, Creature, Pet,
        SpecialVisCreature, TemporarySummon, Totem>{}(m_caster, dist,
        [](auto&& elem)
        {
            return elem->isAlive();
        });

    // Interrupt spells they're casting on us if they're hostile
    for (auto u : vec)
    {
        if (u->IsHostileTo(m_caster))
            u->InterruptSpellOn(m_caster);
    }

    // Update target's spell queue on EffectSanctuary
    auto& spell_queue = unitTarget->spell_queue();
    bool drop_stealth = false;

    for (auto itr = spell_queue.begin(); itr != spell_queue.end();)
    {
        if (itr->spell->m_caster->IsHostileTo(unitTarget))
        {
            // If any hostile spells with no damage components is present,
            // all pending SPELL_AURA_MOD_STEALTH will be dropped
            if (!IsDamagingSpell(itr->spell->m_spellInfo))
                drop_stealth = true;
            // Remove cooldown for SPELL_ATTR_DISABLED_WHILE_ACTIVE
            if (itr->spell->m_spellInfo->HasAttribute(
                    SPELL_ATTR_DISABLED_WHILE_ACTIVE) &&
                unitTarget->GetMap()->GetUnit(itr->spell->GetCasterGUID()) &&
                itr->spell->GetCaster()->GetTypeId() == TYPEID_PLAYER)
            {
                static_cast<Player*>(itr->spell->GetCaster())
                    ->RemoveSpellCooldown(itr->spell->m_spellInfo->Id, true);
            }
            itr = spell_queue.erase(itr);
        }
        else
        {
            ++itr;
        }
    }

    if (drop_stealth)
    {
        for (auto itr = spell_queue.begin(); itr != spell_queue.end();)
        {
            if (itr->spell->m_spellInfo->HasApplyAuraName(
                    SPELL_AURA_MOD_STEALTH))
            {
                // Remove cooldown for SPELL_ATTR_DISABLED_WHILE_ACTIVE
                if (itr->spell->m_spellInfo->HasAttribute(
                        SPELL_ATTR_DISABLED_WHILE_ACTIVE) &&
                    unitTarget->GetMap()->GetUnit(
                        itr->spell->GetCasterGUID()) &&
                    itr->spell->GetCaster()->GetTypeId() == TYPEID_PLAYER)
                {
                    static_cast<Player*>(itr->spell->GetCaster())
                        ->RemoveSpellCooldown(
                            itr->spell->m_spellInfo->Id, true);
                }
                itr = spell_queue.erase(itr);
            }
            else
                ++itr;
        }

        // Drop stealths already applied
        unitTarget->remove_auras(SPELL_AURA_MOD_STEALTH);
    }
}

void Spell::EffectAddComboPoints(SpellEffectIndex /*eff_idx*/)
{
    if (!unitTarget)
        return;

    if (m_caster->GetTypeId() != TYPEID_PLAYER)
        return;

    if (damage <= 0)
        return;

    ((Player*)m_caster)->AddComboPoints(unitTarget, damage);
}

void Spell::EffectDuel(SpellEffectIndex eff_idx)
{
    if (!m_caster || !unitTarget || m_caster->GetTypeId() != TYPEID_PLAYER ||
        unitTarget->GetTypeId() != TYPEID_PLAYER)
        return;

    Player* caster = (Player*)m_caster;
    Player* target = (Player*)unitTarget;

    // Check if target has ignored caster
    if (!target->GetSocial() ||
        target->GetSocial()->HasIgnore(caster->GetObjectGuid()))
        return;

    // target already has a requested or started duel
    if (target->duel)
        return;

    // caster is in a started duel
    if (caster->duel &&
        (caster->duel->startTime != 0 || caster->duel->startTimer != 0))
        return;

    // caster is in a duel, but the opponent has not accepted yet
    if (caster->duel)
    {
        // if the target is the same, just ignore the request
        if (caster->duel->opponent == target)
            return;
        // otherwise, cancel it so a new duel can be started
        caster->DuelComplete(DUEL_INTERUPTED);
    }

    // Players can only fight a duel with each other outside (=not inside
    // dungeons and not in capital cities)
    AreaTableEntry const* casterAreaEntry =
        GetAreaEntryByAreaID(caster->GetAreaId());
    if (casterAreaEntry && !(casterAreaEntry->flags & AREA_FLAG_DUEL))
    {
        SendCastResult(SPELL_FAILED_NO_DUELING); // Dueling isn't allowed here
        return;
    }

    AreaTableEntry const* targetAreaEntry =
        GetAreaEntryByAreaID(target->GetAreaId());
    if (targetAreaEntry && !(targetAreaEntry->flags & AREA_FLAG_DUEL))
    {
        SendCastResult(SPELL_FAILED_NO_DUELING); // Dueling isn't allowed here
        return;
    }

    // CREATE DUEL FLAG OBJECT
    auto pGameObj = new GameObject;

    uint32 gameobject_id = m_spellInfo->EffectMiscValue[eff_idx];

    Map* map = m_caster->GetMap();

    float cathetus = G3D::distance(unitTarget->GetX() - m_caster->GetX(),
                         unitTarget->GetY() - m_caster->GetY(),
                         unitTarget->GetZ() - m_caster->GetZ()) /
                     2.0f;
    float cathetus2 = cathetus * cathetus;
    float hypotenuse = sqrt(cathetus2 + cathetus2);
    auto pos = m_caster->GetPoint(
        m_caster->GetAngle(unitTarget) + M_PI_F / 4 - m_caster->GetO(),
        hypotenuse, true);

    if (!pGameObj->Create(map->GenerateLocalLowGuid(HIGHGUID_GAMEOBJECT),
            gameobject_id, map, pos.x, pos.y, pos.z, m_caster->GetO()))
    {
        delete pGameObj;
        return;
    }

    pGameObj->SetUInt32Value(GAMEOBJECT_FACTION, m_caster->getFaction());
    pGameObj->SetUInt32Value(GAMEOBJECT_LEVEL, m_caster->getLevel() + 1);
    int32 duration = GetSpellDuration(m_spellInfo);
    pGameObj->SetRespawnTime(duration > 0 ? duration / IN_MILLISECONDS : 0);
    pGameObj->SetSpellId(m_spellInfo->Id);
    pGameObj->SetTemporary(true);

    if (!map->insert(pGameObj))
    {
        delete pGameObj;
        return;
    }

    auto caster_guid = m_caster->GetObjectGuid();
    auto target_guid = target->GetObjectGuid();
    pGameObj->queue_action(0, [pGameObj, caster_guid, target_guid]()
        {
            auto go_guid = pGameObj->GetObjectGuid();
            auto caster = pGameObj->GetMap()->GetPlayer(caster_guid);
            auto target = pGameObj->GetMap()->GetPlayer(target_guid);
            if (!caster || !target || caster->duel || target->duel)
            {
                pGameObj->Delete();
                return;
            }

            caster->SetGuidValue(PLAYER_DUEL_ARBITER, go_guid);
            target->SetGuidValue(PLAYER_DUEL_ARBITER, go_guid);

            caster->AddGameObject(pGameObj);

            // create duel-info
            auto duel = new DuelInfo;
            duel->initiator = caster;
            duel->opponent = target;
            duel->startTime = 0;
            duel->startTimer = 0;
            caster->duel = duel;

            auto duel2 = new DuelInfo;
            duel2->initiator = caster;
            duel2->opponent = caster;
            duel2->startTime = 0;
            duel2->startTimer = 0;
            target->duel = duel2;

            // Send request
            WorldPacket data(SMSG_DUEL_REQUESTED, 8 + 8);
            data << pGameObj->GetObjectGuid();
            data << caster->GetObjectGuid();
            caster->GetSession()->send_packet(&data);
            target->GetSession()->send_packet(&data);
        });
}

void Spell::EffectStuck(SpellEffectIndex /*eff_idx*/)
{
    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
        return;

    if (!sWorld::Instance()->getConfig(CONFIG_BOOL_CAST_UNSTUCK))
        return;

    Player* pTarget = (Player*)unitTarget;

    LOG_DEBUG(logging, "Spell Effect: Stuck");
    LOG_DEBUG(logging,
        "Player %s (guid %u) used auto-unstuck future at map %u (%f, %f, %f)",
        pTarget->GetName(), pTarget->GetGUIDLow(), m_caster->GetMapId(),
        m_caster->GetX(), pTarget->GetY(), pTarget->GetZ());

    if (pTarget->IsTaxiFlying())
        return;

    // hearthstone spellinfo
    const SpellEntry* spellInfo = sSpellStore.LookupEntry(8690);
    if (!spellInfo)
        return;

    // kill target if hearthstone is on cooldown
    if (pTarget->HasSpellCooldown(spellInfo->Id))
    {
        pTarget->Kill(pTarget, true, m_spellInfo);
        return;
    }

    // homebind location is loaded always
    pTarget->TeleportToHomebind(unitTarget == m_caster ? TELE_TO_SPELL : 0);

    // Stuck spell trigger Hearthstone cooldown
    Spell spell(pTarget, spellInfo, true);
    spell.SendSpellCooldown();
}

void Spell::EffectSummonPlayer(SpellEffectIndex /*eff_idx*/)
{
    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
        return;

    // Evil Twin (ignore player summon, but hide this for summoner)
    if (unitTarget->has_aura(23445, SPELL_AURA_DUMMY))
        return;

    float x, y, z;
    m_caster->GetPosition(x, y, z);
    ((Player*)unitTarget)->SetSummonPoint(m_caster->GetMapId(), x, y, z);

    WorldPacket data(SMSG_SUMMON_REQUEST, 8 + 4 + 4);
    data << m_caster->GetObjectGuid();     // summoner guid
    data << uint32(m_caster->GetZoneId()); // summoner zone
    data << uint32(
        MAX_PLAYER_SUMMON_DELAY * IN_MILLISECONDS); // auto decline after msecs
    ((Player*)unitTarget)->GetSession()->send_packet(std::move(data));
}

static ScriptInfo generateActivateCommand()
{
    ScriptInfo si;
    si.command = SCRIPT_COMMAND_ACTIVATE_OBJECT;
    si.id = 0;
    si.buddyEntry = 0;
    si.searchRadius = 0;
    si.data_flags = 0x00;
    return si;
}

void Spell::EffectActivateObject(SpellEffectIndex eff_idx)
{
    if (!gameObjTarget)
        return;

    static ScriptInfo activateCommand = generateActivateCommand();
    activateCommand.activateObject.misc_value =
        m_spellInfo->EffectMiscValue[eff_idx];
    if (activateCommand.activateObject.misc_value)
        activateCommand.activateObject.misc_value -= 1;

    int32 delay_secs = m_spellInfo->CalculateSimpleValue(eff_idx);

    // A player charming an NPC should count as the one activating the GO (for
    // purpose of quest credit, etc)
    Unit* activator = m_caster;
    if (m_caster->GetTypeId() == TYPEID_UNIT)
        if (auto player = m_caster->GetCharmerOrOwnerPlayerOrPlayerItself())
            activator = player;

    gameObjTarget->GetMap()->ScriptCommandStart(
        activateCommand, delay_secs, activator, gameObjTarget);
}

void Spell::DoSummonTotem(SpellEffectIndex eff_idx, uint8 slot_dbc)
{
    // DBC store slots starting from 1, with no slot 0 value)
    int slot = slot_dbc ? slot_dbc - 1 : TOTEM_SLOT_NONE;

    // MiscB == 121 => totem with no owner
    if (slot != TOTEM_SLOT_NONE &&
        m_spellInfo->EffectMiscValueB[eff_idx] == 121)
        slot = TOTEM_SLOT_NONE;

    // unsummon old totem
    if (slot < MAX_TOTEM_SLOT)
        if (Totem* OldTotem = m_caster->GetTotem(TotemSlot(slot)))
            OldTotem->UnSummon();

    // FIXME: Setup near to finish point because GetObjectBoundingRadius set in
    // Create but some Create calls can be dependent from proper position
    // if totem have creature_template_addon.auras with persistent point for
    // example or script call
    float angle =
        slot < MAX_TOTEM_SLOT ?
            M_PI_F / MAX_TOTEM_SLOT - (slot * 2 * M_PI_F / MAX_TOTEM_SLOT) :
            0;

    CreatureCreatePos pos(m_caster, m_caster->GetO(), 2.0f, angle);

    CreatureInfo const* cinfo =
        ObjectMgr::GetCreatureTemplate(m_spellInfo->EffectMiscValue[eff_idx]);
    if (!cinfo)
    {
        logging.error(
            "Creature entry %u does not exist but used in spell %u totem "
            "summon.",
            m_spellInfo->Id, m_spellInfo->EffectMiscValue[eff_idx]);
        return;
    }

    auto pTotem = new Totem;

    if (!pTotem->Create(m_caster->GetMap()->GenerateLocalLowGuid(HIGHGUID_UNIT),
            pos, cinfo, m_caster))
    {
        delete pTotem;
        return;
    }

    pTotem->SetSummonPoint(pos);

    // pTotem->SetName("");                                  // generated by
    // client
    if (m_spellInfo->EffectMiscValueB[eff_idx] != 121)
    {
        pTotem->SetOwner(m_caster);
    }
    else
    {
        // We still want everything but creator guid
        pTotem->SetLevel(m_caster->getLevel());
        pTotem->setFaction(m_caster->getFaction());
        pTotem->SetOwnerGuid(m_caster->GetObjectGuid());
    }

    pTotem->SetTypeBySummonSpell(
        m_spellInfo); // must be after Create call where m_spells initialized

    pTotem->SetDuration(m_duration);

    if (damage) // if not spell info, DB values used
    {
        pTotem->SetMaxHealth(damage);
        pTotem->SetHealth(damage);
    }

    pTotem->SetUInt32Value(UNIT_CREATED_BY_SPELL, m_spellInfo->Id);

    if (!pTotem->GetCreatorGuid().IsEmpty() &&
        m_caster->GetTypeId() == TYPEID_PLAYER)
        pTotem->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PVP_ATTACKABLE);

    if (m_caster->IsPvP())
        pTotem->SetPvP(true);

    if (!pTotem->Summon(m_caster))
    {
        delete pTotem;
        return;
    }

    if (slot < MAX_TOTEM_SLOT)
        m_caster->_AddTotem(TotemSlot(slot), pTotem);

    // sending SMSG_TOTEM_CREATED before add to map (done in Summon)
    if (slot < MAX_TOTEM_SLOT && m_caster->GetTypeId() == TYPEID_PLAYER)
    {
        WorldPacket data(SMSG_TOTEM_CREATED, 1 + 8 + 4 + 4);
        data << uint8(slot);
        data << pTotem->GetObjectGuid();
        data << uint32(m_duration);
        data << uint32(m_spellInfo->Id);
        ((Player*)m_caster)->SendDirectMessage(std::move(data));
    }

    if (m_caster->GetTypeId() == TYPEID_UNIT && m_caster->isInCombat())
    {
        auto guid = pTotem->GetObjectGuid();
        auto caster = m_caster;
        caster->queue_action(0, [caster, guid]()
            {
                auto totem = caster->GetMap()->GetUnit(guid);
                if (!totem || !caster->isInCombat())
                    return;
                static_cast<Creature*>(caster)->combat_summons.push_back(
                    totem->GetObjectGuid());
            });
    }

    summoned_target_ = pTotem->GetObjectGuid();
}

void Spell::EffectEnchantHeldItem(SpellEffectIndex eff_idx)
{
    // this is only item spell effect applied to main-hand weapon of target
    // player (players in area)
    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
        return;

    Player* item_owner = (Player*)unitTarget;
    Item* item =
        item_owner->storage().get(inventory::slot(inventory::personal_slot,
            inventory::main_bag, inventory::main_hand_e)); // XXX

    if (!item)
        return;
    // must be equipped
    if (!item->IsEquipped())
        return;

    if (m_spellInfo->EffectMiscValue[eff_idx])
    {
        uint32 enchant_id = m_spellInfo->EffectMiscValue[eff_idx];
        int32 duration =
            GetSpellDuration(m_spellInfo); // Try duration index first...
        if (!duration)
            duration = m_currentBasePoints[eff_idx]; // Base points after...
        if (!duration)
            duration =
                10; // 10 seconds for enchants which don't have listed duration

        SpellItemEnchantmentEntry const* pEnchant =
            sSpellItemEnchantmentStore.LookupEntry(enchant_id);
        if (!pEnchant)
            return;

        // Always go to temp enchantment slot
        EnchantmentSlot slot = TEMP_ENCHANTMENT_SLOT;

        // Enchantment will not be applied if a different one already exists
        if (item->GetEnchantmentId(slot) &&
            item->GetEnchantmentId(slot) != enchant_id)
            return;

        // Apply the temporary enchantment
        item->SetEnchantment(slot, enchant_id, duration * IN_MILLISECONDS, 0);
        item_owner->ApplyEnchantment(item, slot, true, item->slot());
    }
}

void Spell::EffectDisEnchant(SpellEffectIndex /*eff_idx*/)
{
    if (m_caster->GetTypeId() != TYPEID_PLAYER)
        return;

    Player* p_caster = (Player*)m_caster;
    if (!itemTarget || !itemTarget->GetProto()->DisenchantID)
        return;

    p_caster->UpdateCraftSkill(m_spellInfo->Id);

    ((Player*)m_caster)
        ->SendLoot(itemTarget->GetObjectGuid(), LOOT_DISENCHANTING);

    // item will be removed at disenchanting end
}

void Spell::EffectInebriate(SpellEffectIndex /*eff_idx*/)
{
    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
        return;

    Player* player = (Player*)unitTarget;
    uint16 currentDrunk = player->GetDrunkValue();
    uint16 drunkMod = damage * 256;
    if (currentDrunk + drunkMod > 0xFFFF)
        currentDrunk = 0xFFFF;
    else
        currentDrunk += drunkMod;
    player->SetDrunkValue(
        currentDrunk, m_CastItem ? m_CastItem->GetEntry() : 0);
}

void Spell::EffectFeedPet(SpellEffectIndex eff_idx)
{
    if (m_caster->GetTypeId() != TYPEID_PLAYER)
        return;

    Player* _player = (Player*)m_caster;

    Item* foodItem = itemTarget;
    if (!foodItem)
        return;

    Pet* pet = _player->GetPet();
    if (!pet)
        return;

    if (!pet->isAlive())
        return;

    int32 benefit =
        pet->GetCurrentFoodBenefitLevel(foodItem->GetProto()->ItemLevel);
    if (benefit <= 0)
        return;

    // XXX
    InventoryResult err = _player->storage().remove_count(foodItem, 1);
    if (err != EQUIP_ERR_OK)
        return _player->SendEquipError(err, foodItem);

    // TODO: fix crash when a spell has two effects, both pointed at the same
    // item target

    m_caster->CastCustomSpell(m_caster,
        m_spellInfo->EffectTriggerSpell[eff_idx], &benefit, nullptr, nullptr,
        true);
}

void Spell::EffectDismissPet(SpellEffectIndex /*eff_idx*/)
{
    if (m_caster->GetTypeId() != TYPEID_PLAYER)
        return;

    Pet* pet = m_caster->GetPet();

    // not let dismiss dead pet
    if (!pet || !pet->isAlive())
        return;

    pet->Unsummon(PET_SAVE_DISMISS_PET, m_caster);
}

void Spell::EffectSummonObject(SpellEffectIndex eff_idx)
{
    uint32 go_id = m_spellInfo->EffectMiscValue[eff_idx];

    uint8 slot = 0;
    switch (m_spellInfo->Effect[eff_idx])
    {
    case SPELL_EFFECT_SUMMON_OBJECT_SLOT1:
        slot = 0;
        break;
    case SPELL_EFFECT_SUMMON_OBJECT_SLOT2:
        slot = 1;
        break;
    case SPELL_EFFECT_SUMMON_OBJECT_SLOT3:
        slot = 2;
        break;
    case SPELL_EFFECT_SUMMON_OBJECT_SLOT4:
        slot = 3;
        break;
    default:
        return;
    }

    if (ObjectGuid guid = m_caster->m_ObjectSlotGuid[slot])
    {
        if (GameObject* obj =
                m_caster ? m_caster->GetMap()->GetGameObject(guid) : nullptr)
            obj->SetLootState(GO_JUST_DEACTIVATED);
        m_caster->m_ObjectSlotGuid[slot].Clear();
    }

    auto pGameObj = new GameObject;

    float x, y, z;
    // If dest location if present
    if (m_targets.m_targetMask & TARGET_FLAG_DEST_LOCATION)
    {
        x = m_targets.m_destX;
        y = m_targets.m_destY;
        z = m_targets.m_destZ;
    }
    // Summon in front of caster
    else
    {
        auto pos = m_caster->GetPoint(0.0f,
            DEFAULT_BOUNDING_RADIUS + m_caster->GetObjectBoundingRadius(),
            true);
        x = pos.x;
        y = pos.y;
        z = pos.z;
    }

    Map* map = m_caster->GetMap();
    if (!pGameObj->Create(map->GenerateLocalLowGuid(HIGHGUID_GAMEOBJECT), go_id,
            map, x, y, z, m_caster->GetO(), 0.0f, 0.0f, 0.0f, 0.0f,
            GO_ANIMPROGRESS_DEFAULT, GO_STATE_READY))
    {
        delete pGameObj;
        return;
    }

    pGameObj->SetUInt32Value(GAMEOBJECT_LEVEL, m_caster->getLevel());
    int32 duration = GetSpellDuration(m_spellInfo);
    pGameObj->SetRespawnTime(duration > 0 ? duration / IN_MILLISECONDS : 0);
    pGameObj->SetSpellId(m_spellInfo->Id);
    pGameObj->SetTemporary(true);

    if (!map->insert(pGameObj))
    {
        delete pGameObj;
        return;
    }

    m_caster->AddGameObject(pGameObj);

    WorldPacket data(SMSG_GAMEOBJECT_SPAWN_ANIM_OBSOLETE, 8);
    data << ObjectGuid(pGameObj->GetObjectGuid());
    m_caster->SendMessageToSet(&data, true);

    m_caster->m_ObjectSlotGuid[slot] = pGameObj->GetObjectGuid();

    auto caster_guid = m_caster->GetObjectGuid();
    auto orig_guid = m_caster->GetObjectGuid();
    pGameObj->queue_action(0, [pGameObj, caster_guid, orig_guid]()
        {
            auto caster = pGameObj->GetMap()->GetUnit(caster_guid);
            auto orig_caster = pGameObj->GetMap()->GetUnit(orig_guid);
            pGameObj->SummonLinkedTrapIfAny();

            if (caster && caster->GetTypeId() == TYPEID_UNIT &&
                ((Creature*)caster)->AI())
                ((Creature*)caster)->AI()->JustSummoned(pGameObj);
            if (orig_caster && orig_caster != caster &&
                orig_caster->GetTypeId() == TYPEID_UNIT &&
                ((Creature*)orig_caster)->AI())
                ((Creature*)orig_caster)->AI()->JustSummoned(pGameObj);
        });
}

void Spell::EffectResurrect(SpellEffectIndex /*eff_idx*/)
{
    if (!unitTarget)
        return;
    if (unitTarget->GetTypeId() != TYPEID_PLAYER)
        return;

    if (unitTarget->isAlive())
        return;
    if (!unitTarget->IsInWorld())
        return;

    switch (m_spellInfo->Id)
    {
    // Defibrillate (Goblin Jumper Cables) have 33% chance on success
    case 8342:
        if (roll_chance_i(67))
        {
            m_caster->CastSpell(m_caster, 8338, true, m_CastItem);
            return;
        }
        break;
    // Defibrillate (Goblin Jumper Cables XL) have 50% chance on success
    case 22999:
        if (roll_chance_i(50))
        {
            m_caster->CastSpell(m_caster, 23055, true, m_CastItem);
            return;
        }
        break;
    default:
        break;
    }

    Player* pTarget = ((Player*)unitTarget);

    if (pTarget->isRessurectRequested()) // already have one active request
        return;

    uint32 health = pTarget->GetMaxHealth() * damage / 100;
    uint32 mana = pTarget->GetMaxPower(POWER_MANA) * damage / 100;

    pTarget->setResurrectRequestData(m_caster->GetObjectGuid(),
        m_caster->GetMapId(), m_caster->GetX(), m_caster->GetY(),
        m_caster->GetZ(), health, mana);
    SendResurrectRequest(pTarget);
}

void Spell::EffectAddExtraAttacks(SpellEffectIndex /*eff_idx*/)
{
    if (!unitTarget || !unitTarget->isAlive())
        return;

    // Fill attack data
    WhiteAttack attack;
    // TODO: Is it true these attacks are always on main hand?
    attack.weaponAttackType = BASE_ATTACK;
    attack.extraAttackSpellId = m_spellInfo->Id;

    bool isInstant;

    // Some attack data depends on the attack in question
    switch (m_spellInfo->Id)
    {
    /* Spells that can not proc new extra attacks */
    // Instant ones
    case 32746: // Reckoning (the actual attack is instant and not on next)
    case 16459: // All ranks of sword specialization (Rogue and Warrior)
        attack.extraAttackType = EXTRA_ATTACK_PROC_NONE;
        attack.onlyTriggerOnNormalSwing = false;
        isInstant = true;
        break;
    // On next attack ones

    /* Spells that can proc other extra attacks, but not themselves */
    // Instant ones
    case 15601: // Hand of Justice
    case 8516:  // Windfury Totem rank 1
    case 10608: // Windfury Totem rank 2
    case 10610: // Windfury Totem rank 3
    case 25583: // Windfury Totem rank 4
    case 25584: // Windfury Totem rank 5
    case 38229: // Windfury (Serpenthsrine Cavern)
        attack.extraAttackType = EXTRA_ATTACK_PROC_OTHERS;
        attack.onlyTriggerOnNormalSwing = false;
        isInstant = true;
        break;
    // On next attack ones
    case 15494: // Fury of Forgewright
    case 18797: // Flurry Axe
    case 21919: // Thrash Blade
    case 38308: // Blinkstrike
        attack.extraAttackType = EXTRA_ATTACK_PROC_OTHERS;
        attack.onlyTriggerOnNormalSwing = true;
        isInstant = false;
        break;

    /* Spells that can proc other extra attacks, including themselves */
    // Instant ones
    // Pre patch 2.2:
    /*case 16459:                                             // All ranks of
       sword specialization (Rogue and Warrior)
        attack.extraAttackType = EXTRA_ATTACK_PROC_ALL;
        attack.onlyTriggerOnNormalSwing = false;
        isInstant = true;
        break;*/
    // On next attack ones

    default:
        // Assume on-next attack and that it cannot proc more extra attacks
        attack.extraAttackType = EXTRA_ATTACK_PROC_NONE;
        attack.onlyTriggerOnNormalSwing = true;
        isInstant = false;
        break;
    }

    // Skip instant attack if we have no victim
    if (isInstant &&
        (!unitTarget->getVictim() || !unitTarget->getVictim()->isAlive()))
        return;

    // Add attacks to queue
    for (int i = 0; i < damage; ++i)
        m_caster->QueueWhiteAttack(attack);

    // Cause an AttackerStateUpdate if the extra attack should happen right away
    // Note that if we're already locked, this attack will be
    // processed in the current iteration of AttackerStateUpdate
    if (isInstant && !unitTarget->attackerStateLock)
    {
        unitTarget->attackerStateLock = true;
        unitTarget->AttackerStateUpdate(unitTarget->getVictim(), false);
        unitTarget->attackerStateLock = false;
    }
}

void Spell::EffectParry(SpellEffectIndex /*eff_idx*/)
{
    if (unitTarget && unitTarget->GetTypeId() == TYPEID_PLAYER)
        ((Player*)unitTarget)->SetCanParry(true);
}

void Spell::EffectBlock(SpellEffectIndex /*eff_idx*/)
{
    if (unitTarget && unitTarget->GetTypeId() == TYPEID_PLAYER)
        ((Player*)unitTarget)->SetCanBlock(true);
}

void Spell::EffectHelperBlink()
{
    std::string perf;

    int dist = 20;
    bool found_pos = false;
    G3D::Vector3 pos;

    while (dist > 0 && !found_pos)
    {
        // blink always goes directly forward
        pos = m_caster->GetPoint(0.0f, (float)dist, true, true);
        // check so we don't blink through closed doors
        if (m_caster->GetMap()->isInDynLineOfSight(m_caster->GetX(),
                m_caster->GetY(), m_caster->GetZ() + 2.0f, pos.x, pos.y,
                pos.z + 2.0f))
            found_pos = true;
        dist -= 5;
    }

    if (!found_pos)
        pos =
            G3D::Vector3(m_caster->GetX(), m_caster->GetY(), m_caster->GetZ());

    m_caster->NearTeleportTo(pos.x, pos.y, pos.z, m_caster->GetO());
}

void Spell::EffectLeapForward(SpellEffectIndex eff_idx)
{
    if (unitTarget->IsTaxiFlying())
        return;

    if (m_spellInfo->rangeIndex == 1) // self range
    {
        float dist = GetSpellRadius(sSpellRadiusStore.LookupEntry(
            m_spellInfo->EffectRadiusIndex[eff_idx]));
        // Mage's Blink
        if (m_spellInfo->Id == 1953)
        {
            EffectHelperBlink();
        }
        else
        {
            // teleport forward
            auto pos = unitTarget->GetPoint(
                0.0f, dist, m_caster->GetTypeId() == TYPEID_PLAYER);
            unitTarget->NearTeleportTo(pos.x, pos.y, pos.z, unitTarget->GetO(),
                unitTarget == m_caster);
        }
    }
}

void Spell::EffectLeapBack(SpellEffectIndex eff_idx)
{
    if (!m_caster || m_caster->GetTypeId() != TYPEID_PLAYER)
        return;

    if (unitTarget->IsTaxiFlying())
        return;

    ((Player*)m_caster)
        ->KnockBackFrom(unitTarget,
            float(m_spellInfo->EffectMiscValue[eff_idx]) / 10,
            float(damage) / 10);
}

void Spell::EffectReputation(SpellEffectIndex eff_idx)
{
    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
        return;

    Player* _player = (Player*)unitTarget;

    int32 rep_change = m_currentBasePoints[eff_idx];
    uint32 faction_id = m_spellInfo->EffectMiscValue[eff_idx];

    FactionEntry const* factionEntry = sFactionStore.LookupEntry(faction_id);

    if (!factionEntry)
        return;

    rep_change = _player->CalculateReputationGain(
        REPUTATION_SOURCE_SPELL, rep_change, faction_id);

    _player->GetReputationMgr().ModifyReputation(factionEntry, rep_change);
}

void Spell::EffectQuestComplete(SpellEffectIndex eff_idx)
{
    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
        return;

    uint32 quest_id = m_spellInfo->EffectMiscValue[eff_idx];
    ((Player*)unitTarget)->AreaExploredOrEventHappens(quest_id);
}

void Spell::EffectSelfResurrect(SpellEffectIndex eff_idx)
{
    if (!unitTarget || unitTarget->isAlive())
        return;
    if (unitTarget->GetTypeId() != TYPEID_PLAYER)
        return;
    if (!unitTarget->IsInWorld())
        return;

    uint32 health = 0;
    uint32 mana = 0;

    // flat case
    if (damage < 0)
    {
        health = uint32(-damage);
        mana = m_spellInfo->EffectMiscValue[eff_idx];
    }
    // percent case
    else
    {
        health = uint32(damage / 100.0f * unitTarget->GetMaxHealth());
        if (unitTarget->GetMaxPower(POWER_MANA) > 0)
            mana =
                uint32(damage / 100.0f * unitTarget->GetMaxPower(POWER_MANA));
    }

    Player* plr = ((Player*)unitTarget);
    plr->ResurrectPlayer(0.0f);

    plr->SetHealth(health);
    plr->SetPower(POWER_MANA, mana);
    plr->SetPower(POWER_RAGE, 0);
    plr->SetPower(POWER_ENERGY, plr->GetMaxPower(POWER_ENERGY));

    plr->SpawnCorpseBones();
}

void Spell::EffectSkinning(SpellEffectIndex /*eff_idx*/)
{
    if (unitTarget->GetTypeId() != TYPEID_UNIT)
        return;
    if (!m_caster || m_caster->GetTypeId() != TYPEID_PLAYER)
        return;

    Creature* creature = (Creature*)unitTarget;
    int32 targetLevel = creature->getLevel();

    uint32 skill = creature->GetCreatureInfo()->GetRequiredLootSkill();

    if (!creature->GetLootDistributor() ||
        creature->GetLootDistributor()->loot_type() != LOOT_SKINNING)
        return;
    creature->GetLootDistributor()->recipient_mgr()->add_solo_tap(
        static_cast<Player*>(m_caster));
    static_cast<Player*>(m_caster)->SendLoot(
        creature->GetObjectGuid(), LOOT_SKINNING);

    int32 reqValue = targetLevel < 10 ? 0 : targetLevel < 20 ?
                                        (targetLevel - 10) * 10 :
                                        targetLevel * 5;

    int32 skillValue = ((Player*)m_caster)->GetPureSkillValue(skill);

    // Double chances for elites
    // NOTE: Mining/herbalism cannot be leveled by skinning mobs
    if (skill == SKILL_SKINNING)
        static_cast<Player*>(m_caster)->UpdateGatherSkill(
            skill, skillValue, reqValue, creature->IsElite() ? 2 : 1);

    // remove skinning flag
    creature->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SKINNABLE);
}

void Spell::EffectCharge(SpellEffectIndex /*eff_idx*/)
{
    if (!unitTarget || pregenerated_path.empty())
        return;

    if (unitTarget->GetTypeId() != TYPEID_PLAYER)
        ((Creature*)unitTarget)->StopMoving();

    // reset falling position to end of charge path
    if (m_caster->GetTypeId() == TYPEID_PLAYER)
        static_cast<Player*>(m_caster)->SetFallInformation(
            0, pregenerated_path[pregenerated_path.size() - 1].z);

    auto speed =
        m_spellInfo->speed > 0.0f ? m_spellInfo->speed : CHARGE_DEFAULT_SPEED;

    uint32 mh_timer = 1000.0f * m_caster->GetDistance(unitTarget) / speed + 500;

    if (m_caster->getAttackTimer(BASE_ATTACK) < mh_timer)
        m_caster->setAttackTimer(BASE_ATTACK, mh_timer);

    if (m_caster->getAttackTimer(OFF_ATTACK) < mh_timer + ATTACK_DISPLAY_DELAY)
        m_caster->setAttackTimer(OFF_ATTACK, mh_timer + ATTACK_DISPLAY_DELAY);

    m_caster->movement_gens.push(new movement::ChargeMovementGenerator(
        pregenerated_path, unitTarget->GetObjectGuid(), speed));

    // not all charge effects used in negative spells
    if (unitTarget != m_caster && !IsPositiveSpell(m_spellInfo->Id) &&
        (m_caster->GetTypeId() != TYPEID_UNIT ||
            static_cast<Creature*>(m_caster)->behavior() == nullptr))
        m_caster->Attack(unitTarget, true);
}

void Spell::EffectCharge2(SpellEffectIndex /*eff_idx*/)
{
    /* Unused spell effect */
}

void Spell::DoSummonCritter(SpellEffectIndex eff_idx, uint32 forceFaction)
{
    if (m_caster->GetTypeId() != TYPEID_PLAYER)
        return;
    Player* player = (Player*)m_caster;

    uint32 pet_entry = m_spellInfo->EffectMiscValue[eff_idx];
    if (!pet_entry)
        return;

    CreatureInfo const* cInfo =
        sCreatureStorage.LookupEntry<CreatureInfo>(pet_entry);
    if (!cInfo)
    {
        logging.error(
            "Spell::DoSummonCritter: creature entry %u not found for spell %u.",
            pet_entry, m_spellInfo->Id);
        return;
    }

    Pet* old_critter = player->GetMiniPet();

    // for same pet just despawn
    if (old_critter && old_critter->GetEntry() == pet_entry)
    {
        player->RemoveMiniPet();
        return;
    }

    // despawn old pet before summon new
    if (old_critter)
        player->RemoveMiniPet();

    CreatureCreatePos pos(m_caster->GetMap(), m_targets.m_destX,
        m_targets.m_destY, m_targets.m_destZ, m_caster->GetO());
    if (!(m_targets.m_targetMask & TARGET_FLAG_DEST_LOCATION))
        pos = CreatureCreatePos(m_caster, m_caster->GetO());

    // summon new pet
    auto critter = new Pet(MINI_PET);

    Map* map = m_caster->GetMap();
    uint32 pet_number = sObjectMgr::Instance()->GeneratePetNumber();
    if (!critter->Create(
            map->GenerateLocalLowGuid(HIGHGUID_PET), pos, cInfo, pet_number))
    {
        logging.error(
            "Spell::EffectSummonCritter, spellid %u: no such creature entry %u",
            m_spellInfo->Id, pet_entry);
        delete critter;
        return;
    }

    critter->SetSummonPoint(pos);

    // critter->SetName("");                                 // generated by
    // client
    critter->SetOwnerGuid(m_caster->GetObjectGuid());
    critter->SetCreatorGuid(m_caster->GetObjectGuid());

    critter->SetUInt32Value(UNIT_CREATED_BY_SPELL, m_spellInfo->Id);
    critter->setFaction(forceFaction ? forceFaction : m_caster->getFaction());
    critter->SelectLevel(critter->GetCreatureInfo());

    if (!map->insert(critter))
    {
        delete critter;
        return;
    }

    auto caster_guid = m_caster->GetObjectGuid();
    auto orig_guid = m_caster->GetObjectGuid();
    int32 duration = GetSpellDuration(m_spellInfo);
    critter->queue_action(0, [critter, caster_guid, orig_guid, duration]()
        {
            auto caster = critter->GetMap()->GetUnit(caster_guid);
            auto orig_caster = critter->GetMap()->GetUnit(orig_guid);

            critter->AIM_Initialize();
            critter->InitPetCreateSpells();
            critter->SetUInt32Value(
                UNIT_NPC_FLAGS, critter->GetCreatureInfo()->npcflag);

            critter->init_pet_template_data();

            // set timer for unsummon
            if (duration > 0)
                critter->SetDuration(duration);

            if (caster->GetTypeId() == TYPEID_PLAYER)
                static_cast<Player*>(caster)->_SetMiniPet(critter);

            // Notify Summoner
            if (caster->GetTypeId() == TYPEID_UNIT && ((Creature*)caster)->AI())
                ((Creature*)caster)->AI()->JustSummoned(critter);
            if (orig_caster && orig_caster != caster &&
                orig_caster->GetTypeId() == TYPEID_UNIT &&
                ((Creature*)orig_caster)->AI())
                ((Creature*)orig_caster)->AI()->JustSummoned(critter);
            if (critter->AI())
            {
                critter->AI()->SummonedBy(caster);
                if (orig_caster && orig_caster != caster)
                    critter->AI()->SummonedBy(caster);
            }
        });

    summoned_target_ = critter->GetObjectGuid();
}

void Spell::EffectKnockBack(SpellEffectIndex eff_idx)
{
    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
        return;

    float angle = 0;

    // Knockbacks which do not knockback from
    switch (m_spellInfo->Id)
    {
    case 30571: // Quake
    {
        float angle = unitTarget->GetAngle(-22, 2);

        // Low chance to go in random direction
        if (rand_norm_f() < 0.2f)
        {
            switch (urand(0, 5))
            {
            case 0:
                angle += M_PI_F / 4;
                break;
            case 1:
                angle += 3 * M_PI_F / 4;
                break;
            case 2:
                angle += M_PI_F;
                break;
            case 3:
                angle -= M_PI_F / 4;
                break;
            case 4:
                angle -= 3 * M_PI_F / 4;
                break;
            }
        }
        // Otherwise towards wall (away from center point)
        else
        {
            switch (urand(0, 2))
            {
            case 0:
                angle += M_PI_F;
                break;
            case 1:
                angle += 3 * M_PI_F / 4;
                break;
            case 2:
                angle -= 3 * M_PI_F / 4;
                break;
            }
        }

        if (angle < 0)
            angle += 2 * M_PI_F;

        ((Player*)unitTarget)
            ->KnockBack(angle,
                float(m_spellInfo->EffectMiscValue[eff_idx]) / 10.0f,
                float(damage) / 10.0f);
        return;
    }
    default:
        break;
    }

    ((Player*)unitTarget)
        ->KnockBackFrom(m_caster,
            float(m_spellInfo->EffectMiscValue[eff_idx]) / 10,
            float(damage) / 10);
}

void Spell::EffectSendTaxi(SpellEffectIndex eff_idx)
{
    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
        return;

    ((Player*)unitTarget)
        ->ActivateTaxiPathTo(
            m_spellInfo->EffectMiscValue[eff_idx], m_spellInfo->Id);
}

void Spell::EffectPlayerPull(SpellEffectIndex /*eff_idx*/)
{
    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
        return;

    // Temporary Solution. Tried porting Trinitycore's MoveJump and logically
    // all went fine, but the in-game Z was never changed for the players, only
    // the server-side value got properly modified to be a parabolic motion
    // In other words, you just appeared to be running towards the target really
    // fast
    float angle = unitTarget->GetAngle(m_caster);
    float dist = unitTarget->GetDistance(m_caster);
    if (dist < 0.5f)
        return;
    float zSpeed = -11.0f;
    float xySpeed = dist;

    WorldPacket data(SMSG_MOVE_KNOCK_BACK, 9 + 4 + 4 + 4 + 4 + 4);
    data << unitTarget->GetPackGUID();
    data << uint32(0);         // Sequence
    data << float(cos(angle)); // x direction
    data << float(sin(angle)); // y direction
    data << float(xySpeed);    // Horizontal speed
    data << float(zSpeed);     // Z Movement speed (vertical)
    ((Player*)unitTarget)->GetSession()->send_packet(std::move(data));

    if (static_cast<Player*>(unitTarget)->move_validator)
        static_cast<Player*>(unitTarget)->move_validator->knock_back(zSpeed);
}

void Spell::EffectDispelMechanic(SpellEffectIndex eff_idx)
{
    if (!unitTarget)
        return;

    uint32 mechanic = m_spellInfo->EffectMiscValue[eff_idx];
    unitTarget->remove_auras_if([mechanic](AuraHolder* holder)
        {
            return holder->HasMechanic(mechanic);
        });
}

void Spell::EffectSummonDeadPet(SpellEffectIndex /*eff_idx*/)
{
    if (m_caster->GetTypeId() != TYPEID_PLAYER)
        return;
    Player* _player = (Player*)m_caster;
    Pet* pet = _player->GetPet();

    if (!pet)
    {
        // Summon the poor, dead pet :(
        if (_player->HasDeadPet())
        {
            pet = new Pet(SUMMON_PET);
            if (!pet->LoadPetFromDB((Player*)m_caster))
            {
                delete pet;
                return;
            }
        }
        else
            return;
    }
    else if (pet->isAlive())
        return;
    else if (damage < 0)
        return;
    else
    {
        pet->SetUInt32Value(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_NONE);
        pet->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SKINNABLE);
        pet->SetDeathState(ALIVE);
        pet->clearUnitState(UNIT_STAT_ALL_STATE);
        // Teleport pet to player instead of ressurecting on the spot
        pet->NearTeleportTo(m_caster->GetX(), m_caster->GetY(),
            m_caster->GetZ(), m_caster->GetO(), false);
        pet->AIM_Initialize();
    }

    pet->SetHealth(uint32(pet->GetMaxHealth() * (float(damage) / 100)));

    // _player->PetSpellInitialize(); -- action bar not removed at death and not
    // required send at revive
    pet->SavePetToDB(PET_SAVE_AS_CURRENT);
}

void Spell::EffectDestroyAllTotems(SpellEffectIndex /*eff_idx*/)
{
    int32 mana = 0;
    for (int slot = 0; slot < MAX_TOTEM_SLOT; ++slot)
    {
        if (Totem* totem = m_caster->GetTotem(TotemSlot(slot)))
        {
            if (damage)
            {
                uint32 spell_id = totem->GetUInt32Value(UNIT_CREATED_BY_SPELL);
                if (SpellEntry const* spellInfo =
                        sSpellStore.LookupEntry(spell_id))
                {
                    uint32 manacost = spellInfo->manaCost +
                                      m_caster->GetCreateMana() *
                                          spellInfo->ManaCostPercentage / 100;
                    mana += manacost * damage / 100;
                }
            }
            totem->UnSummon();
        }
    }

    if (mana)
        m_caster->CastCustomSpell(
            m_caster, 39104, &mana, nullptr, nullptr, true);
}

void Spell::EffectDurabilityDamage(SpellEffectIndex eff_idx)
{
    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
        return;

    int32 index = m_spellInfo->EffectMiscValue[eff_idx];

    // Note: damage is reversed from durability
    // if damage > 0 it reduces durability on your item(s),
    // if damage < 0 it increases durability on your item(s)
    int32 corrected_dmg = -damage;

    // FIXME: some spells effects have value -1/-2
    // Possibly its mean -1 all player equipped items and -2 all items
    if (index < 0)
    {
        bool include_inventory = index == -2;
        static_cast<Player*>(unitTarget)
            ->durability(false, corrected_dmg, include_inventory);
    }
    else
    {
        inventory::slot slot(
            inventory::personal_slot, inventory::main_bag, index);
        if (Item* item = static_cast<Player*>(unitTarget)->storage().get(slot))
            static_cast<Player*>(unitTarget)
                ->durability(item, false, corrected_dmg);
    }
}

void Spell::EffectDurabilityDamagePCT(SpellEffectIndex eff_idx)
{
    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
        return;

    int32 index = m_spellInfo->EffectMiscValue[eff_idx];

    // Note: there is no spells in 2.4.3 that have a negative damage, in other
    // words no spells are able to "repair" your items percentage-wise. Rather
    // than care for this case if it were to change in a later expansion, we
    // produce an error.
    if (damage < 0)
    {
        logging.error(
            "Spell::EffectDurabilityDamagePCT: Encountered negative damage. "
            "Assumed to be not possible.");
        return;
    }

    // FIXME: some spells effects have value -1/-2
    // Possibly its mean -1 all player equipped items and -2 all items
    if (index < 0)
    {
        bool include_inventory = index == -2;
        static_cast<Player*>(unitTarget)
            ->durability(
                true, -static_cast<double>(damage) / 100, include_inventory);
    }
    else
    {
        inventory::slot slot(
            inventory::personal_slot, inventory::main_bag, index);
        if (Item* item = static_cast<Player*>(unitTarget)->storage().get(slot))
            static_cast<Player*>(unitTarget)
                ->durability(item, true, -static_cast<double>(damage) / 100);
    }
}

void Spell::EffectModifyThreatPercent(SpellEffectIndex /*eff_idx*/)
{
    if (!unitTarget)
        return;

    unitTarget->getThreatManager().modifyThreatPercent(m_caster, damage);
}

void Spell::EffectTransmitted(SpellEffectIndex eff_idx)
{
    uint32 name_id = m_spellInfo->EffectMiscValue[eff_idx];

    GameObjectInfo const* goinfo = ObjectMgr::GetGameObjectInfo(name_id);

    if (!goinfo)
    {
        logging.error(
            "Gameobject (Entry: %u) not exist and not created at spell (ID: "
            "%u) cast",
            name_id, m_spellInfo->Id);
        return;
    }

    float fx, fy, fz;

    if (m_targets.m_targetMask & TARGET_FLAG_DEST_LOCATION)
    {
        fx = m_targets.m_destX;
        fy = m_targets.m_destY;
        fz = m_targets.m_destZ;
    }
    else if (m_spellInfo->EffectRadiusIndex[eff_idx])
    {
        float dis = GetSpellRadius(sSpellRadiusStore.LookupEntry(
            m_spellInfo->EffectRadiusIndex[eff_idx]));
        G3D::Vector3 pos;
        if (unitTarget)
            pos =
                unitTarget->GetPoint(0.0f, DEFAULT_BOUNDING_RADIUS + dis, true);
        else
            pos = m_caster->GetPoint(0.0f, DEFAULT_BOUNDING_RADIUS + dis, true);
        fx = pos.x;
        fy = pos.y;
        fz = pos.z;
    }
    else
    {
        float min_dis = GetSpellMinRange(
            sSpellRangeStore.LookupEntry(m_spellInfo->rangeIndex));
        float max_dis = GetSpellMaxRange(
            sSpellRangeStore.LookupEntry(m_spellInfo->rangeIndex));
        float dis = rand_norm_f() * (max_dis - min_dis) + min_dis;

        // special code for fishing bobber (TARGET_SELF_FISHING), should not try
        // to avoid objects
        // nor try to find ground level, but randomly vary in angle
        if (goinfo->type == GAMEOBJECT_TYPE_FISHINGNODE)
        {
            // calculate angle variation for roughly equal dimensions of target
            // area
            float max_angle = (max_dis - min_dis) /
                              (max_dis + m_caster->GetObjectBoundingRadius());
            float angle_offset = max_angle * (rand_norm_f() - 0.5f);
            auto pos = m_caster->GetPoint2d(angle_offset, dis);
            fx = pos.x;
            fy = pos.y;

            bool cancel = false;

            GridMapLiquidData liqData;
            if (!m_caster->GetTerrain()->IsInWater(
                    fx, fy, m_caster->GetZ() + 1.f, &liqData))
            {
                SendCastResult(SPELL_FAILED_NOT_FISHABLE);
                cancel = true;
            }

            fz = liqData.level;
            // finally, check LoS
            if (!m_caster->IsWithinWmoLOS(fx, fy, fz + 2.0f))
            {
                SendCastResult(SPELL_FAILED_LINE_OF_SIGHT);
                cancel = true;
            }

            if (cancel)
            {
                auto tar = unitTarget;
                tar->queue_action(0, [tar, this]()
                    {
                        if (tar->GetCurrentSpell(CURRENT_CHANNELED_SPELL) ==
                            this)
                        {
                            tar->GetCurrentSpell(CURRENT_CHANNELED_SPELL)
                                ->cancel();
                        }
                    });
                return;
            }
        }
        else
        {
            G3D::Vector3 pos;
            if (unitTarget)
                pos = unitTarget->GetPoint(
                    0.0f, DEFAULT_BOUNDING_RADIUS + dis, true);
            else
                pos = m_caster->GetPoint(
                    0.0f, DEFAULT_BOUNDING_RADIUS + dis, true);
            fx = pos.x;
            fy = pos.y;
            fz = pos.z;
        }
    }

    Map* cMap = m_caster->GetMap();

    // if gameobject is summoning object, it should be spawned right on caster's
    // position
    if (goinfo->type == GAMEOBJECT_TYPE_SUMMONING_RITUAL)
    {
        // Meeting stone, to prevent the portal going into the meeting stone
        // game object
        if (goinfo->id == 179944)
            m_caster->GetPosition(fx, fy, fz);
        else
        {
            float radius = 2.5f; // dbc values spawn the object too far or too
                                 // close, this hardcoded value seems about
                                 // right
            auto pos = m_caster->GetPoint(0.0f, radius, true);
            fx = pos.x;
            fy = pos.y;
            fz = pos.z;
        }
    }

    auto pGameObj = new GameObject;

    if (!pGameObj->Create(cMap->GenerateLocalLowGuid(HIGHGUID_GAMEOBJECT),
            name_id, cMap, fx, fy, fz, m_caster->GetO()))
    {
        delete pGameObj;
        return;
    }

    int32 duration = GetSpellDuration(m_spellInfo);

    switch (goinfo->type)
    {
    case GAMEOBJECT_TYPE_FISHINGNODE:
    {
        m_caster->SetChannelObjectGuid(pGameObj->GetObjectGuid());
        m_caster->AddGameObject(pGameObj); // will removed at spell cancel

        // end time of range when possible catch fish
        // (FISHING_BOBBER_READY_TIME..GetDuration(m_spellInfo))
        // start time == fish-FISHING_BOBBER_READY_TIME
        // (0..GetDuration(m_spellInfo)-FISHING_BOBBER_READY_TIME)
        int32 lastSec = 0;
        switch (urand(0, 3))
        {
        case 0:
            lastSec = 3;
            break;
        case 1:
            lastSec = 7;
            break;
        case 2:
            lastSec = 13;
            break;
        case 3:
            lastSec = 17;
            break;
        }

        duration = duration - lastSec * IN_MILLISECONDS +
                   FISHING_BOBBER_READY_TIME * IN_MILLISECONDS;
        break;
    }
    case GAMEOBJECT_TYPE_SUMMONING_RITUAL:
    {
        if (m_caster->GetTypeId() == TYPEID_PLAYER)
        {
            pGameObj->AddUniqueUse((Player*)m_caster);
            m_caster->AddGameObject(pGameObj); // will removed at spell cancel
        }
        break;
    }
    case GAMEOBJECT_TYPE_FISHINGHOLE:
    case GAMEOBJECT_TYPE_CHEST:
    default:
        break;
    }

    pGameObj->SetRespawnTime(duration > 0 ? duration / IN_MILLISECONDS : 0);
    pGameObj->SetTemporary(true);

    pGameObj->SetOwnerGuid(m_caster->GetObjectGuid());

    pGameObj->SetUInt32Value(GAMEOBJECT_LEVEL, m_caster->getLevel());
    pGameObj->SetSpellId(m_spellInfo->Id);

    if (!cMap->insert(pGameObj))
    {
        delete pGameObj;
        return;
    }

    Player* target_player;
    if (m_caster->GetTypeId() == TYPEID_PLAYER &&
        (target_player = ObjectAccessor::FindPlayer(
             static_cast<Player*>(m_caster)->GetSelectionGuid())) != nullptr)
        pGameObj->SetRitualTargetGuid(target_player->GetObjectGuid());

    LOG_DEBUG(logging, "AddObject at SpellEfects.cpp EffectTransmitted");
    // m_caster->AddGameObject(pGameObj);
    // m_ObjToDel.push_back(pGameObj);

    auto caster_guid = m_caster->GetObjectGuid();
    auto orig_guid = m_caster->GetObjectGuid();
    pGameObj->queue_action(0, [pGameObj, caster_guid, orig_guid]()
        {
            auto caster = pGameObj->GetMap()->GetUnit(caster_guid);
            auto orig_caster = pGameObj->GetMap()->GetUnit(orig_guid);
            pGameObj->SummonLinkedTrapIfAny();

            if (caster && caster->GetTypeId() == TYPEID_UNIT &&
                ((Creature*)caster)->AI())
                ((Creature*)caster)->AI()->JustSummoned(pGameObj);
            if (orig_caster && orig_caster != caster &&
                orig_caster->GetTypeId() == TYPEID_UNIT &&
                ((Creature*)orig_caster)->AI())
                ((Creature*)orig_caster)->AI()->JustSummoned(pGameObj);
        });
}

void Spell::EffectProspecting(SpellEffectIndex /*eff_idx*/)
{
    if (m_caster->GetTypeId() != TYPEID_PLAYER || !itemTarget)
        return;

    Player* p_caster = (Player*)m_caster;

    if (sWorld::Instance()->getConfig(CONFIG_BOOL_SKILL_PROSPECTING))
    {
        uint32 SkillValue = p_caster->GetPureSkillValue(SKILL_JEWELCRAFTING);
        uint32 reqSkillValue = itemTarget->GetProto()->RequiredSkillRank;
        p_caster->UpdateGatherSkill(
            SKILL_JEWELCRAFTING, SkillValue, reqSkillValue);
    }

    ((Player*)m_caster)
        ->SendLoot(itemTarget->GetObjectGuid(), LOOT_PROSPECTING);
}

void Spell::EffectSkill(SpellEffectIndex /*eff_idx*/)
{
    LOG_DEBUG(logging, "WORLD: SkillEFFECT");
}

void Spell::EffectSpiritHeal(SpellEffectIndex /*eff_idx*/)
{
    // TODO player can't see the heal-animation - he should respawn some ticks
    // later
    if (!unitTarget || unitTarget->isAlive())
        return;
    if (unitTarget->GetTypeId() != TYPEID_PLAYER)
        return;
    if (!unitTarget->IsInWorld())
        return;
    if (m_spellInfo->Id == 22012 && !unitTarget->has_aura(2584))
        return;

    Player* player = static_cast<Player*>(unitTarget);

    player->ResurrectPlayer(1.0f);
    player->SpawnCorpseBones();

    // Resummon pets with full health, wheter they were dead or alive
    // Exclude dismissed or demonic sacrified pets
    if (player->getClass() == CLASS_HUNTER ||
        player->getClass() == CLASS_WARLOCK)
    {
        if (ObjectGuid guid = player->GetBgResummonGuid())
        {
            auto summon = new Pet(SUMMON_PET);
            if (!summon->LoadPetFromDB(player, 0, guid.GetEntry()))
                delete summon;
            else
            {
                // Reset state of pet
                summon->SetHealth(summon->GetMaxHealth());
                summon->SetPower(summon->getPowerType(),
                    summon->GetMaxPower(summon->getPowerType()));
            }
            player->SetBgResummonGuid(ObjectGuid());
        }
    }
}

// remove insignia spell effect
void Spell::EffectSkinPlayerCorpse(SpellEffectIndex /*eff_idx*/)
{
    LOG_DEBUG(logging, "Effect: SkinPlayerCorpse");
    if ((m_caster->GetTypeId() != TYPEID_PLAYER) ||
        (unitTarget->GetTypeId() != TYPEID_PLAYER) || (unitTarget->isAlive()))
        return;

    ((Player*)unitTarget)->RemovedInsignia((Player*)m_caster);
}

void Spell::EffectStealBeneficialBuff(SpellEffectIndex eff_idx)
{
    LOG_DEBUG(logging, "Effect: StealBeneficialBuff");

    if (!unitTarget || unitTarget == m_caster) // can't steal from self
        return;

    DispelHelper(eff_idx, true);
}

void Spell::EffectKillCreditGroup(SpellEffectIndex eff_idx)
{
    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
        return;

    ((Player*)unitTarget)
        ->RewardPlayerAndGroupAtEvent(
            m_spellInfo->EffectMiscValue[eff_idx], unitTarget);
}

void Spell::EffectQuestFail(SpellEffectIndex eff_idx)
{
    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
        return;

    ((Player*)unitTarget)->FailQuest(m_spellInfo->EffectMiscValue[eff_idx]);
}

void Spell::EffectPlaySound(SpellEffectIndex eff_idx)
{
    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
        return;

    uint32 soundId = m_spellInfo->EffectMiscValue[eff_idx];
    if (!sSoundEntriesStore.LookupEntry(soundId))
    {
        logging.error(
            "EffectPlaySound: Sound (Id: %u) in spell %u does not exist.",
            soundId, m_spellInfo->Id);
        return;
    }

    unitTarget->PlayDirectSound(soundId, (Player*)unitTarget);
}

void Spell::EffectPlayMusic(SpellEffectIndex eff_idx)
{
    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
        return;

    uint32 soundId = m_spellInfo->EffectMiscValue[eff_idx];
    if (!sSoundEntriesStore.LookupEntry(soundId))
    {
        logging.error(
            "EffectPlayMusic: Sound (Id: %u) in spell %u does not exist.",
            soundId, m_spellInfo->Id);
        return;
    }

    WorldPacket data(SMSG_PLAY_MUSIC, 4);
    data << uint32(soundId);
    ((Player*)unitTarget)->GetSession()->send_packet(std::move(data));
}

void Spell::EffectBind(SpellEffectIndex /*eff_idx*/)
{
    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
        return;

    Player* player = (Player*)unitTarget;

    uint32 area_id;
    WorldLocation loc;
    player->GetPosition(loc);
    area_id = player->GetAreaId();

    player->SetHomebindToLocation(loc, area_id);

    // binding
    WorldPacket data(SMSG_BINDPOINTUPDATE, (4 + 4 + 4 + 4 + 4));
    data << float(loc.coord_x);
    data << float(loc.coord_y);
    data << float(loc.coord_z);
    data << uint32(loc.mapid);
    data << uint32(area_id);
    player->SendDirectMessage(std::move(data));

    LOG_DEBUG(logging, "New Home Position X is %f", loc.coord_x);
    LOG_DEBUG(logging, "New Home Position Y is %f", loc.coord_y);
    LOG_DEBUG(logging, "New Home Position Z is %f", loc.coord_z);
    LOG_DEBUG(logging, "New Home MapId is %u", loc.mapid);
    LOG_DEBUG(logging, "New Home AreaId is %u", area_id);

    // zone update
    data.initialize(SMSG_PLAYERBOUND, 8 + 4);
    data << player->GetObjectGuid();
    data << uint32(area_id);
    player->SendDirectMessage(std::move(data));
}

void Spell::EffectRedirectThreat(SpellEffectIndex /*eff_idx*/)
{
    if (!unitTarget)
        return;

    m_caster->getHostileRefManager().SetThreatRedirection(
        unitTarget->GetObjectGuid());
}

void Spell::DispelHelper(SpellEffectIndex eff_idx, bool steal_buff)
{
    // Create dispel mask by dispel type
    uint32 dispel_type = m_spellInfo->EffectMiscValue[eff_idx];
    uint32 dispel_mask = GetDispelMask(static_cast<DispelType>(dispel_type));

    std::vector<uint32> failed_dispels;

    // If damage is 0 the count should be 1, otherwise the damage indicates
    // count
    auto success_dispels = unitTarget->get_dispel_buffs(damage ? damage : 1,
        dispel_mask, steal_buff, m_caster, this, &failed_dispels, m_reflected);

    // Send success log and remove the successful dispels
    if (!success_dispels.empty())
    {
        int32 count = success_dispels.size();
        WorldPacket data(steal_buff ? SMSG_SPELLSTEALLOG : SMSG_SPELLDISPELLOG,
            8 + 8 + 4 + 1 + 4 + count * 5);
        data << unitTarget->GetPackGUID(); // Victim GUID
        data << m_caster->GetPackGUID();   // Caster GUID
        data << uint32(m_spellInfo->Id);   // Dispel spell id
        data << uint8(0);                  // not used
        data << uint32(count);             // count
        for (auto& success_dispel : success_dispels)
        {
            AuraHolder* holder = success_dispel.first;
            data << uint32(holder->GetId()); // Spell Id
            data << uint8(0); // (SMSG_SPELLDISPELLOG: 0 - dispelled !=0
                              // cleansed) (SMSG_SPELLSTEALLOG: 0 - steals !=0
                              // transfers)
            if (steal_buff)
                StealAura(holder, unitTarget);
            else
                DispelAura(holder, success_dispel.second, unitTarget);
        }
        m_caster->SendMessageToSet(&data, true);

        // On success dispel
        // Devour Magic
        if (m_spellInfo->SpellFamilyName == SPELLFAMILY_WARLOCK &&
            m_spellInfo->Category == SPELLCATEGORY_DEVOUR_MAGIC)
        {
            uint32 heal_spell = 0;
            switch (m_spellInfo->Id)
            {
            case 19505:
                heal_spell = 19658;
                break;
            case 19731:
                heal_spell = 19732;
                break;
            case 19734:
                heal_spell = 19733;
                break;
            case 19736:
                heal_spell = 19735;
                break;
            case 27276:
                heal_spell = 27278;
                break;
            case 27277:
                heal_spell = 27279;
                break;
            default:
                LOG_DEBUG(logging,
                    "Spell for Devour Magic %d not handled in "
                    "Spell::EffectDispel",
                    m_spellInfo->Id);
                break;
            }
            if (heal_spell)
                m_caster->CastSpell(m_caster, heal_spell, true);
        }
    }
    // Send the spells our dispel failed to dispel
    if (!failed_dispels.empty())
    {
        WorldPacket data(
            SMSG_DISPEL_FAILED, 8 + 8 + 4 + 4 * failed_dispels.size());
        data << m_caster->GetObjectGuid();   // Caster GUID
        data << unitTarget->GetObjectGuid(); // Victim GUID
        data << uint32(m_spellInfo->Id);     // Dispel spell id
        for (auto& failed_dispel : failed_dispels)
            data << uint32(failed_dispel); // Spell Id
        m_caster->SendMessageToSet(&data, true);
    }
}

void Spell::StealAura(AuraHolder* holder, Unit* target)
{
    if (holder->IsDisabled())
        return;

    const SpellEntry* proto = holder->GetSpellProto();
    AuraHolder* new_holder = CreateAuraHolder(proto, m_caster, m_caster);

    // set stolen aura to spell's max duration (capping out at 2 minutes)
    int32 dur = holder->GetAuraDuration();
    int32 max_dur = 2 * MINUTE * IN_MILLISECONDS;
    int32 new_max_dur = max_dur > dur ? dur : max_dur;
    new_holder->SetAuraMaxDuration(new_max_dur);
    new_holder->SetAuraDuration(new_max_dur);

    for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        Aura* aura = holder->GetAura(SpellEffectIndex(i));
        if (!aura)
            continue;

        int32 basePoints = aura->GetBasePoints();
        Aura* new_aura = CreateAura(proto, aura->GetEffIndex(), &basePoints,
            new_holder, m_caster, target);

        // set periodic to do at least one tick
        int32 periodic = aura->GetModifier()->periodictime;
        new_aura->GetModifier()->periodictime =
            periodic < new_max_dur ? periodic : new_max_dur;

        new_holder->AddAura(new_aura, new_aura->GetEffIndex());
    }

    // remove aura as dispelled
    target->RemoveAuraHolder(holder, AURA_REMOVE_BY_DISPEL);

    m_caster->AddAuraHolder(new_holder);
}

void Spell::DispelAura(AuraHolder* holder, uint32 stacks, Unit* target)
{
    if (holder->IsDisabled())
        return;

    const SpellEntry* proto = holder->GetSpellProto();

    // Unstable Affliction
    if (proto->SpellFamilyName == SPELLFAMILY_WARLOCK &&
        proto->SpellFamilyFlags & 0x10000000000)
    {
        // use clean value for initial damage
        int32 damage = proto->CalculateSimpleValue(EFFECT_INDEX_0) * 9;

        // backfire damage and silence
        m_caster->CastCustomSpell(m_caster, 31117, &damage, nullptr, nullptr,
            true, nullptr, nullptr, target->GetObjectGuid());
    }

    if (holder->ModStackAmount(-static_cast<int32>(stacks)))
        target->RemoveAuraHolder(holder, AURA_REMOVE_BY_DISPEL);
}
