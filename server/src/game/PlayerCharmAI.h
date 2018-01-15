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

#ifndef MANGOS_PLAYERCHARMAI_H
#define MANGOS_PLAYERCHARMAI_H

#include "CreatureAI.h"
#include "ObjectGuid.h"
#include "Timer.h"
#include <vector>

class Creature;
class PCAIClassBehavior;
class Player;
class Unit;

#define PCAI_DIST_NEW_VICTIM_BREAKPOINT \
    1.5f // oldTargetDist * thisValue > newTargetDist == switch target
#define PCAI_HOSTILE_SEARCH_RANGE 100.0f

class PlayerCharmAI : public CreatureAI
{
public:
    // NOTE: CreatureAI does nothing with the pointer except saving it in
    // Creature* m_creature
    // so despite this reinterpret being completely idiotic at first look, it
    // has no harming effect
    explicit PlayerCharmAI(Player* plr);
    ~PlayerCharmAI();

    void Reset() override;

    // Emulation of the SelectHostileTarget() accessible to Creatures
    bool SelectHostileTarget();
    // Casts most appropriate spell for combat. Returns true if GCD was
    // consumed.
    bool CastAnySpellIfCan();

    void AttackStart(Unit* who) override;
    void EnterEvadeMode(bool by_group = false) override;

    void UpdateAI(const uint32 diff) override;

private:
    Player* m_player;
    ObjectGuid m_victim; // Current victim
    PCAIClassBehavior* m_classBehavior;
    uint32 m_gcd;
    uint32 m_meleeCastSpell;
    ObjectGuid m_meleeTarget;
    uint32 m_forceTargetSwapTimer;

    uint32 SpellCastable(uint32 spellId, Unit* target);
    uint32 GetHighestSpellRank(uint32 spellId);
};

#endif
