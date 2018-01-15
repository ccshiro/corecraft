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
SDName: Boss_Curator
SD%Complete: 100%
SDComment:
SDCategory: Karazhan
EndScriptData */

#include "karazhan.h"
#include "precompiled.h"

enum
{
    SAY_AGGRO = -1532057,
    SAY_SUMMON_1 = -1532058,
    SAY_SUMMON_2 = -1532059,
    SAY_EVOCATE = -1532060,
    SAY_ARCANE_INFUSION = -1532061,
    SAY_KILL_1 = -1532062,
    SAY_KILL_2 = -1532063,
    SAY_DEATH = -1532064,

    // Flare
    NPC_ASTRAL_FLARE = 17096,
    SPELL_ASTRAL_FLARE_PASSIVE = 30234,

    // The Curator
    SPELL_HATEFUL_BOLT = 30383,
    SPELL_EVOCATION = 30254,
    SPELL_ASTRAL_ARMOR = 29476,
    SPELL_ARCANE_INFUSION = 30403,
    SPELL_ASTRAL_DECONSTRUCTION = 30407 // Hard enrage
};

struct MANGOS_DLL_DECL boss_curatorAI : public ScriptedAI
{
    boss_curatorAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_instance = pCreature->GetMapId() == KARAZHAN_MAP_ID ?
                         (instance_karazhan*)pCreature->GetInstanceData() :
                         NULL;
        Reset();
    }

    instance_karazhan* m_instance;
    uint32 m_flareTimer;
    uint32 m_hatefulTimer;
    uint32 m_enrageTimer;
    bool m_arcaneInfused;

    void Reset() override
    {
        m_flareTimer = 10000;
        m_hatefulTimer = urand(10000, 12000);
        m_enrageTimer = 12 * MINUTE * IN_MILLISECONDS;
        m_arcaneInfused = false;
        m_creature->ApplySpellImmune(
            0, IMMUNITY_DAMAGE, SPELL_SCHOOL_MASK_ARCANE, true);
        m_creature->SetAggroDistance(60.0f);
    }

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_KILL_1, SAY_KILL_2);
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        DoScriptText(SAY_DEATH, m_creature);
        if (m_instance)
            m_instance->SetData(TYPE_CURATOR, DONE);
    }

    void JustReachedHome() override
    {
        if (m_instance)
            m_instance->SetData(TYPE_CURATOR, FAIL);
    }

    void Aggro(Unit* /*pWho*/) override
    {
        DoScriptText(SAY_AGGRO, m_creature);
        if (m_instance)
            m_instance->SetData(TYPE_CURATOR, IN_PROGRESS);
        m_creature->CastSpell(m_creature, SPELL_ASTRAL_ARMOR, true);
    }

    void JustSummoned(Creature* pSummoned) override
    {
        if (pSummoned->GetEntry() == NPC_ASTRAL_FLARE)
        {
            // 40% chance to do a say
            if (urand(1, 10) <= 4)
                DoScriptText(
                    urand(0, 1) ? SAY_SUMMON_1 : SAY_SUMMON_2, m_creature);
        }
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->isInCombat())
            return;

        // Hard Enrage:
        if (m_enrageTimer)
        {
            if (m_enrageTimer <= uiDiff)
            {
                // Break evocation
                m_creature->remove_auras(SPELL_EVOCATION);

                if (DoCastSpellIfCan(m_creature, SPELL_ASTRAL_DECONSTRUCTION,
                        CAST_INTERRUPT_PREVIOUS) == CAST_OK)
                {
                    m_creature->remove_auras(SPELL_ASTRAL_ARMOR);
                    m_enrageTimer = 0;
                }
            }
            else
                m_enrageTimer -= uiDiff;
        }

        // Arcane Infusion (the 10% hp enrage):
        if (!m_arcaneInfused && m_creature->GetHealthPercent() < 11.0f)
        {
            // Break evocation
            m_creature->remove_auras(SPELL_EVOCATION);

            if (DoCastSpellIfCan(m_creature, SPELL_ARCANE_INFUSION,
                    CAST_INTERRUPT_PREVIOUS) == CAST_OK)
            {
                DoScriptText(SAY_ARCANE_INFUSION, m_creature);
                m_creature->remove_auras(SPELL_ASTRAL_ARMOR);
                m_arcaneInfused = true;
                m_hatefulTimer = 6000;
            }
        }

        // Not supposed to do anything while evocating
        if (m_creature->has_aura(SPELL_EVOCATION))
        {
            // Update timers, however:
            if (m_hatefulTimer > uiDiff)
                m_hatefulTimer -= uiDiff;
            if (m_flareTimer > uiDiff)
                m_flareTimer -= uiDiff;
        }

        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim() ||
            m_creature->has_aura(SPELL_EVOCATION))
            return;

        // Skip summons and evocations if enraged
        if (m_enrageTimer && !m_arcaneInfused)
        {
            if (!m_creature->has_aura(SPELL_ASTRAL_ARMOR))
                m_creature->CastSpell(m_creature, SPELL_ASTRAL_ARMOR, true);

            if (m_flareTimer <= uiDiff)
            {
                int32 mana = m_creature->GetPower(POWER_MANA);
                int32 maxMana = m_creature->GetMaxPower(POWER_MANA);
                bool summon = true;
                if (mana <= maxMana * 0.15f) // If we cannot afford a new summon
                                             // after this (regeneration
                                             // included)
                {
                    if (DoCastSpellIfCan(m_creature, SPELL_EVOCATION) ==
                        CAST_OK)
                    {
                        m_creature->remove_auras(SPELL_ASTRAL_ARMOR);
                        DoScriptText(SAY_EVOCATE, m_creature);
                    }
                    else
                        summon = false;
                }
                if (summon)
                {
                    // Lower mana
                    int32 newMana = mana - (maxMana / 10.0f);
                    if (newMana < 0)
                        newMana = 0;
                    m_creature->SetPower(POWER_MANA, newMana);

                    // Summon
                    auto pos = m_creature->GetPoint(
                        frand(0, 2 * M_PI_F), frand(10.0f, 20.0f));
                    m_creature->SummonCreature(NPC_ASTRAL_FLARE, pos.x, pos.y,
                        pos.z, 0.0f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,
                        30 * IN_MILLISECONDS);
                    m_flareTimer = 10000;
                }
            }
            else
                m_flareTimer -= uiDiff;
        }

        if (m_hatefulTimer <= uiDiff)
        {
            // Hateful bolt on: The person in top 3 on the aggro list with the
            // most health, and is not the main tank.
            if (Unit* tar1 = m_creature->SelectAttackingTarget(
                    ATTACKING_TARGET_TOPAGGRO, 1, SPELL_HATEFUL_BOLT))
            {
                Unit* tar2 = m_creature->SelectAttackingTarget(
                    ATTACKING_TARGET_TOPAGGRO, 2, SPELL_HATEFUL_BOLT);
                bool tar2MostHp =
                    tar2 ? tar2->GetHealth() > tar1->GetHealth() : false;
                if (DoCastSpellIfCan(tar2MostHp ? tar2 : tar1,
                        SPELL_HATEFUL_BOLT) == CAST_OK)
                    m_hatefulTimer =
                        !m_arcaneInfused ? urand(5000, 15000) : 6000;
            }
        }
        else
            m_hatefulTimer -= uiDiff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_curator(Creature* pCreature)
{
    return new boss_curatorAI(pCreature);
}

void AddSC_boss_curator()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_curator";
    pNewScript->GetAI = &GetAI_boss_curator;
    pNewScript->RegisterSelf();
}
