/* Copyright (C) 2006 - 2012 ScriptDev2 <http://www.scriptdev2.com/>
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

/* ScriptData
SDName: Romeo and Julianne
SD%Complete: 100
SDComment:
SDCategory: Karazhan
EndScriptData */

#include "karazhan.h"
#include "precompiled.h"

/**** Speech *****/
enum
{
    SAY_JULIANNE_AGGRO = -1532046,
    // SAY_JULIANNE_ENTER = -1532047,
    SAY_JULIANNE_POTION = -1532048,
    SAY_JULIANNE_DEATH = -1532049,
    SAY_JULIANNE_RESURRECT = -1532050,
    SAY_JULIANNE_KILL = -1532051,

    SAY_ROMULO_AGGRO = -1532052,
    SAY_ROMULO_DEATH_PHASE_2 = -1532053,
    SAY_ROMULO_DEATH = -1532054,
    SAY_ROMULO_RESURRECT = -1532055,
    SAY_ROMULO_KILL = -1532056,

    /***** Spells For Julianne *****/
    SPELL_BLINDING_PASSION = 30890,
    SPELL_DEVOTION = 30887,
    SPELL_ETERNAL_AFFECTION = 30878,
    SPELL_POWERFUL_ATTRACTION = 30889,
    SPELL_DRINK_POISON = 30907,

    /***** Spells For Romulo ****/
    SPELL_BACKWARD_LUNGE = 30815,
    SPELL_DARING = 30841,
    SPELL_DEADLY_SWATHE = 30817,
    SPELL_POISON_THRUST = 30822,

    /**** Other Misc. Spells ****/
    SPELL_UNDYING_LOVE = 30951,
    SPELL_RES_VISUAL = 21074,

    // Not part of the fight, but proves useful
    SPELL_STUN_SELF = 48342,
};

// The fight goes like this:
// Julianne enters.
// Players kill her. Romulo enters.
// Players kill him. They both lie dead for a bit.
// They both resurrect. You fight them both.
// If one dies much before the other, he resurrects the other.
enum RAJPhase
{
    PHASE_JULIANNE = 0,
    PHASE_ROMULO = 1,
    PHASE_BOTH = 2,
};

void FakeDeath(Creature* c, bool apply)
{
    if (apply)
    {
        c->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
        c->InterruptNonMeleeSpells(false);
        c->remove_auras();
        c->SetHealth(0);
        c->AI()->Pacify(true);
        c->movement_gens.push(new movement::StoppedMovementGenerator,
            movement::EVENT_LEAVE_COMBAT);
    }
    else
    {
        c->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
        c->SetHealth(c->GetMaxHealth());
        c->AI()->Pacify(false);
        c->movement_gens.remove_all(movement::gen::stopped);
    }
}

struct MANGOS_DLL_DECL boss_julianneAI : public Scripted_BehavioralAI
{
    boss_julianneAI(Creature* pCreature) : Scripted_BehavioralAI(pCreature)
    {
        m_instance = pCreature->GetMapId() == KARAZHAN_MAP_ID ?
                         (instance_karazhan*)pCreature->GetInstanceData() :
                         NULL;
        m_aggroYellTimer = 3000;
        Reset();
    }

    instance_karazhan* m_instance;
    bool m_dead;
    uint32 m_aggroYellTimer;
    uint32 m_phase;
    uint32 m_romuloTimer;
    uint32 m_resurrectTimer;

    void Reset() override
    {
        Scripted_BehavioralAI::Reset();
        m_phase = PHASE_JULIANNE;
        m_dead = false;
        m_romuloTimer = 0;
        m_resurrectTimer = 0;
    }

    void EnterEvadeMode(bool by_group = false) override
    {
        if (m_dead)
            m_creature->ForcedDespawn();
        else
            Scripted_BehavioralAI::EnterEvadeMode(by_group);
    }

    void JustReachedHome() override
    {
        if (m_instance &&
            m_instance->GetData(TYPE_OPERA) !=
                FAIL) // May already be set to fail by Romulo
            m_instance->SetData(TYPE_OPERA, FAIL);
        m_creature->ForcedDespawn();
    }

    void JustDied(Unit* /*killer*/) override
    {
        if (m_instance &&
            m_instance->GetData(TYPE_OPERA) !=
                DONE) // May already be set to done by Romulo
            m_instance->SetData(TYPE_OPERA, DONE);
    }

    void SpellHit(Unit* /*caster*/, const SpellEntry* spell) override
    {
        if (spell->Id == SPELL_DRINK_POISON)
        {
            FakeDeath(m_creature, true);
            m_romuloTimer = 5000;
        }
        else if (spell->Id == SPELL_UNDYING_LOVE)
            FakeDeath(m_creature, false);
    }

    void DamageTaken(Unit* /*doneBy*/, uint32& damage) override
    {
        if (!m_instance)
            return;

        if (m_creature->GetHealth() <= damage)
        {
            // We die if Romulo is dead as well in phase 3
            if (m_phase == PHASE_BOTH)
                if (Creature* romulo =
                        m_instance->GetSingleCreatureFromStorage(NPC_ROMULO))
                    if (!romulo->GetHealth())
                    {
                        romulo->SetHealth(1);
                        romulo->SetStandState(UNIT_STAND_STATE_DEAD);
                        romulo->Kill(romulo);
                        return;
                    }

            // Fake death
            damage = m_creature->GetHealth() - 1;
            if (!m_dead)
            {
                if (m_phase == PHASE_JULIANNE)
                {
                    if (DoCastSpellIfCan(m_creature, SPELL_DRINK_POISON) !=
                        CAST_OK)
                        return;
                    DoScriptText(SAY_JULIANNE_POTION, m_creature);
                    m_phase = PHASE_ROMULO;
                }
                else
                {
                    FakeDeath(m_creature, true);
                    m_resurrectTimer = 10000;
                }
                m_dead = true;
                if (m_phase == PHASE_BOTH)
                    DoScriptText(SAY_JULIANNE_DEATH, m_creature);
            }
        }
    }

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_JULIANNE_KILL);
    }

    void UpdateAI(const uint32 diff) override
    {
        if (m_aggroYellTimer)
        {
            if (m_aggroYellTimer <= diff)
            {
                DoScriptText(SAY_JULIANNE_AGGRO, m_creature);
                m_creature->RemoveFlag(UNIT_FIELD_FLAGS,
                    UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE);
                m_creature->SetInCombatWithZone();
                m_aggroYellTimer = 0;
            }
            else
                m_aggroYellTimer -= diff;
        }

        if (m_resurrectTimer && m_instance)
        {
            if (m_resurrectTimer <= diff)
            {
                if (Creature* romulo =
                        m_instance->GetSingleCreatureFromStorage(NPC_ROMULO))
                {
                    if (CreatureAI* AI =
                            dynamic_cast<CreatureAI*>(romulo->AI()))
                        if (AI->DoCastSpellIfCan(
                                m_creature, SPELL_UNDYING_LOVE) != CAST_OK)
                            return;
                    m_resurrectTimer = 0;
                    DoScriptText(SAY_ROMULO_RESURRECT, romulo);
                    m_dead = false;
                }
            }
            else
                m_resurrectTimer -= diff;
        }

        // Summon Romulo
        if (m_romuloTimer)
        {
            if (m_romuloTimer <= diff)
            {
                m_creature->SummonCreature(NPC_ROMULO, -10893.0f, -1758.4f,
                    90.5f, 4.738f, TEMPSUMMON_CORPSE_TIMED_DESPAWN,
                    sWorld::Instance()->getConfig(
                        CONFIG_UINT32_CORPSE_DECAY_WORLDBOSS) *
                        1000);
                m_romuloTimer = 0;
            }
            else
                m_romuloTimer -= diff;
        }

        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim() ||
            m_dead || m_phase == PHASE_ROMULO)
            return;

        Scripted_BehavioralAI::UpdateInCombatAI(diff);
        DoMeleeAttackIfReady();
    }
};

struct MANGOS_DLL_DECL boss_romuloAI : public Scripted_BehavioralAI
{
    boss_romuloAI(Creature* pCreature) : Scripted_BehavioralAI(pCreature)
    {
        m_instance = pCreature->GetMapId() == KARAZHAN_MAP_ID ?
                         (instance_karazhan*)pCreature->GetInstanceData() :
                         NULL;
        Reset();
    }

    instance_karazhan* m_instance;
    bool m_dead;
    uint32 m_phase;
    uint32 m_resurrectTimer;

    void Reset() override
    {
        m_phase = PHASE_ROMULO;
        m_dead = false;
        m_resurrectTimer = 0;
    }

    void JustSummoned(Creature*) override { m_creature->SetInCombatWithZone(); }

    void EnterEvadeMode(bool by_group) override
    {
        if (m_dead)
            m_creature->ForcedDespawn();
        else
            Scripted_BehavioralAI::EnterEvadeMode(by_group);
    }

    void JustReachedHome() override
    {
        if (m_instance &&
            m_instance->GetData(TYPE_OPERA) !=
                FAIL) // May already be set to fail by Julianne
            m_instance->SetData(TYPE_OPERA, FAIL);
        m_creature->ForcedDespawn();
    }

    void JustDied(Unit* /*killer*/) override
    {
        if (m_instance &&
            m_instance->GetData(TYPE_OPERA) !=
                DONE) // May already be set to done by Romulo
            m_instance->SetData(TYPE_OPERA, DONE);
    }

    void SpellHit(Unit* /*caster*/, const SpellEntry* spell) override
    {
        if (spell->Id == SPELL_UNDYING_LOVE)
        {
            FakeDeath(m_creature, false);
            if (Creature* julianne =
                    m_instance->GetSingleCreatureFromStorage(NPC_JULIANNE))
            {
                if (boss_julianneAI* AI =
                        dynamic_cast<boss_julianneAI*>(julianne->AI()))
                    AI->m_phase = PHASE_BOTH;
            }
        }
    }

    void DamageTaken(Unit* /*done_by*/, uint32& damage) override
    {
        if (!m_instance)
            return;

        if (m_creature->GetHealth() <= damage)
        {
            // We die if Julianne is dead as well in phase 3
            if (m_phase == PHASE_BOTH)
            {
                if (Creature* julianne =
                        m_instance->GetSingleCreatureFromStorage(NPC_JULIANNE))
                    if (!julianne->GetHealth())
                    {
                        julianne->SetHealth(1);
                        julianne->SetStandState(UNIT_STAND_STATE_DEAD);
                        julianne->Kill(julianne);
                        return;
                    }
            }
            // Fake death
            damage = m_creature->GetHealth() - 1;
            if (!m_dead)
            {
                if (m_phase == PHASE_ROMULO)
                {
                    DoScriptText(SAY_ROMULO_DEATH_PHASE_2, m_creature);
                    m_resurrectTimer = 6000;
                }
                else
                    m_resurrectTimer = 10000;
                FakeDeath(m_creature, true);
                m_dead = true;
                if (m_phase == PHASE_BOTH)
                    DoScriptText(SAY_ROMULO_DEATH, m_creature);
            }
        }
    }

    void Aggro(Unit* /*who*/) override
    {
        DoScriptText(SAY_ROMULO_AGGRO, m_creature);
    }

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_ROMULO_KILL);
    }

    void UpdateAI(const uint32 diff) override
    {
        if (m_resurrectTimer && m_instance)
        {
            if (m_resurrectTimer <= diff)
            {
                if (m_phase == PHASE_ROMULO)
                {
                    // Resurrect Julianne, leave phase and fake death of Romulo;
                    // she resurrects him to bring the fight into phase 3
                    if (Creature* julianne =
                            m_instance->GetSingleCreatureFromStorage(
                                NPC_JULIANNE))
                    {
                        if (boss_julianneAI* AI =
                                dynamic_cast<boss_julianneAI*>(julianne->AI()))
                        {
                            AI->m_dead = false;
                            AI->m_phase = PHASE_ROMULO;
                            FakeDeath(julianne, false);
                        }
                        m_phase = PHASE_BOTH;
                        m_resurrectTimer = 1;
                        return;
                    }
                }
                else
                {
                    if (Creature* julianne =
                            m_instance->GetSingleCreatureFromStorage(
                                NPC_JULIANNE))
                    {
                        if (boss_julianneAI* AI =
                                dynamic_cast<boss_julianneAI*>(julianne->AI()))
                        {
                            if (AI->DoCastSpellIfCan(
                                    m_creature, SPELL_UNDYING_LOVE) != CAST_OK)
                                return; // Try again
                        }
                        DoScriptText(SAY_JULIANNE_RESURRECT, julianne);
                        m_dead = false;
                    }
                }

                m_resurrectTimer = 0;
            }
            else
                m_resurrectTimer -= diff;
        }

        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim() ||
            m_dead)
            return;

        Scripted_BehavioralAI::UpdateInCombatAI(diff);

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_julianne(Creature* pCreature)
{
    return new boss_julianneAI(pCreature);
}

CreatureAI* GetAI_boss_romulo(Creature* pCreature)
{
    return new boss_romuloAI(pCreature);
}

void Opera_RomeoAndJulianne()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->GetAI = &GetAI_boss_julianne;
    pNewScript->Name = "boss_julianne";
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->GetAI = &GetAI_boss_romulo;
    pNewScript->Name = "boss_romulo";
    pNewScript->RegisterSelf();
}
