/* Copyright (C) 2013-2015 Corecraft <https://www.worldofcorecraft.com>
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
SDName: Boss_Doomwalker
SD%Complete: 100
SDComment:
SDCategory: Shadowmoon Valley
EndScriptData */

#include "precompiled.h"

enum
{
    SAY_AGGRO = -1000159,
    SAY_EARTHQUAKE_1 = -1000160,
    SAY_EARTHQUAKE_2 = -1000161,
    SAY_OVERRUN_1 = -1000162,
    SAY_OVERRUN_2 = -1000163,
    SAY_KILL_1 = -1000164,
    SAY_KILL_2 = -1000165,
    SAY_KILL_3 = -1000166,
    SAY_DEATH = -1000167,

    SPELL_EARTHQUAKE = 32686,
    SPELL_CRUSH_ARMOR = 33661,
    SPELL_LIGHTNING_WRATH = 33665,
    SPELL_OVERRUN = 32636,
    SPELL_OVERRUN_KNOCK = 32637,
    SPELL_ENRAGE = 33653,
    SPELL_MARK_OF_DEATH_PLAYER = 37128,
    SPELL_MARK_OF_DEATH_AURA =
        37125, // triggers 37131 on target if it has aura 37128
    SPELL_MOD_AURA_DUMMY = 150018
};

struct MANGOS_DLL_DECL boss_doomwalkerAI : public ScriptedAI
{
    boss_doomwalkerAI(Creature* pCreature) : ScriptedAI(pCreature) { Reset(); }

    bool m_enrage;
    uint32 m_earthquake;
    uint32 m_wrath;
    uint32 m_overrun;
    uint32 m_knockback;
    uint32 m_crush;
    uint32 m_dummy;
    ObjectGuid m_target;
    time_t last_aggro_say = 0;

    void Reset() override
    {
        m_enrage = false;
        m_earthquake = 30000;
        m_wrath = urand(10000, 15000);
        m_overrun = 20000;
        m_knockback = 0;
        m_crush = urand(5000, 15000);
        m_dummy = 0;
        m_target = ObjectGuid();

        m_creature->SetAggroDistance(60.0f);
    }

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_KILL_1, SAY_KILL_2, SAY_KILL_3);
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        DoScriptText(SAY_DEATH, m_creature);
    }

    void Aggro(Unit* /*pWho*/) override
    {
        DoCastSpellIfCan(m_creature, SPELL_MARK_OF_DEATH_AURA, CAST_TRIGGERED);
        if (last_aggro_say + 8 < WorldTimer::time_no_syscall())
        {
            last_aggro_say = WorldTimer::time_no_syscall();
            DoScriptText(SAY_AGGRO, m_creature);
        }
    }

    void MovementInform(movement::gen type, uint32 id) override
    {
        if (type == movement::gen::point && id == 100)
        {
            DoResetThreat();
            Unit* tar = m_creature->GetMap()->GetUnit(m_target);
            if (tar && tar->isAlive())
            {
                m_creature->AddThreat(tar, 10.0f);
                AttackStart(tar);
            }
            m_creature->remove_auras(SPELL_OVERRUN);
            m_knockback = 0;
        }
    }

    void UpdateAI(const uint32 diff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (m_creature->movement_gens.has(movement::gen::point))
        {
            if (m_knockback <= diff)
            {
                if (DoCastSpellIfCan(m_creature, SPELL_OVERRUN_KNOCK) ==
                    CAST_OK)
                    m_knockback = 250;
            }
            else
                m_knockback -= diff;
            return; // Dont continue logic while overrunning
        }

        if (m_dummy <= diff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_MOD_AURA_DUMMY,
                    CAST_TRIGGERED) == CAST_OK)
                m_dummy = 2000;
        }
        else
            m_dummy -= diff;

        if (!m_enrage && m_creature->GetHealthPercent() <= 20.0f)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_ENRAGE) == CAST_OK)
                m_enrage = true;
        }

        if (m_overrun <= diff)
        {
            if (Unit* target =
                    m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM,
                        1, (uint32)0, SELECT_FLAG_NOT_IN_MELEE_RANGE))
                if (DoCastSpellIfCan(m_creature, SPELL_OVERRUN) == CAST_OK)
                {
                    DoScriptText(urand(0, 1) ? SAY_OVERRUN_1 : SAY_OVERRUN_2,
                        m_creature);
                    float x, y, z;
                    target->GetPosition(x, y, z);
                    m_target = target->GetObjectGuid();
                    m_creature->movement_gens.push(
                        new movement::PointMovementGenerator(
                            100, x, y, z, true, true),
                        movement::EVENT_LEAVE_COMBAT);
                    m_knockback = 250;
                    m_overrun = urand(40000, 45000);
                    return;
                }
        }
        else
            m_overrun -= diff;

        if (m_earthquake <= diff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_EARTHQUAKE) == CAST_OK)
            {
                DoScriptText(urand(0, 1) ? SAY_EARTHQUAKE_1 : SAY_EARTHQUAKE_2,
                    m_creature);
                m_earthquake = urand(40000, 60000);
            }
        }
        else
            m_earthquake -= diff;

        if (m_wrath <= diff)
        {
            if (Unit* target = m_creature->SelectAttackingTarget(
                    ATTACKING_TARGET_RANDOM, 1, SPELL_LIGHTNING_WRATH))
                if (DoCastSpellIfCan(target, SPELL_LIGHTNING_WRATH) == CAST_OK)
                    m_wrath = urand(10000, 15000);
        }
        else
            m_wrath -= diff;

        if (m_crush <= diff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_CRUSH_ARMOR) ==
                CAST_OK)
                m_crush = urand(5000, 15000);
        }
        else
            m_crush -= diff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_doomwalker(Creature* pCreature)
{
    return new boss_doomwalkerAI(pCreature);
}

void AddSC_boss_doomwalker()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_doomwalker";
    pNewScript->GetAI = &GetAI_boss_doomwalker;
    pNewScript->RegisterSelf();
}
