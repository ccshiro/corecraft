/*
 * Copyright (C) 2013 CoreCraft <https://www.worldofcorecraft.com/>
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

#ifndef MANGOS_PLAYERCHARMAICLASSBEHAVIOR_H
#define MANGOS_PLAYERCHARMAICLASSBEHAVIOR_H

#include "Common.h"

class Player;

enum PCAITalentPage
{
    PCAI_TALENT_PAGE_NONE, // If no talents are chosen (as in a pre-level 10
                           // character or a character without talents)
    PCAI_TALENT_PAGE_ONE,
    PCAI_TALENT_PAGE_TWO,
    PCAI_TALENT_PAGE_THREE,
};

enum PCAIClassType
{
    PCAI_CT_MELEE,
    PCAI_CT_SPELLCASTER,
    PCAI_CT_HUNTER,
    PCAI_CT_HEALER,
};

class PCAIClassBehavior
{
public:
    PCAIClassBehavior(Player* /*plr*/, bool can_heal_charmer)
      : m_canHealCharmer(can_heal_charmer)
    {
    }
    virtual ~PCAIClassBehavior() {}

    // Stuff you must override to inherit
    virtual PCAIClassType GetClassType() = 0;
    // Stuff you may override
    virtual float GetMaxDist() { return 0.0f; }
    virtual const std::vector<uint32>& GetFormSpells() { return m_formSpells; }
    virtual const std::vector<uint32>& GetCooldownSpells()
    {
        return m_cooldownSpells;
    }
    virtual const std::vector<uint32>& GetDpsSpells() { return m_dpsSpells; }
    virtual const std::vector<uint32>& GetDumbSpells() { return m_dumbSpells; }
    virtual const std::vector<uint32>& GetBeneficialSpells()
    {
        return m_beneficialSpells;
    }
    virtual const std::vector<uint32>& GetHealerSpells()
    {
        return m_healerSpells;
    }

    // Calculates which talent tree has the most spent points in it
    void CalculateTalentPage(Player* plr); // Implemented in PlayerCharmAI.cpp

protected:
    PCAITalentPage m_talentPage;
    std::vector<uint32> m_formSpells;     // Casted on self
    std::vector<uint32> m_cooldownSpells; // Casted on self
    std::vector<uint32> m_dpsSpells;      // Casted on victim
    std::vector<uint32> m_dumbSpells; // Casted on self. To make the AI weaker,
                                      // some spells that perform poorly
    std::vector<uint32> m_beneficialSpells; // Casted on charmer. Buffs, etc.
    std::vector<uint32> m_healerSpells;     // Spells used if PCAI_CT_HEALER
    bool m_canHealCharmer;
};

class NullPCAIBehavior : public PCAIClassBehavior
{
public:
    NullPCAIBehavior(Player* plr, bool can_heal_charmer)
      : PCAIClassBehavior(plr, can_heal_charmer)
    {
    }

    PCAIClassType GetClassType() override { return PCAI_CT_MELEE; }
};

/*
 * Warrior
 */
class WarriorPCAIBehavior : public PCAIClassBehavior
{
public:
    WarriorPCAIBehavior(Player* plr, bool can_heal_charmer)
      : PCAIClassBehavior(plr, can_heal_charmer)
    {
        CalculateTalentPage(plr);
    }
    PCAIClassType GetClassType() override { return PCAI_CT_MELEE; }

    const std::vector<uint32>& GetFormSpells() override
    {
        if (m_formSpells.empty())
        {
            if (m_talentPage == PCAI_TALENT_PAGE_ONE)
            {
                m_formSpells.push_back(2458);
                m_formSpells.push_back(2457);
            }
            else if (m_talentPage == PCAI_TALENT_PAGE_TWO)
                m_formSpells.push_back(2458);
            else if (m_talentPage == PCAI_TALENT_PAGE_THREE)
                m_formSpells.push_back(71);
            else
                m_formSpells.push_back(2457);
        }
        return m_formSpells;
    }
    const std::vector<uint32>& GetCooldownSpells() override
    {
        if (m_cooldownSpells.empty())
        {
            m_cooldownSpells.reserve(5);
            m_cooldownSpells.push_back(1719);
            m_cooldownSpells.push_back(20230);
            m_cooldownSpells.push_back(871); // Recklessness, Retaliation and
                                             // Shield Wall (only one used
                                             // depending on form)
            switch (m_talentPage)
            {
            case PCAI_TALENT_PAGE_ONE:
                m_cooldownSpells.push_back(12292);
                m_cooldownSpells.push_back(1680);
                break; // Death Wish & Whirlwind
            case PCAI_TALENT_PAGE_TWO:
                m_cooldownSpells.push_back(12328);
                m_cooldownSpells.push_back(1680);
                break; // Sweeping Strikes & Whirlwind
            case PCAI_TALENT_PAGE_THREE:
                m_cooldownSpells.push_back(12975);
                break; // Last Stand
            default:
                break;
            }
        }
        return m_cooldownSpells;
    }
    const std::vector<uint32>& GetDpsSpells() override
    {
        if (m_dpsSpells.empty())
        {
            m_dpsSpells.reserve(4);
            switch (m_talentPage)
            {
            case PCAI_TALENT_PAGE_ONE:
                m_dpsSpells.push_back(12294);
                break; // Mortal Strike
            case PCAI_TALENT_PAGE_TWO:
                m_dpsSpells.push_back(23881);
                break; // Blood thirst
            case PCAI_TALENT_PAGE_THREE:
                m_dpsSpells.push_back(12809);
                m_dpsSpells.push_back(23922);
                m_dpsSpells.push_back(20243);
                break; // Concussion Blow & Shield Slam & Devastate
            default:
                break;
            }
            m_dpsSpells.push_back(20252);
            m_dpsSpells.push_back(5246);
            m_dpsSpells.push_back(
                78); // Intercept & Intimidating shout & Heroic Strike
        }
        return m_dpsSpells;
    }
    const std::vector<uint32>& GetDumbSpells() override
    {
        if (m_dumbSpells.empty())
        {
            m_dumbSpells.reserve(2);
            m_dumbSpells.push_back(2687);
            m_dumbSpells.push_back(6673); // Bloodrage & Battle Shout
        }
        return m_dumbSpells;
    }
};

/*
 * Rogue
 */
class RoguePCAIBehavior : public PCAIClassBehavior
{
public:
    RoguePCAIBehavior(Player* plr, bool can_heal_charmer)
      : PCAIClassBehavior(plr, can_heal_charmer)
    {
        CalculateTalentPage(plr);
    }
    PCAIClassType GetClassType() override { return PCAI_CT_MELEE; }

    const std::vector<uint32>& GetCooldownSpells() override
    {
        if (m_cooldownSpells.empty())
        {
            m_cooldownSpells.reserve(4);
            if (m_talentPage == PCAI_TALENT_PAGE_TWO)
                m_cooldownSpells.push_back(13750); // Adrenaline Rush
            m_cooldownSpells.push_back(14177);
            m_cooldownSpells.push_back(14185);
            m_cooldownSpells.push_back(5277);
            m_cooldownSpells.push_back(
                13877); // Cold Blood & Preparation & Evasion & Blade Flurry
        }
        return m_cooldownSpells;
    }
    const std::vector<uint32>& GetDpsSpells() override
    {
        if (m_dpsSpells.empty())
        {
            m_dpsSpells.reserve(3);
            if (m_talentPage == PCAI_TALENT_PAGE_ONE)
                m_dpsSpells.push_back(1329); // Mutilate
            m_dpsSpells.push_back(2098);
            m_dpsSpells.push_back(53);
            m_dpsSpells.push_back(
                1752); // Eviscerate & Backstab & Sinister Strike
        }
        return m_dpsSpells;
    }
    const std::vector<uint32>& GetDumbSpells() override
    {
        if (m_dumbSpells.empty())
            m_dumbSpells.push_back(2983); // Sprint
        return m_dumbSpells;
    }
};

/*
 * Paladin
 */
class PaladinPCAIBehavior : public PCAIClassBehavior
{
public:
    PaladinPCAIBehavior(Player* plr, bool can_heal_charmer)
      : PCAIClassBehavior(plr, can_heal_charmer)
    {
        CalculateTalentPage(plr);
    }
    PCAIClassType GetClassType() override
    {
        if (m_canHealCharmer && m_talentPage == PCAI_TALENT_PAGE_ONE)
            return PCAI_CT_HEALER;
        return PCAI_CT_MELEE;
    }
    float GetMaxDist() override
    {
        return GetClassType() == PCAI_CT_HEALER ? 15.0f : 0.0f;
    }

    const std::vector<uint32>& GetCooldownSpells() override
    {
        if (m_cooldownSpells.empty())
        {
            m_cooldownSpells.push_back(31884);
            m_cooldownSpells.push_back(
                31842); // Avenging Wrath & Divine Illumination
        }
        return m_cooldownSpells;
    }
    const std::vector<uint32>& GetDpsSpells() override
    {
        if (m_dpsSpells.empty())
        {
            m_dpsSpells.reserve(4);
            switch (m_talentPage)
            {
            case PCAI_TALENT_PAGE_ONE:
                m_dpsSpells.push_back(20473);
                break; // Holy Shock
            case PCAI_TALENT_PAGE_TWO:
                m_dpsSpells.push_back(31935);
                break; // Avenger's Shield
            case PCAI_TALENT_PAGE_THREE:
                m_dpsSpells.push_back(20066);
                m_dpsSpells.push_back(35395);
                break; // Repentance & Crusader Strike
            default:
                break;
            }
            m_dpsSpells.push_back(20271);
            m_dpsSpells.push_back(853); // Judgement & Hammer of Justice
        }
        return m_dpsSpells;
    }
    const std::vector<uint32>& GetDumbSpells() override
    {
        if (m_dumbSpells.empty())
        {
            m_dumbSpells.reserve(2);
            if (m_talentPage == PCAI_TALENT_PAGE_THREE)
                m_dumbSpells.push_back(20375); // Seal of Command
            else
                m_dumbSpells.push_back(20154); // Seal of Righteousness
            m_dumbSpells.push_back(25780);     // Righteous Fury
        }
        return m_dumbSpells;
    }
    const std::vector<uint32>& GetBeneficialSpells() override
    {
        if (m_beneficialSpells.empty())
        {
            m_beneficialSpells.push_back(1022); // Blessing of Protection
        }
        return m_beneficialSpells;
    }
    const std::vector<uint32>& GetHealerSpells() override
    {
        m_healerSpells.reserve(3);
        if (m_healerSpells.empty())
        {
            m_healerSpells.push_back(20473); // Holy Shock
            m_healerSpells.push_back(19750); // Flash of Light
            m_healerSpells.push_back(635);   // Holy Light
        }
        return m_healerSpells;
    }
};

/*
 * Mage
 */
class MagePCAIBehavior : public PCAIClassBehavior
{
public:
    MagePCAIBehavior(Player* plr, bool can_heal_charmer)
      : PCAIClassBehavior(plr, can_heal_charmer)
    {
        CalculateTalentPage(plr);
    }
    PCAIClassType GetClassType() override { return PCAI_CT_SPELLCASTER; }
    float GetMaxDist() override { return 20.0f; }

    const std::vector<uint32>& GetCooldownSpells() override
    {
        if (m_cooldownSpells.empty())
        {
            m_cooldownSpells.reserve(4);
            m_cooldownSpells.push_back(12051); // Evocation
            switch (m_talentPage)
            {
            case PCAI_TALENT_PAGE_ONE:
                m_cooldownSpells.push_back(12042);
                break; // Arcane Power
            case PCAI_TALENT_PAGE_TWO:
                m_cooldownSpells.push_back(11129);
                break; // Combustion
            case PCAI_TALENT_PAGE_THREE:
                m_cooldownSpells.push_back(11958);
                m_cooldownSpells.push_back(31687);
                m_cooldownSpells.push_back(11426);
                break; // Cold Snap & Summon Water Elemental & Ice Barrier
            default:
                break;
            }
            m_cooldownSpells.push_back(12043);
            m_cooldownSpells.push_back(12472); // Presence of Mind & Icy Veins
        }
        return m_cooldownSpells;
    }
    const std::vector<uint32>& GetDpsSpells() override
    {
        if (m_dpsSpells.empty())
        {
            m_dpsSpells.reserve(2);
            m_dpsSpells.push_back(2136); // Fire blast
            switch (m_talentPage)
            {
            case PCAI_TALENT_PAGE_ONE:
                m_dpsSpells.push_back(30451);
                break; // Arcane Blast
            case PCAI_TALENT_PAGE_TWO:
                m_dpsSpells.push_back(133);
                break; // Fireball
            case PCAI_TALENT_PAGE_THREE:
                m_dpsSpells.push_back(116);
                break; // Frostbolt
            default:
                m_dpsSpells.push_back(133);
                break; // Fireball
            }
        }
        return m_dpsSpells;
    }
    const std::vector<uint32>& GetDumbSpells() override
    {
        if (m_dumbSpells.empty())
        {
            m_dumbSpells.reserve(2);
            if (m_talentPage == PCAI_TALENT_PAGE_TWO)
                m_dumbSpells.push_back(11113); // Blast wave
            m_dumbSpells.push_back(122);
            m_dumbSpells.push_back(1449); // Frost Nova & Arcane Explosion
        }
        return m_dumbSpells;
    }
};

/*
 * Warlock
 */
class WarlockPCAIBehavior : public PCAIClassBehavior
{
public:
    WarlockPCAIBehavior(Player* plr, bool can_heal_charmer)
      : PCAIClassBehavior(plr, can_heal_charmer)
    {
        CalculateTalentPage(plr);
    }
    PCAIClassType GetClassType() override { return PCAI_CT_SPELLCASTER; }
    float GetMaxDist() override { return 30.0f; }

    const std::vector<uint32>& GetDpsSpells() override
    {
        if (m_dpsSpells.empty())
        {
            m_dpsSpells.reserve(4);
            switch (m_talentPage)
            {
            case PCAI_TALENT_PAGE_ONE:
                m_dpsSpells.push_back(30108);
                m_dpsSpells.push_back(980);
                m_dpsSpells.push_back(172);
                m_dpsSpells.push_back(348);
                break; // Unstable Affliction & Curse of Agony & Corruption &
                       // Immolate
            case PCAI_TALENT_PAGE_TWO:
                m_dpsSpells.push_back(980);
                m_dpsSpells.push_back(172);
                break; // Curse of Agony & Corruption
            case PCAI_TALENT_PAGE_THREE:
                m_dpsSpells.push_back(1490);
                m_dpsSpells.push_back(348);
                m_dpsSpells.push_back(17962);
                break; // Curse of the Elements & Immolate & Conflagrate
            default:
                break;
            }
            m_dpsSpells.push_back(686); // Shadow Bolt
        }
        return m_dpsSpells;
    }
    const std::vector<uint32>& GetDumbSpells() override
    {
        if (m_dumbSpells.empty())
            m_dumbSpells.push_back(1949); // Hellfire
        return m_dumbSpells;
    }
};

/*
 * Priest
 */
class PriestPCAIBehavior : public PCAIClassBehavior
{
public:
    PriestPCAIBehavior(Player* plr, bool can_heal_charmer)
      : PCAIClassBehavior(plr, can_heal_charmer)
    {
        CalculateTalentPage(plr);
    }
    PCAIClassType GetClassType() override
    {
        if (m_canHealCharmer && m_talentPage == PCAI_TALENT_PAGE_ONE)
            return PCAI_CT_HEALER;
        else if (m_canHealCharmer && m_talentPage == PCAI_TALENT_PAGE_TWO)
            return PCAI_CT_HEALER;
        return PCAI_CT_SPELLCASTER;
    }
    float GetMaxDist() override { return 30.0f; }

    const std::vector<uint32>& GetFormSpells() override
    {
        if (m_formSpells.empty())
        {
            if (m_talentPage == PCAI_TALENT_PAGE_THREE)
                m_formSpells.push_back(15473); // Shadowform
        }
        return m_formSpells;
    }
    const std::vector<uint32>& GetCooldownSpells() override
    {
        if (m_cooldownSpells.empty())
        {
            if (m_talentPage == PCAI_TALENT_PAGE_ONE)
            {
                m_cooldownSpells.push_back(10060);
                m_cooldownSpells.push_back(33206);
            } // Power Infusion & Pain Suppression
            else if (m_talentPage == PCAI_TALENT_PAGE_TWO)
                m_cooldownSpells.push_back(724); // Lightwell
            m_cooldownSpells.push_back(14751);   // Inner Focus
        }
        return m_cooldownSpells;
    }
    const std::vector<uint32>& GetDpsSpells() override
    {
        if (m_dpsSpells.empty())
        {
            m_dpsSpells.reserve(5);
            m_dpsSpells.push_back(34433); // Shadowfiend
            m_dpsSpells.push_back(589);
            m_dpsSpells.push_back(8092);
            m_dpsSpells.push_back(32379);
            m_dpsSpells.push_back(15407);
            m_dpsSpells.push_back(
                585); // SW:Pain & Mind Blast & SW:Death & Mind Flay & Smite
        }
        return m_dpsSpells;
    }
    const std::vector<uint32>& GetDumbSpells() override
    {
        if (m_dumbSpells.empty())
        {
            m_dumbSpells.reserve(3);
            if (m_talentPage == PCAI_TALENT_PAGE_TWO)
                m_dumbSpells.push_back(15237); // Holy Nova
            m_dumbSpells.push_back(17);
            m_dumbSpells.push_back(586);
            m_dumbSpells.push_back(
                8122); // Power Word: Shield & Fade & Psychic Scream
        }
        return m_dumbSpells;
    }
    const std::vector<uint32>& GetHealerSpells() override
    {
        m_healerSpells.reserve(3);
        if (m_healerSpells.empty())
        {
            m_healerSpells.push_back(2061); // Flash Heal
            m_healerSpells.push_back(2060); // Greater Heal
        }
        return m_healerSpells;
    }
};

/*
 * Shaman
 */
class ShamanPCAIBehavior : public PCAIClassBehavior
{
public:
    ShamanPCAIBehavior(Player* plr, bool can_heal_charmer)
      : PCAIClassBehavior(plr, can_heal_charmer)
    {
        CalculateTalentPage(plr);
    }
    PCAIClassType GetClassType() override
    {
        if (m_canHealCharmer && m_talentPage == PCAI_TALENT_PAGE_THREE)
            return PCAI_CT_HEALER;
        else if (m_talentPage == PCAI_TALENT_PAGE_ONE ||
                 m_talentPage == PCAI_TALENT_PAGE_THREE)
            return PCAI_CT_SPELLCASTER;
        return PCAI_CT_MELEE;
    }
    float GetMaxDist() override
    {
        if (GetClassType() == PCAI_CT_SPELLCASTER)
            return 20.0f;
        else if (GetClassType() == PCAI_CT_HEALER)
            return 30.0f;
        return 0.0f;
    }

    const std::vector<uint32>& GetCooldownSpells() override
    {
        if (m_cooldownSpells.empty())
        {
            m_cooldownSpells.reserve(5);
            m_cooldownSpells.push_back(2825);
            m_cooldownSpells.push_back(32182); // Bloodlust & Heroism
            switch (m_talentPage)
            {
            case PCAI_TALENT_PAGE_ONE:
                m_cooldownSpells.push_back(16166);
                break; // Elemental Mastery
            case PCAI_TALENT_PAGE_TWO:
                m_cooldownSpells.push_back(30823);
                break; // Shamanistic Rage
            case PCAI_TALENT_PAGE_THREE:
                m_cooldownSpells.push_back(16190);
                break; // Mana Tide Totem
            default:
                break;
            }
            m_cooldownSpells.push_back(2894);
            m_cooldownSpells.push_back(
                16188); // Fire Elemental Totem & Nature's Swiftness
        }
        return m_cooldownSpells;
    }
    const std::vector<uint32>& GetDpsSpells() override
    {
        if (m_dpsSpells.empty())
        {
            m_dpsSpells.reserve(3);
            if (m_talentPage == PCAI_TALENT_PAGE_TWO)
            {
                m_dpsSpells.push_back(8056);
                m_dpsSpells.push_back(17364);
            } // Frost Shock & Stormstrike
            else
            {
                m_dpsSpells.push_back(421);
                m_dpsSpells.push_back(8042);
                m_dpsSpells.push_back(403);
            } // Chain Lightning & Earth Shock & Lightning Bolt
        }
        return m_dpsSpells;
    }
    const std::vector<uint32>& GetDumbSpells() override
    {
        if (m_dumbSpells.empty())
            m_dumbSpells.push_back(324); // Lightning Shield
        return m_dumbSpells;
    }
    const std::vector<uint32>& GetHealerSpells() override
    {
        m_healerSpells.reserve(2);
        if (m_healerSpells.empty())
        {
            m_healerSpells.push_back(8004); // Lesser Healing Wave
            m_healerSpells.push_back(331);  // Healing Wave
        }
        return m_healerSpells;
    }
};

/*
 * Hunter
 */
class HunterPCAIBehavior : public PCAIClassBehavior
{
public:
    HunterPCAIBehavior(Player* plr, bool can_heal_charmer)
      : PCAIClassBehavior(plr, can_heal_charmer)
    {
        CalculateTalentPage(plr);
    }
    PCAIClassType GetClassType() override { return PCAI_CT_HUNTER; }
    float GetMaxDist() override { return 30.0f; }

    const std::vector<uint32>& GetCooldownSpells() override
    {
        if (m_cooldownSpells.empty())
        {
            m_cooldownSpells.reserve(2);
            if (m_talentPage == PCAI_TALENT_PAGE_ONE)
            {
                m_cooldownSpells.push_back(19574);
                m_cooldownSpells.push_back(19577);
            } // Bestial Wrath & Intimidation
            else if (m_talentPage == PCAI_TALENT_PAGE_THREE)
                m_cooldownSpells.push_back(23989); // Readiness
            m_cooldownSpells.push_back(19263);
            m_cooldownSpells.push_back(3045); // Deterrence & Rapid Fire
        }
        return m_cooldownSpells;
    }
    const std::vector<uint32>& GetDpsSpells() override
    {
        if (m_dpsSpells.empty())
        {
            m_dpsSpells.reserve(8);
            m_dpsSpells.push_back(75); // Auto shot
            m_dpsSpells.push_back(1130);
            m_dpsSpells.push_back(1978);
            m_dpsSpells.push_back(2643);
            m_dpsSpells.push_back(3044);
            m_dpsSpells.push_back(19434); // Hunter's mark & Serpent Sting &
                                          // Multi-Shot & Arcane Shot &
                                          // Aimed-Shot
            m_dpsSpells.push_back(2973);
            m_dpsSpells.push_back(1495); // Raptor Strike & Mongoose Bite
        }
        return m_dpsSpells;
    }
    const std::vector<uint32>& GetDumbSpells() override
    {
        if (m_dumbSpells.empty())
        {
            m_dumbSpells.reserve(2);
            m_dumbSpells.push_back(13809);
            m_dumbSpells.push_back(1543); // Frost Trap & Flare
        }
        return m_dumbSpells;
    }
};

/*
 * Druid
 */
class DruidPCAIBehavior : public PCAIClassBehavior
{
public:
    DruidPCAIBehavior(Player* plr, bool can_heal_charmer)
      : PCAIClassBehavior(plr, can_heal_charmer)
    {
        CalculateTalentPage(plr);
    }
    PCAIClassType GetClassType() override
    {
        if (m_canHealCharmer && m_talentPage == PCAI_TALENT_PAGE_THREE)
            return PCAI_CT_HEALER;
        else if (m_talentPage == PCAI_TALENT_PAGE_ONE ||
                 m_talentPage == PCAI_TALENT_PAGE_THREE)
            return PCAI_CT_SPELLCASTER;
        return PCAI_CT_MELEE;
    }
    float GetMaxDist() override
    {
        return GetClassType() == PCAI_CT_MELEE ? 0.0f : 30.0f;
    }

    const std::vector<uint32>& GetFormSpells() override
    {
        if (m_formSpells.empty())
        {
            if (m_talentPage == PCAI_TALENT_PAGE_ONE)
                m_formSpells.push_back(24858); // Moonkin
            else if (m_talentPage == PCAI_TALENT_PAGE_TWO)
                m_formSpells.push_back(768); // Cat
            else if (m_talentPage == PCAI_TALENT_PAGE_THREE)
                ; // No form
            else
                m_formSpells.push_back(768); // Cat
        }
        return m_formSpells;
    }
    const std::vector<uint32>& GetCooldownSpells() override
    {
        if (m_cooldownSpells.empty())
        {
            m_cooldownSpells.push_back(29166); // Innervate
            if (m_talentPage != PCAI_TALENT_PAGE_TWO)
                m_cooldownSpells.push_back(17116); // Nature's Swiftness
        }
        return m_cooldownSpells;
    }
    const std::vector<uint32>& GetDpsSpells() override
    {
        if (m_dpsSpells.empty())
        {
            m_dpsSpells.reserve(2);
            switch (m_talentPage)
            {
            case PCAI_TALENT_PAGE_ONE:
                m_dpsSpells.push_back(33831);
                m_dpsSpells.push_back(5570);
                break; // Force of Nature & Insect Swarm
            case PCAI_TALENT_PAGE_TWO:
                m_dpsSpells.push_back(22568);
                m_dpsSpells.push_back(33876);
                m_dpsSpells.push_back(1082);
                break; // Ferocious Bite & Mangle (Cat) & Claw
            case PCAI_TALENT_PAGE_THREE:
                break;
            default:
                break;
            }
            if (m_talentPage != PCAI_TALENT_PAGE_TWO)
            {
                m_dpsSpells.push_back(8921);
                m_dpsSpells.push_back(2912);
            } // Moonfire & Starfire
        }
        return m_dpsSpells;
    }
    const std::vector<uint32>& GetDumbSpells() override
    {
        if (m_dumbSpells.empty())
        {
            m_dumbSpells.push_back(740);
            m_dumbSpells.push_back(22812); // Tranquility & Barkskin
        }
        return m_dumbSpells;
    }
    const std::vector<uint32>& GetHealerSpells() override
    {
        m_healerSpells.reserve(3);
        if (m_healerSpells.empty())
        {
            m_healerSpells.push_back(8936); // Regrowth
            m_healerSpells.push_back(5185); // Healing Touch
        }
        return m_healerSpells;
    }
};

#endif
