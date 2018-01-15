#include "critter_ai.h"
#include "Creature.h"
#include "movement/FleeingMovementGenerator.h"
#include "movement/HomeMovementGenerator.h"
#include "movement/TargetedMovementGenerator.h"

void critter_ai::AttackStart(Unit* attacker)
{
    if (begun_fleeing_)
        return;

    if (m_creature->Attack(attacker, false))
    {
        m_creature->AddThreat(attacker);
        m_creature->SetInCombatWith(attacker);
        attacker->SetInCombatWith(m_creature);

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

void critter_ai::AttackedBy(Unit* attacker)
{
    if (m_creature->Attack(attacker, false))
    {
        m_creature->movement_gens.on_event(movement::EVENT_ENTER_COMBAT);

        if (!m_creature->movement_gens.has(movement::gen::home))
            m_creature->movement_gens.push(
                new movement::HomeMovementGenerator());

        m_creature->RunAwayInFear(false);
        begun_fleeing_ = true;

        m_creature->AddThreat(attacker);
        m_creature->SetInCombatWith(attacker);
        attacker->SetInCombatWith(m_creature);
    }
}

void critter_ai::UpdateAI(const uint32)
{
    if (!m_creature->SelectHostileTarget())
        return;

    if (m_creature->movement_gens.has(movement::gen::chase))
    {
        if (!m_creature->getVictim())
            return;
        DoMeleeAttackIfReady();
    }
    else if (!m_creature->movement_gens.has(movement::gen::run_in_fear))
    {
        EnterEvadeMode();
    }
}

void critter_ai::EnterEvadeMode(bool)
{
    begun_fleeing_ = false;

    m_creature->OnEvadeActions(false);

    if (!m_creature->isAlive())
        return;

    m_creature->remove_auras_on_evade();
    m_creature->DeleteThreatList();
    m_creature->CombatStop(true);
    m_creature->ResetLootRecipients();

    m_creature->movement_gens.on_event(movement::EVENT_LEAVE_COMBAT);
}
