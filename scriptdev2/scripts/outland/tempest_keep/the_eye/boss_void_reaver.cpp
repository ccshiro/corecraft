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
SDName: boss_void_reaver
SD%Complete: 100
SDComment:
SDCategory: Tempest Keep, The Eye
EndScriptData */

#include "precompiled.h"
#include "the_eye.h"

enum
{
    SAY_AGGRO = -1550000,
    SAY_POUND_1 = -1550005,
    SAY_POUND_2 = -1550006,
    SAY_KILL_1 = -1550001,
    SAY_KILL_2 = -1550002,
    SAY_KILL_3 = -1550003,
    SAY_DEATH = -1550004,

    SPELL_ARCANE_ORB = 34172,
    SPELL_ARCANE_ORB_TRIGGER = 34190,
    SPELL_POUNDING = 34162,
    SPELL_BERSERK = 26662,
    SPELL_KNOCK_AWAY = 25778, // Does not use the periodic aura
};

struct MANGOS_DLL_DECL boss_void_reaverAI : public ScriptedAI
{
    boss_void_reaverAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_instance = (ScriptedInstance*)pCreature->GetInstanceData();
        Reset();
    }

    ScriptedInstance* m_instance;
    uint32 m_orb;
    uint32 m_pounding;
    uint32 m_knockAway;
    uint32 m_berserk;

    void Reset() override
    {
        m_orb = 10000;
        m_pounding = 7000;
        m_knockAway = 17000;
        m_berserk = 10 * MINUTE * IN_MILLISECONDS;
    }

    void Aggro(Unit* /*pWho*/) override
    {
        if (m_instance)
            m_instance->SetData(TYPE_VOID_REAVER, IN_PROGRESS);

        DoScriptText(SAY_AGGRO, m_creature);
    }

    void JustReachedHome() override
    {
        if (m_instance)
            m_instance->SetData(TYPE_VOID_REAVER, FAIL);
    }

    void JustDied(Unit* /*pVictim*/) override
    {
        if (m_instance)
            m_instance->SetData(TYPE_VOID_REAVER, DONE);

        DoScriptText(SAY_DEATH, m_creature);
    }

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_KILL_1, SAY_KILL_2, SAY_KILL_3);
    }

    void SummonedMovementInform(Creature* summon, movement::gen gen, uint32)
    {
        if (gen == movement::gen::point)
        {
            summon->remove_auras(40931);
            m_creature->CastSpell(summon, SPELL_ARCANE_ORB_TRIGGER, true);
        }
    }

    void UpdateAI(const uint32 diff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (m_orb <= diff)
        {
            // Casts on non-melee targets, if there are none casts on melee
            // targets
            Unit* tar =
                m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0,
                    SPELL_ARCANE_ORB, SELECT_FLAG_NOT_IN_MELEE_RANGE);
            if (!tar)
                tar = m_creature->SelectAttackingTarget(
                    ATTACKING_TARGET_RANDOM, 0);
            if (tar)
            {
                // This is a giant hack to get around the fact that this spell
                // became an arcing orb spell, and in 2.0.3 it went straight to
                // the target.
                if (auto c = m_creature->SummonCreature(19577,
                        m_creature->GetX(), m_creature->GetY(),
                        m_creature->GetZ() + 18.0f, 0, TEMPSUMMON_TIMED_DESPAWN,
                        20000, SUMMON_OPT_NOT_COMBAT_SUMMON))
                {
                    c->SetLevitate(true);
                    c->SendHeartBeat();

                    c->movement_gens.remove_all(movement::gen::idle);
                    c->movement_gens.push(
                        new movement::PointMovementGenerator(0, tar->GetX(),
                            tar->GetY(), tar->GetZ() + 2.0f, false, true));

                    c->AddAuraThroughNewHolder(40931, c, 20000);
                }
                m_orb = urand(3000, 5000);
            }
        }
        else
            m_orb -= diff;

        if (m_pounding <= diff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_POUNDING) == CAST_OK)
            {
                DoScriptText(
                    urand(0, 1) ? SAY_POUND_1 : SAY_POUND_2, m_creature);
                m_pounding = 14000;
            }
        }
        else
            m_pounding -= diff;

        if (m_knockAway <= diff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_KNOCK_AWAY,
                    CAST_TRIGGERED) == CAST_OK)
                m_knockAway = 17000;
        }
        else
            m_knockAway -= diff;

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

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_void_reaver(Creature* pCreature)
{
    return new boss_void_reaverAI(pCreature);
}

void AddSC_boss_void_reaver()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_void_reaver";
    pNewScript->GetAI = &GetAI_boss_void_reaver;
    pNewScript->RegisterSelf();
}
