/* Copyright (C) 2013 Corecraft <https://www.worldofcorecraft.com>
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
SDName: Boss_Gruul
SD%Complete: 100
SDComment:
SDCategory: Gruul's Lair
EndScriptData */

#include "gruuls_lair.h"
#include "precompiled.h"

enum
{
    SAY_AGGRO = -1565010,
    SAY_SLAM_1 = -1565011,
    SAY_SLAM_2 = -1565012,
    SAY_SHATTER_1 = -1565013,
    SAY_SHATTER_2 = -1565014,
    SAY_KILL_1 = -1565015,
    SAY_KILL_2 = -1565016,
    SAY_KILL_3 = -1565017,
    SAY_DEATH = -1565018,
    EMOTE_GROWTH = -1565019,
    EMOTE_ROARS = -1565020,

    SPELL_GROWTH = 36300,
    SPELL_HURTFUL_STRIKE = 33813,
    SPELL_CAVE_IN = 36240,
    SPELL_REVERBERATION = 36297,
    SPELL_GROUND_SLAM = 33525, // Added a dummy to this spell
    SPELL_SHATTER = 33654,
};

struct MANGOS_DLL_DECL boss_gruulAI : public ScriptedAI
{
    boss_gruulAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_instance = (ScriptedInstance*)pCreature->GetInstanceData();
        Reset();
    }

    ScriptedInstance* m_instance;
    uint32 m_growth;
    uint32 m_hurtful;
    uint32 m_cave;
    uint32 m_reverberation;
    uint32 m_groundSlam;
    uint32 m_shatter;

    void Reset() override
    {
        m_growth = 30000;
        m_hurtful = 5000;
        m_cave = urand(5000, 15000);
        m_reverberation = 50000; // Not casted until first shatter
        m_groundSlam = 40000;
        m_shatter = 0;
    }

    void Aggro(Unit* /*pWho*/) override
    {
        if (m_instance)
            m_instance->SetData(TYPE_GRUUL, IN_PROGRESS);

        DoScriptText(SAY_AGGRO, m_creature);
    }

    void JustReachedHome() override
    {
        if (m_instance)
            m_instance->SetData(TYPE_GRUUL, FAIL);
    }

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_KILL_1, SAY_KILL_2, SAY_KILL_3);
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        if (m_instance)
            m_instance->SetData(TYPE_GRUUL, DONE);

        DoScriptText(SAY_DEATH, m_creature);
    }

    void GroundSlamKnockback()
    {
        Pacify(true);
        m_creature->movement_gens.push(new movement::StoppedMovementGenerator(),
            movement::EVENT_LEAVE_COMBAT, 70);
        m_shatter = 9000; // 1 sec disappears from the cast time of ground slam
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (m_shatter)
        {
            if (m_shatter <= uiDiff)
            {
                Pacify(false);
                m_creature->movement_gens.remove_all(movement::gen::stopped);
                if (DoCastSpellIfCan(m_creature, SPELL_SHATTER) == CAST_OK)
                {
                    DoScriptText(EMOTE_ROARS, m_creature);
                    DoScriptText(urand(0, 1) ? SAY_SHATTER_1 : SAY_SHATTER_2,
                        m_creature);
                    m_shatter = 0;
                    m_reverberation = urand(10000, 30000);
                }
            }
            else
                m_shatter -= uiDiff;
            if (m_growth > uiDiff)
                m_growth -= uiDiff;
            if (m_cave > uiDiff)
                m_cave -= uiDiff;
            return;
        }

        if (m_growth <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_GROWTH) == CAST_OK)
            {
                DoScriptText(EMOTE_GROWTH, m_creature);
                m_growth = 30000;
            }
        }
        else
            m_growth -= uiDiff;

        if (m_hurtful <= uiDiff)
        {
            Unit* target =
                m_creature->SelectAttackingTarget(ATTACKING_TARGET_TOPAGGRO, 1,
                    SPELL_HURTFUL_STRIKE, SELECT_FLAG_IN_MELEE_RANGE);
            if (DoCastSpellIfCan(target ? target : m_creature->getVictim(),
                    SPELL_HURTFUL_STRIKE) == CAST_OK)
                m_hurtful = 8000;
        }
        else
            m_hurtful -= uiDiff;

        if (m_cave <= uiDiff)
        {
            if (Unit* target = m_creature->SelectAttackingTarget(
                    ATTACKING_TARGET_RANDOM, 0, SPELL_CAVE_IN))
                if (DoCastSpellIfCan(target, SPELL_CAVE_IN) == CAST_OK)
                    m_cave = urand(5000, 15000);
        }
        else
            m_cave -= uiDiff;

        if (m_reverberation <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_REVERBERATION) == CAST_OK)
                m_reverberation = urand(25000, 45000);
        }
        else
            m_reverberation -= uiDiff;

        if (m_groundSlam <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_GROUND_SLAM) == CAST_OK)
            {
                DoScriptText(urand(0, 1) ? SAY_SLAM_1 : SAY_SLAM_2, m_creature);
                m_groundSlam = urand(60000, 80000);
            }
        }
        else
            m_groundSlam -= uiDiff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_gruul(Creature* pCreature)
{
    return new boss_gruulAI(pCreature);
}

bool DummyNPC_Gruul(
    Unit* tar, uint32 spellId, SpellEffectIndex /*effIndex*/, Creature* c)
{
    if (tar && tar->GetEntry() == NPC_GRUUL && spellId == SPELL_GROUND_SLAM)
    {
        if (boss_gruulAI* AI = dynamic_cast<boss_gruulAI*>(c->AI()))
            AI->GroundSlamKnockback();
    }
    return true;
}

void AddSC_boss_gruul()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_gruul";
    pNewScript->GetAI = &GetAI_boss_gruul;
    pNewScript->pEffectDummyNPC = &DummyNPC_Gruul;
    pNewScript->RegisterSelf();
}
