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
SDName: Boss_Maiden_of_Virtue
SD%Complete: 100
SDComment:
SDCategory: Karazhan
EndScriptData */

#include "karazhan.h"
#include "precompiled.h"

enum
{
    SAY_AGGRO = -1532018,
    SAY_KILL_1 = -1532019,
    SAY_KILL_2 = -1532020,
    SAY_KILL_3 = -1532021,
    SAY_REPENTANCE_1 = -1532022,
    SAY_REPENTANCE_2 = -1532023,
    SAY_DEATH = -1532024,

    SPELL_REPENTANCE = 29511,
    SPELL_HOLY_FIRE = 29522,
    SPELL_HOLY_WRATH = 32445,
    SPELL_HOLY_GROUND = 29512,
    SPELL_BERSERK = 26662,
};

struct MANGOS_DLL_DECL boss_maiden_of_virtueAI : public ScriptedAI
{
    boss_maiden_of_virtueAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_instance = pCreature->GetMapId() == KARAZHAN_MAP_ID ?
                         (instance_karazhan*)pCreature->GetInstanceData() :
                         NULL;
        Reset();
    }

    instance_karazhan* m_instance;
    uint32 m_repentanceTimer;
    uint32 m_holyFireTimer;
    uint32 m_holyWrathTimer;
    uint32 m_holyGroundTimer;
    uint32 m_berserkTimer;

    void Reset() override
    {
        m_repentanceTimer = urand(30000, 50000);
        m_holyFireTimer = urand(5000, 15000);
        m_holyWrathTimer = urand(15000, 25000);
        m_holyGroundTimer = 2000;
        m_berserkTimer = 10 * 60 * 1000; // Berserk after 10 minutes
    }

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_KILL_1, SAY_KILL_2, SAY_KILL_3);
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        DoScriptText(SAY_DEATH, m_creature);
        if (m_instance)
            m_instance->SetData(TYPE_MAIDEN, DONE);
    }

    void Aggro(Unit* /*pWho*/) override
    {
        DoScriptText(SAY_AGGRO, m_creature);
        if (m_instance)
            m_instance->SetData(TYPE_MAIDEN, IN_PROGRESS);
    }

    void JustReachedHome() override
    {
        if (m_instance)
            m_instance->SetData(TYPE_MAIDEN, FAIL);
    }

    Unit* MaidenSelectTarget(uint32 spellId)
    {
        // Select random target which we can cast spell on and that is not
        // affected by repentance
        std::vector<Unit*> targets;
        targets.reserve(m_creature->getThreatManager().getThreatList().size());
        for (const auto& elem : m_creature->getThreatManager().getThreatList())
        {
            if (Unit* t = m_creature->GetMap()->GetUnit((elem)->getUnitGuid()))
                if (t->GetTypeId() == TYPEID_PLAYER &&
                    !t->has_aura(SPELL_REPENTANCE) &&
                    CanCastSpell(t, spellId, false) == CAST_OK &&
                    !(spellId == SPELL_HOLY_FIRE &&
                        m_creature->GetDistance(t) <
                            12)) // Only cast holy fire on non-melee targets
                    targets.push_back(t);
        }
        if (targets.empty())
            return NULL;
        return targets[urand(0, targets.size() - 1)];
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (m_holyGroundTimer <= uiDiff)
        {
            DoCastSpellIfCan(m_creature, SPELL_HOLY_GROUND,
                CAST_TRIGGERED); // Triggered so it doesn't interrupt her at all
            m_holyGroundTimer = 2000;
        }
        else
            m_holyGroundTimer -= uiDiff;

        if (m_repentanceTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_REPENTANCE) == CAST_OK)
            {
                DoScriptText(urand(0, 1) ? SAY_REPENTANCE_1 : SAY_REPENTANCE_2,
                    m_creature);
                m_repentanceTimer = urand(30000, 50000);
            }
        }
        else
            m_repentanceTimer -= uiDiff;

        if (m_holyFireTimer <= uiDiff)
        {
            if (Unit* target = MaidenSelectTarget(SPELL_HOLY_FIRE))
                if (DoCastSpellIfCan(target, SPELL_HOLY_FIRE) == CAST_OK)
                    m_holyFireTimer = urand(5000, 15000);
        }
        else
            m_holyFireTimer -= uiDiff;

        if (m_holyWrathTimer <= uiDiff)
        {
            if (Unit* target = MaidenSelectTarget(SPELL_HOLY_WRATH))
            {
                if (DoCastSpellIfCan(target, SPELL_HOLY_WRATH) == CAST_OK)
                    m_holyWrathTimer = urand(15000, 25000);
            }
        }
        else
            m_holyWrathTimer -= uiDiff;

        if (m_berserkTimer)
        {
            if (m_berserkTimer <= uiDiff)
            {
                if (DoCastSpellIfCan(m_creature, SPELL_BERSERK) == CAST_OK)
                    m_berserkTimer = 0;
            }
            else
                m_berserkTimer -= uiDiff;
        }

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_maiden_of_virtue(Creature* pCreature)
{
    return new boss_maiden_of_virtueAI(pCreature);
}

void AddSC_boss_maiden_of_virtue()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_maiden_of_virtue";
    pNewScript->GetAI = &GetAI_boss_maiden_of_virtue;
    pNewScript->RegisterSelf();
}
