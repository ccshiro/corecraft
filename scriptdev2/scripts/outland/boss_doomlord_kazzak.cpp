/* Copyright (C) 2006 - 2012 ScriptDev2 <http://www.scriptdev2.com/>, 2013
 *Corecraft <https://www.worldofcorecraft.com>
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
SDName: Boss_Doomlord_Kazzak
SD%Complete: 100
SDComment:
SDCategory: Hellfire Peninsula
EndScriptData */

#include "precompiled.h"

enum
{
    SAY_INTRO = -1000147,
    SAY_AGGRO_1 = -1000148,
    SAY_AGGRO_2 = -1000149,
    SAY_SURPREME_1 = -1000150,
    SAY_SURPREME_2 = -1000151,
    SAY_KILL_1 = -1000152,
    SAY_KILL_2 = -1000153,
    SAY_WIPE = -1000154,
    SAY_DEATH = -1000155,
    EMOTE_GENERIC_ENRAGE = -1000003,

    SPELL_SHADOW_VOLLEY = 32963,
    SPELL_CLEAVE = 16044,
    SPELL_THUNDERCLAP = 36706,
    SPELL_MARK_OF_KAZZAK = 32960,
    SPELL_FRENZY = 32964,
    SPELL_CAPTURE_SOUL = 32966,
    SPELL_TWISTED_REFLECTION = 21063,
    SPELL_TWISTED_HEAL = 21064
};

struct MANGOS_DLL_DECL boss_doomlordkazzakAI : public ScriptedAI
{
    boss_doomlordkazzakAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        Reset();
    }

    uint32 m_aggroTimer;
    uint32 m_volley;
    uint32 m_cleave;
    uint32 m_thunderclap;
    uint32 m_mark;
    uint32 m_enrage;
    uint32 m_twisted;
    time_t last_aggro_say = 0;
    time_t last_evade_say = 0;

    void Reset() override
    {
        m_aggroTimer = 1000;
        m_volley = urand(3000, 8000);
        m_cleave = urand(8000, 12000);
        m_thunderclap = urand(20000, 40000);
        m_mark = 10000;
        m_enrage = urand(50000, 60000);
        m_twisted = 5000;

        m_creature->SetAggroDistance(60.0f);
    }

    void JustReachedHome() override
    {
        if (last_evade_say + 8 < WorldTimer::time_no_syscall())
        {
            last_evade_say = WorldTimer::time_no_syscall();
            DoScriptText(SAY_WIPE, m_creature);
        }
    }

    void JustRespawned() override { DoScriptText(SAY_INTRO, m_creature); }

    void Aggro(Unit* /*pWho*/) override
    {
        if (last_aggro_say + 8 < WorldTimer::time_no_syscall())
        {
            last_aggro_say = WorldTimer::time_no_syscall();
            DoScriptText(urand(0, 1) ? SAY_AGGRO_1 : SAY_AGGRO_2, m_creature);
        }
    }

    void DamageDeal(Unit* pDoneTo, uint32& uiDamage) override
    {
        if (uiDamage > 0 && pDoneTo->has_aura(SPELL_TWISTED_REFLECTION))
            DoCastSpellIfCan(m_creature, SPELL_TWISTED_HEAL, CAST_TRIGGERED);
    }

    void KilledUnit(Unit* victim) override
    {
        // Targets that do not cause a heal:
        // 1. Opposing faction players (of the ones he's fighting)
        // 2. Players not level 70
        // 3. Pets & Totems
        if (victim->GetTypeId() != TYPEID_PLAYER || victim->getLevel() != 70 ||
            m_creature->tapping_team() !=
                static_cast<Player*>(victim)->GetTeam())
            return;

        DoCastSpellIfCan(m_creature, SPELL_CAPTURE_SOUL, CAST_TRIGGERED);

        DoKillSay(m_creature, victim, SAY_KILL_1, SAY_KILL_2);
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        DoScriptText(SAY_DEATH, m_creature);
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
        {
            if (m_aggroTimer <= uiDiff)
            {
                DoAggroEnemyInRange(m_creature, 40.0f, true);
                m_aggroTimer = 1000;
            }
            else
                m_aggroTimer -= uiDiff;
            return;
        }

        if (m_volley <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_SHADOW_VOLLEY) == CAST_OK)
                m_volley = urand(10000, 20000);
        }
        else
            m_volley -= uiDiff;

        if (m_cleave <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_CLEAVE) ==
                CAST_OK)
                m_cleave = urand(8000, 12000);
        }
        else
            m_cleave -= uiDiff;

        if (m_thunderclap <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_THUNDERCLAP) == CAST_OK)
                m_thunderclap = urand(20000, 40000);
        }
        else
            m_thunderclap -= uiDiff;

        if (m_mark <= uiDiff)
        {
            if (Unit* target =
                    m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM,
                        0, SPELL_MARK_OF_KAZZAK, SELECT_FLAG_POWER_MANA))
                if (DoCastSpellIfCan(target, SPELL_MARK_OF_KAZZAK) == CAST_OK)
                    m_mark = urand(20000, 40000);
        }
        else
            m_mark -= uiDiff;

        if (m_enrage <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_FRENZY) == CAST_OK)
            {
                DoScriptText(
                    urand(0, 1) ? SAY_SURPREME_1 : SAY_SURPREME_2, m_creature);
                DoScriptText(EMOTE_GENERIC_ENRAGE, m_creature);
                m_enrage = urand(50000, 60000);
            }
        }
        else
            m_enrage -= uiDiff;

        if (m_twisted <= uiDiff)
        {
            if (Unit* target = m_creature->SelectAttackingTarget(
                    ATTACKING_TARGET_RANDOM, 1, SPELL_TWISTED_REFLECTION))
                if (DoCastSpellIfCan(target, SPELL_TWISTED_REFLECTION) ==
                    CAST_OK)
                    m_twisted = urand(20000, 40000);
        }
        else
            m_twisted -= uiDiff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_doomlordkazzak(Creature* pCreature)
{
    return new boss_doomlordkazzakAI(pCreature);
}

void AddSC_boss_doomlordkazzak()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_doomlord_kazzak";
    pNewScript->GetAI = &GetAI_boss_doomlordkazzak;
    pNewScript->RegisterSelf();
}
