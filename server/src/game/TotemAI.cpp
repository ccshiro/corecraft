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

#include "TotemAI.h"
#include "Creature.h"
#include "DBCStores.h"
#include "Map.h"
#include "Pet.h"
#include "Player.h"
#include "SpecialVisCreature.h"
#include "Totem.h"
#include "TemporarySummon.h"
#include "SpellMgr.h"
#include "Totem.h"
#include "maps/visitors.h"

TotemAI::TotemAI(Creature* c) : CreatureAI(c)
{
}

void TotemAI::MoveInLineOfSight(Unit*)
{
}

void TotemAI::EnterEvadeMode(bool by_group)
{
    m_creature->CombatStop(false);

    // process creature evade actions
    m_creature->OnEvadeActions(by_group);
}

void TotemAI::UpdateAI(const uint32 /*diff*/)
{
    if (getTotem().GetTotemType() != TOTEM_ACTIVE)
        return;

    if (!m_creature->isAlive() || m_creature->IsNonMeleeSpellCasted(false))
        return;

    // Search spell
    SpellEntry const* spellInfo =
        sSpellStore.LookupEntry(getTotem().GetSpell());
    if (!spellInfo)
        return;

    // Get spell rangy
    SpellRangeEntry const* srange =
        sSpellRangeStore.LookupEntry(spellInfo->rangeIndex);
    float max_range = GetSpellMaxRange(srange);

    // SPELLMOD_RANGE not applied in this place just because nonexistent range
    // mods for attacking totems

    // pointer to appropriate target if found any
    Unit* victim = m_creature->GetMap()->GetUnit(i_victimGuid);

    // Search victim if no, not attackable, or out of range, or friendly
    // (possible in case duel end)
    if (!victim || !victim->isTargetableForAttack() ||
        !m_creature->IsWithinDistInMap(victim, max_range) ||
        m_creature->IsFriendlyTo(victim) ||
        !victim->can_be_seen_by(m_creature, m_creature))
    {
        float range = max_range;
        victim = maps::visitors::yield_best_match<Unit, Player, Pet, Creature,
            SpecialVisCreature, TemporarySummon, Totem>{}(m_creature, max_range,
            [this, range](auto&& elem) mutable
            {
                if (elem->isTargetableForAttack() &&
                    m_creature->IsWithinDistInMap(elem, range) &&
                    m_creature->IsHostileTo(elem) &&
                    elem->can_be_seen_by(m_creature, m_creature, false))
                {
                    range = m_creature->GetDistance(elem);
                    return true;
                }

                return false;
            });
    }

    // If have target
    if (victim)
    {
        // remember
        i_victimGuid = victim->GetObjectGuid();

        // attack
        m_creature->CastSpell(victim, getTotem().GetSpell(), false);
    }
    else
        i_victimGuid.Clear();
}

bool TotemAI::IsVisible(Unit*) const
{
    return false;
}

void TotemAI::AttackStart(Unit*)
{
}

Totem& TotemAI::getTotem()
{
    return static_cast<Totem&>(*m_creature);
}
