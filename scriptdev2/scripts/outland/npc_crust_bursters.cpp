/* Copyright (C) 2014 Corecraft <https://www.worldofcorecraft.com>
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

#include "precompiled.h"
#include "Unit.h"

#define CRUST_BURSTER_EVADE_RANGE 50.0f

enum
{
    // Crust bursters visual spells
    SPELL_FULGORGE_SUBMERGE = 33928, // Makes fulgorge appear under ground
    SPELL_TUNNEL_BORE =
        29147, // Makes mob appear as boring tunnels under ground
    SPELL_GRAY_TUNNEL_BORE =
        37989, // Makes mob appear as boring tunnels under ground (Auchindoun)
    SPELL_FULGORGE_BORE = 34039, // There's no passive spell for this, so we
                                 // hack it by repeatedly casting it in the
                                 // script
    SPELL_SUBMERGE = 37751, // Plays animation that subemerges the crust burster
                            // into the ground
    SPELL_BIRTH =
        35177, // Plays animation that emerges the crust burster from the ground
    SPELL_STAND = 37752, // Makes sure the mob is targettable
    SPELL_SELF_ROOT =
        42716, // Used to allow out of range retargetting in combat

    // Combat spells
    // Poison needs to do more damage
    SPELL_POSION = 31747,
    SPELL_POISON_SPIT = 32330,
    SPELL_BORE = 32738,
    SPELL_TUNNEL = 33932,
    SPELL_ENRAGE = 32714,
};

struct npc_crust_burstersAI : public ScriptedAI
{
    npc_crust_burstersAI(Creature* c) : ScriptedAI(c)
    {
        submerged = false;
        cast_spell = tunnel_bore();
        fulgorge_hack = 0;

        switch (c->GetEntry())
        {
        case 18678: // Fulgorge
        case 16968: // Tunneler
        case 22038: // Hai'shulud
        case 22482: // Mature Bone Sifter (Q Fumping)
            has_tunnel = true;
            break;
        default:
            has_tunnel = false;
            break;
        }

        Reset();
    }

    bool submerged;
    uint32 animation_timer;
    int emerge_hack;
    uint32 cast_spell;
    uint32 fulgorge_hack;
    // combat stuff
    uint32 poison;
    uint32 bore;
    uint32 tunnel;
    uint32 enrage;
    uint32 emerge_timer;
    bool has_tunnel;

    uint32 tunnel_bore()
    {
        switch (m_creature->GetEntry())
        {
        case 18678:
            return SPELL_FULGORGE_SUBMERGE;
        case 21849: // Bone Crawler
            return SPELL_GRAY_TUNNEL_BORE;
        default:
            return SPELL_TUNNEL_BORE;
        }
    }

    void Reset() override
    {
        SetCombatMovement(false);

        animation_timer = 0;
        emerge_hack = 0;

        poison = urand(4000, 8000);
        bore = urand(10000, 20000);
        tunnel = urand(4000, 8000);
        enrage = urand(10000, 20000);
        emerge_timer = 0;
        m_creature->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
    }

    void SummonedBy(WorldObject* sum) override
    {
        if (sum->GetTypeId() == TYPEID_PLAYER)
        {
            switch (m_creature->GetEntry())
            {
            case 22038: // Hai'shulud
            case 22482: // Mature Bone Sifter (Q Fumping)
                AttackStart(static_cast<Unit*>(sum));
                break;
            }
        }
    }

    void JustRespawned() override { submerge(); }

    void EnterEvadeMode(bool by_group = false) override
    {
        ScriptedAI::EnterEvadeMode(by_group);
        switch (m_creature->GetEntry())
        {
        case 22038: // Hai'shulud
        case 22482: // Mature Bone Sifter (Q Fumping)
            m_creature->ForcedDespawn();
            break;
        default:
            submerge();
            break;
        }
    }

    void Aggro(Unit*) override
    {
        m_creature->movement_gens.push(new movement::StoppedMovementGenerator(),
            movement::EVENT_LEAVE_COMBAT);
        m_creature->AddAuraThroughNewHolder(SPELL_SELF_ROOT, m_creature);

        cast_spell = 0;
        emerge();
        m_creature->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
    }

    bool is_submerged() { return submerged; }

    void submerge()
    {
        submerged = true;
        emerge_hack = 0;

        m_creature->CastSpell(m_creature, SPELL_SUBMERGE, false);
        animation_timer = 1500;
        cast_spell = tunnel_bore();

        // Don't continue WP until we're fully submerged
        m_creature->movement_gens.push(
            new movement::StoppedMovementGenerator(1500),
            movement::EVENT_ENTER_COMBAT);
    }

    void emerge()
    {
        submerged = false;

        m_creature->remove_auras(tunnel_bore());
        emerge_hack = 2;

        m_creature->CastSpell(m_creature, SPELL_STAND, true);

        animation_timer = 3000;
    }

    void UpdateAI(const uint32 diff) override
    {
        // Fulgorge red bore hack
        if (m_creature->GetEntry() == 18678)
        {
            if (m_creature->has_aura(SPELL_FULGORGE_SUBMERGE))
            {
                if (fulgorge_hack == 0)
                    fulgorge_hack = 1000;

                if (fulgorge_hack <= diff)
                {
                    m_creature->CastSpell(
                        m_creature, SPELL_FULGORGE_BORE, true);
                    fulgorge_hack = 1000;
                }
                else
                    fulgorge_hack -= diff;
            }
        }

        if (animation_timer)
        {
            if (animation_timer <= diff)
                animation_timer = 0;
            else
                animation_timer -= diff;
        }

        bool animating = animation_timer != 0;

        if (cast_spell && !animating)
        {
            m_creature->CastSpell(m_creature, cast_spell, true);
            cast_spell = 0;
        }

        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (emerge_hack)
        {
            if (--emerge_hack == 0)
            {
                m_creature->CastSpell(m_creature, SPELL_BIRTH, true);
            }
        }

        if (animating)
            return;

        /* Combat Logic */

        if (!m_creature->IsWithinDistInMap(
                m_creature->getVictim(), CRUST_BURSTER_EVADE_RANGE))
        {
            EnterEvadeMode();
            return;
        }

        if (has_tunnel)
        {
            if (tunnel <= diff)
            {
                if (!m_creature->IsNonMeleeSpellCasted(true) &&
                    CanCastSpell(m_creature, SPELL_TUNNEL, true) == CAST_OK)
                {
                    m_creature->CastSpell(m_creature, SPELL_TUNNEL, true);
                    animation_timer = 3000;
                    tunnel = urand(15000, 25000);
                    return; // Don't do any further combat logic
                }
                else
                    return; // wait for spell to connect if tunnel is ready
            }
            else
                tunnel -= diff;
        }

        // Tunneler casts enrage
        if (m_creature->GetEntry() == 16968)
        {
            if (enrage <= diff)
            {
                if (DoCastSpellIfCan(m_creature, SPELL_ENRAGE) == CAST_OK)
                    enrage = urand(10000, 20000);
            }
            else
                enrage -= diff;
        }

        if (m_creature->CanReachWithMeleeAttack(m_creature->getVictim()))
        {
            if (poison <= diff)
            {
                // Fulgorge uses a different poison
                if (DoCastSpellIfCan(m_creature->getVictim(),
                        m_creature->GetEntry() == 18678 ?
                            SPELL_POISON_SPIT :
                            SPELL_POSION) == CAST_OK)
                    poison = urand(5000, 10000);
            }
            else
                poison -= diff;

            if (bore <= diff)
            {
                if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_BORE) ==
                    CAST_OK)
                    bore = urand(10000, 20000);
            }
            else
                bore -= diff;

            DoMeleeAttackIfReady();
        }
        else
        {
            // Spam poison if out of range (Fulgorge uses a different poison)
            if (!m_creature->IsNonMeleeSpellCasted(false))
                DoCastSpellIfCan(m_creature->getVictim(),
                    m_creature->GetEntry() == 18678 ? SPELL_POISON_SPIT :
                                                      SPELL_POSION);
        }
    }
};

CreatureAI* GetAI_npc_crust_bursters(Creature* c)
{
    return new npc_crust_burstersAI(c);
}

void AddSC_crust_busters()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "npc_crust_bursters";
    pNewScript->GetAI = GetAI_npc_crust_bursters;
    pNewScript->RegisterSelf();
}
