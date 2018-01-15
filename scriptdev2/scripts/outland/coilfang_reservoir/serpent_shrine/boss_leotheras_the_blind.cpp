/* Copyright (C) 2013 Corecraft <https://www.worldofcorecraft.com/>
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
SDName: Boss_Leotheras_The_Blind
SD%Complete: 100
SDComment:
SDCategory: Coilfang Resevoir, Serpentshrine Cavern
EndScriptData */

#include "precompiled.h"
#include "serpentshrine_cavern.h"

enum
{
    SAY_DEMON_FORM = -1548010,
    SAY_INSIDIOUS_WHISPER = -1548011,
    SAY_DEMON_KILL_1 = -1548012,
    SAY_DEMON_KILL_2 = -1548013,
    SAY_DEMON_KILL_3 = -1548014,
    SAY_NORMAL_KILL_1 = -1548015,
    SAY_NORMAL_KILL_2 = -1548016,
    SAY_NORMAL_KILL_3 = -1548017,
    SAY_BREAK_FREE = -1548018,
    SAY_SHADOW_FORM = -1548019,
    SAY_DEATH = -1548020,

    SPELL_METAMORPHOSIS =
        37673, // We dont use this as mangos doesnt use UpdateEntry for it
    SPELL_MORPHOSIS_STUN = 37768,

    NPC_LEOTHERAS_DEMON = 21845, // Demon phase
    NPC_INNER_DEMON = 21857,

    NORMAL_PHASE = 1,
    DEMON_PHASE = 2,

    DEMON_FACTION_OFFSET = 1829,
    PLAYER_FACTION_OFFSET = 1018,

    SPELL_WHIRLWIND = 37640,
    SPELL_CHAOS_BLAST = 37674,
    SPELL_INSIDIOUS_WHISPER = 37676,
    SPELL_CLEAR_INSIDIOUS = 37922,   // Not used. We use core functions instead.
    SPELL_INNER_SHADOW_BOLT = 39309, // Used by inner demon
    SPELL_DEMON_LINK = 37716,
    SPELL_CONSUMING_MADNESS = 37749,
    SPELL_CLEAR_CONSUMING_MADNESS = 37750,
    SPELL_BERSERK = 27680,
};

struct MANGOS_DLL_DECL boss_leotheras_the_blindAI : public Scripted_BehavioralAI
{
    boss_leotheras_the_blindAI(Creature* pCreature)
      : Scripted_BehavioralAI(pCreature)
    {
        m_instance = (ScriptedInstance*)pCreature->GetInstanceData();
        Reset();

        m_creature->movement_gens.push(new movement::StoppedMovementGenerator(),
            movement::EVENT_LEAVE_COMBAT);
    }

    ScriptedInstance* m_instance;
    uint32 m_phase;
    uint32 m_phaseSwitch;
    uint32 m_whirlwind;
    uint32 m_whirlwindFade;
    uint32 m_targetChange;
    uint32 m_chaos;
    uint32 m_whisper;
    uint32 m_summons;
    bool m_brokenFree;
    uint32 m_freeRp;
    uint32 m_berserk;

    void Reset() override
    {
        m_phase = NORMAL_PHASE;
        m_phaseSwitch = 75000;
        m_whirlwind = urand(15000, 17000);
        m_whirlwindFade = 0;
        m_targetChange = 0;
        m_chaos = 0;
        m_whisper = 0;
        m_summons = 0;
        m_brokenFree = false;
        m_freeRp = 0;
        m_berserk = 10 * MINUTE * IN_MILLISECONDS;
        m_creature->UpdateEntry(NPC_LEOTHERAS);

        SetBehavioralPhase(0);
        Scripted_BehavioralAI::Reset();

        m_creature->SetFocusTarget(nullptr);
        Pacify(true);
    }

    void Aggro(Unit* /*pWho*/) override
    {
        if (m_instance)
            m_instance->SetData(TYPE_LEOTHERAS, IN_PROGRESS);
    }

    void JustReachedHome() override
    {
        // Set passive before clear consuming madness to not re-enter combat
        m_creature->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PASSIVE);

        m_creature->CastSpell(m_creature, SPELL_CLEAR_CONSUMING_MADNESS, true);

        if (m_instance)
        {
            m_instance->SetData(TYPE_LEOTHERAS, FAIL);
            if (Creature* shadow = m_instance->GetSingleCreatureFromStorage(
                    NPC_SHADOW_LEOTHERAS))
                shadow->ForcedDespawn();
        }

        if (!m_creature->movement_gens.has(movement::gen::stopped))
            m_creature->movement_gens.push(
                new movement::StoppedMovementGenerator(),
                movement::EVENT_LEAVE_COMBAT);
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        DoScriptText(SAY_DEATH, m_creature);
        if (m_instance)
        {
            m_instance->SetData(TYPE_LEOTHERAS, DONE);
            if (Creature* shadow = m_instance->GetSingleCreatureFromStorage(
                    NPC_SHADOW_LEOTHERAS))
                shadow->ForcedDespawn();
        }
    }

    void KilledUnit(Unit* victim) override
    {
        if (m_phase == NORMAL_PHASE)
            DoKillSay(m_creature, victim, SAY_NORMAL_KILL_1, SAY_NORMAL_KILL_2,
                SAY_NORMAL_KILL_3);
        else
            DoKillSay(m_creature, victim, SAY_DEMON_KILL_1, SAY_DEMON_KILL_2,
                SAY_DEMON_KILL_3);
    }

    void JustSummoned(Creature* c) override
    {
        if (c->GetEntry() == NPC_INNER_DEMON)
            c->setFaction(DEMON_FACTION_OFFSET + m_summons++);
        else if (c->GetEntry() == NPC_LEOTHERAS_DEMON)
            DoScriptText(SAY_SHADOW_FORM, c);
    }

    void UpdateAI(const uint32 diff) override
    {
        if (m_freeRp)
        {
            if (m_freeRp <= diff)
            {
                m_creature->movement_gens.remove_all(movement::gen::stopped);
                Pacify(false);

                m_creature->SetStandState(UNIT_STAND_STATE_STAND);
                m_creature->RemoveFlag(UNIT_FIELD_FLAGS,
                    UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE);
                m_creature->SummonCreature(NPC_SHADOW_LEOTHERAS,
                    m_creature->GetX(), m_creature->GetY(), m_creature->GetZ(),
                    m_creature->GetO(), TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,
                    15000);
                m_freeRp = 0;
            }
            else
                m_freeRp -= diff;
            return;
        }

        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim() ||
            IsPacified())
            return;

        if (m_berserk)
        {
            if (m_berserk <= diff)
            {
                if (DoCastSpellIfCan(m_creature, SPELL_BERSERK) == CAST_OK)
                    m_berserk = 0;
            }
            else
                m_berserk -= diff;
        }

        // Break free logic
        if (!m_brokenFree && m_creature->GetHealthPercent() < 16.0f)
        {
            m_creature->remove_auras(SPELL_WHIRLWIND);
            m_creature->movement_gens.push(
                new movement::StoppedMovementGenerator(),
                movement::EVENT_LEAVE_COMBAT);
            Pacify(true);

            DoScriptText(SAY_BREAK_FREE, m_creature);
            m_creature->SetFlag(UNIT_FIELD_FLAGS,
                UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE);
            m_creature->SetStandState(UNIT_STAND_STATE_KNEEL);

            m_phase = NORMAL_PHASE;
            m_brokenFree = true;
            m_whirlwind = urand(1000, 5000);
            m_whirlwindFade = 0;
            m_freeRp = 8000;
            SetBehavioralPhase(0);
            return;
        }

        if (m_phase == NORMAL_PHASE) // Normal form
        {
            if (m_whirlwindFade)
            {
                if (m_whirlwindFade <= diff)
                {
                    DoResetThreat();
                    m_creature->SetFocusTarget(nullptr);
                    m_whirlwindFade = 0;
                    return;
                }
                else
                    m_whirlwindFade -= diff;

                if (m_targetChange <= diff)
                {
                    if (Unit* tar = m_creature->SelectAttackingTarget(
                            ATTACKING_TARGET_RANDOM, 0))
                    {
                        m_creature->SetFocusTarget(tar);
                        m_targetChange = 1000;
                    }
                }
                else
                    m_targetChange -= diff;
            }

            if (!m_brokenFree)
            {
                if (m_phaseSwitch <= diff)
                {
                    if (!m_whirlwindFade &&
                        DoCastSpellIfCan(m_creature, SPELL_MORPHOSIS_STUN) ==
                            CAST_OK)
                    {
                        DoResetThreat();
                        DoScriptText(SAY_DEMON_FORM, m_creature);
                        m_creature->UpdateEntry(NPC_LEOTHERAS_DEMON);
                        m_phaseSwitch = 60000;
                        m_phase = DEMON_PHASE;
                        m_chaos = 100;
                        m_whisper = 15000;
                        SetBehavioralPhase(1);
                    }
                }
                else
                    m_phaseSwitch -= diff;
            }

            if (m_whirlwind <= diff)
            {
                if (DoCastSpellIfCan(m_creature, SPELL_WHIRLWIND) == CAST_OK)
                {
                    m_whirlwind = urand(30000, 35000);
                    m_whirlwindFade = 12000;
                    m_targetChange = 1000;
                }
            }
            else
                m_whirlwind -= diff;
        }
        else // Demon Form
        {
            if (!m_brokenFree)
            {
                if (m_phaseSwitch <= diff)
                {
                    if (DoCastSpellIfCan(m_creature, SPELL_MORPHOSIS_STUN) ==
                        CAST_OK)
                    {
                        DoResetThreat();
                        m_creature->UpdateEntry(NPC_LEOTHERAS);
                        m_phaseSwitch = 60000;
                        m_whirlwind = urand(1000, 5000);
                        m_phase = NORMAL_PHASE;
                        SetBehavioralPhase(0);
                    }
                }
                else
                    m_phaseSwitch -= diff;
            }

            if (m_whisper)
            {
                if (m_whisper <= diff)
                {
                    if (DoCastSpellIfCan(m_creature, SPELL_INSIDIOUS_WHISPER) ==
                        CAST_OK)
                    {
                        DoScriptText(SAY_INSIDIOUS_WHISPER, m_creature);
                        m_whisper = 0;
                        m_summons = 0;
                    }
                }
                else
                    m_whisper -= diff;
            }
        }

        Scripted_BehavioralAI::UpdateInCombatAI(diff);

        if (!m_whirlwindFade)
            DoMeleeAttackIfReady();
    }
};

/*
 * Inner Demon
 */
struct MANGOS_DLL_DECL npc_inner_demonAI : public ScriptedAI
{
    npc_inner_demonAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_instance = (ScriptedInstance*)pCreature->GetInstanceData();
        Reset();
    }

    ScriptedInstance* m_instance;
    ObjectGuid m_target;
    uint32 m_bolt;
    uint32 m_timeout;

    void Reset() override
    {
        m_bolt = urand(10000, 13000);
        m_timeout = 30000;
    }

    void AttackStart(Unit* pWho) override
    {
        if (m_target || pWho->GetTypeId() != TYPEID_PLAYER || !m_instance)
            return;
        DoCastSpellIfCan(m_creature, SPELL_DEMON_LINK);
        pWho->setFaction(PLAYER_FACTION_OFFSET +
                         (m_creature->getFaction() - DEMON_FACTION_OFFSET));
        m_target = pWho->GetObjectGuid();
        ScriptedAI::AttackStart(pWho);
    }

    void ResetFaction()
    {
        if (auto t = m_creature->GetMap()->GetPlayer(m_target))
            t->setFaction(t->getFactionForRace(t->getRace()));
    }

    void JustReachedHome() override
    {
        ResetFaction();
        m_creature->ForcedDespawn();
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        ResetFaction();
        if (Unit* t = m_creature->GetMap()->GetUnit(m_target))
            t->remove_auras(SPELL_INSIDIOUS_WHISPER);
    }

    void KilledUnit(Unit* pWho) override
    {
        if (pWho->GetObjectGuid() == m_target)
            ResetFaction();
    }

    void UpdateAI(const uint32 diff) override
    {
        if (m_timeout <= diff)
        {
            if (Unit* t = m_creature->GetMap()->GetUnit(m_target))
            {
                ResetFaction();
                if (Creature* leo =
                        m_instance->GetSingleCreatureFromStorage(NPC_LEOTHERAS))
                {
                    leo->CastSpell(t, SPELL_CONSUMING_MADNESS, true);
                    m_creature->ForcedDespawn();
                }
            }
        }
        else
            m_timeout -= diff;

        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (m_bolt <= diff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(),
                    SPELL_INNER_SHADOW_BOLT) == CAST_OK)
                m_bolt = 11000;
        }
        else
            m_bolt -= diff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_leotheras_the_blind(Creature* pCreature)
{
    return new boss_leotheras_the_blindAI(pCreature);
}

CreatureAI* GetAI_npc_inner_demon(Creature* pCreature)
{
    return new npc_inner_demonAI(pCreature);
}

void AddSC_boss_leotheras_the_blind()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_leotheras_the_blind";
    pNewScript->GetAI = &GetAI_boss_leotheras_the_blind;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_inner_demon";
    pNewScript->GetAI = &GetAI_npc_inner_demon;
    pNewScript->RegisterSelf();
}
