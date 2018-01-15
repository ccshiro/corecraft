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

#include "GuardAI.h"
#include "Creature.h"
#include "Player.h"
#include "World.h"
#include "movement/HomeMovementGenerator.h"
#include "movement/TargetedMovementGenerator.h"

GuardAI::GuardAI(Creature* c)
  : CreatureAI(c), i_state(STATE_NORMAL), i_tracker(TIME_INTERVAL_LOOK)
{
}

void GuardAI::MoveInLineOfSight(Unit* u)
{
    // Ignore Z for flying creatures
    if (!m_creature->CanFly() &&
        m_creature->GetDistanceZ(u) > CREATURE_Z_ATTACK_RANGE)
        return;

    if (!m_creature->getVictim() && u->isTargetableForAttack() &&
        (u->IsHostileToPlayers() ||
            m_creature->IsHostileTo(
                u) /*|| u->getVictim() && m_creature->IsFriendlyTo(u->getVictim())*/) &&
        u->isInAccessablePlaceFor(m_creature))
    {
        if (m_creature->IsWithinAggroDistance(u))
        {
            // Need add code to let guard support player
            AttackStart(u);
        }
    }
}

void GuardAI::EnterEvadeMode(bool by_group)
{
    // process creature evade actions
    m_creature->OnEvadeActions(by_group);

    if (!m_creature->isAlive())
    {
        i_state = STATE_NORMAL;

        i_victimGuid.Clear();
        m_creature->CombatStop(true);
        m_creature->DeleteThreatList();
        return;
    }

    Unit* victim = m_creature->GetMap()->GetUnit(i_victimGuid);

    if (!victim)
    {
        LOG_DEBUG(logging, "Creature stopped attacking, no victim [guid=%u]",
            m_creature->GetGUIDLow());
    }
    else if (!victim->isAlive())
    {
        LOG_DEBUG(logging,
            "Creature stopped attacking, victim is dead [guid=%u]",
            m_creature->GetGUIDLow());
    }
    else if (victim->HasStealthAura())
    {
        LOG_DEBUG(logging,
            "Creature stopped attacking, victim is in stealth [guid=%u]",
            m_creature->GetGUIDLow());
    }
    else if (victim->IsTaxiFlying())
    {
        LOG_DEBUG(logging,
            "Creature stopped attacking, victim is in flight [guid=%u]",
            m_creature->GetGUIDLow());
    }
    else
    {
        LOG_DEBUG(logging,
            "Creature stopped attacking, victim out run him [guid=%u]",
            m_creature->GetGUIDLow());
    }

    m_creature->remove_auras_on_evade();
    m_creature->DeleteThreatList();
    i_victimGuid.Clear();
    m_creature->CombatStop(true);
    i_state = STATE_NORMAL;

    m_creature->movement_gens.on_event(movement::EVENT_LEAVE_COMBAT);
}

void GuardAI::UpdateAI(const uint32 /*diff*/)
{
    // update i_victimGuid if i_creature.getVictim() !=0 and changed
    if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
        return;

    i_victimGuid = m_creature->getVictim()->GetObjectGuid();

    DoMeleeAttackIfReady();
}

bool GuardAI::IsVisible(Unit* pl) const
{
    return m_creature->IsWithinDist(
               pl, sWorld::Instance()->getConfig(CONFIG_FLOAT_SIGHT_GUARDER)) &&
           pl->can_be_seen_by(m_creature, m_creature);
}

void GuardAI::AttackStart(Unit* u)
{
    if (!u)
        return;

    if (m_creature->Attack(u, true))
    {
        i_victimGuid = u->GetObjectGuid();
        m_creature->AddThreat(u);
        m_creature->SetInCombatWith(u);
        u->SetInCombatWith(m_creature);

        m_creature->movement_gens.on_event(movement::EVENT_ENTER_COMBAT);

        if (!m_creature->movement_gens.has(movement::gen::chase))
            m_creature->movement_gens.push(
                new movement::ChaseMovementGenerator(),
                movement::EVENT_LEAVE_COMBAT);

        if (!m_creature->movement_gens.has(movement::gen::home))
            m_creature->movement_gens.push(
                new movement::HomeMovementGenerator());
    }
}

void GuardAI::JustDied(Unit* killer)
{
    if (Player* pkiller = killer->GetCharmerOrOwnerPlayerOrPlayerItself())
        m_creature->SendZoneUnderAttackMessage(pkiller);
}
