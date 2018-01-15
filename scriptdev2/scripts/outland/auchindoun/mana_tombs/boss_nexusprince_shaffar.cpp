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
SDName: Boss_NexusPrince_Shaffar
SD%Complete: 100
SDComment:
SDCategory: Auchindoun, Mana Tombs
EndScriptData */

/* ContentData
boss_nexusprince_shaffar
mob_ethereal_beacon
EndContentData */

#include "mana_tombs.h"
#include "precompiled.h"

enum
{
    SAY_INTRO = -1557000,
    SAY_AGGRO_1 = -1557001,
    SAY_AGGRO_2 = -1557002,
    SAY_AGGRO_3 = -1557003,
    SAY_KILL_1 = -1557004,
    SAY_KILL_2 = -1557005,
    SAY_SUMMON = -1557006,
    SAY_DEAD = -1557007,

    SPELL_BLINK = 34605,
    SPELL_FROSTBOLT = 32364,
    SPELL_FIREBALL = 32363,
    SPELL_FROSTNOVA = 32365,

    SPELL_ETHEREAL_BEACON = 32371, // Summons NPC_BEACON
    SPELL_ETHEREAL_BEACON_VISUAL = 32368,

    NPC_BEACON = 18431
};

struct MANGOS_DLL_DECL boss_nexusprince_shaffarAI : public Scripted_BehavioralAI
{
    boss_nexusprince_shaffarAI(Creature* pCreature)
      : Scripted_BehavioralAI(pCreature)
    {
        m_pInstance = (instance_mana_tombs*)pCreature->GetInstanceData();
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();
        m_bHasTaunted = false;
        Reset();
    }

    uint32 m_uiBlinkTimer;
    uint32 m_uiBeaconTimer;
    uint32 m_uiMainSpellTimer;
    uint32 m_uiFrostNovaTimer;

    instance_mana_tombs* m_pInstance;
    bool m_bIsRegularMode;
    bool m_bHasTaunted;

    void Reset() override
    {
        m_uiBlinkTimer = 30000;
        m_uiBeaconTimer = m_bIsRegularMode ? urand(20000, 30000) : 10000;
        m_uiMainSpellTimer = 1000;
        m_uiFrostNovaTimer = urand(15000, 20000);

        Scripted_BehavioralAI::Reset();
    }

    void MoveInLineOfSight(Unit* pWho) override
    {
        if (!m_bHasTaunted && pWho->GetTypeId() == TYPEID_PLAYER &&
            m_creature->IsWithinDistInMap(pWho, 100.0f))
        {
            DoScriptText(SAY_INTRO, m_creature);
            m_bHasTaunted = true;
        }

        Scripted_BehavioralAI::MoveInLineOfSight(pWho);
    }

    void Aggro(Unit* /*pWho*/) override
    {
        switch (urand(0, 2))
        {
        case 0:
            DoScriptText(SAY_AGGRO_1, m_creature);
            break;
        case 1:
            DoScriptText(SAY_AGGRO_2, m_creature);
            break;
        case 2:
            DoScriptText(SAY_AGGRO_3, m_creature);
            break;
        }

        if (m_pInstance)
            m_pInstance->SetData(TYPE_SHAFFAR, IN_PROGRESS);
    }

    void JustReachedHome() override
    {
        if (m_pInstance)
            m_pInstance->SetData(TYPE_SHAFFAR, FAIL);
    }

    void JustDied(Unit* /*pWho*/) override
    {
        DoScriptText(SAY_DEAD, m_creature);

        if (m_pInstance)
            m_pInstance->SetData(TYPE_SHAFFAR, DONE);
    }

    void JustSummoned(Creature* pSummoned) override
    {
        if (pSummoned->GetEntry() == NPC_BEACON)
        {
            pSummoned->CastSpell(
                pSummoned, SPELL_ETHEREAL_BEACON_VISUAL, false);

            if (Unit* pTarget = m_creature->SelectAttackingTarget(
                    ATTACKING_TARGET_RANDOM, 0))
                pSummoned->AI()->AttackStart(pTarget);
        }
    }

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_KILL_1, SAY_KILL_2);
    }

    void MovementInform(movement::gen gen_type, uint32 uiData) override
    {
        if (gen_type == movement::gen::point && uiData == 100)
            Scripted_BehavioralAI::ToggleBehavioralAI(true);
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        Scripted_BehavioralAI::UpdateInCombatAI(uiDiff);

        // Run away if target is frost novad
        bool hasNova = m_creature->getVictim()->has_aura(SPELL_FROSTNOVA);
        if (hasNova && !m_creature->movement_gens.has(movement::gen::point) &&
            m_creature->GetDistance(m_creature->getVictim()) < 8.0f)
        {
            Scripted_BehavioralAI::ToggleBehavioralAI(false);
            auto pos = m_creature->getVictim()->GetPoint(m_creature, 15.0f);
            m_creature->movement_gens.push(
                new movement::PointMovementGenerator(
                    100, pos.x, pos.y, pos.z, true, true),
                movement::EVENT_LEAVE_COMBAT);
        }
        else if (!hasNova &&
                 m_creature->movement_gens.has(movement::gen::point))
        {
            m_creature->movement_gens.remove_all(movement::gen::point);
            Scripted_BehavioralAI::ToggleBehavioralAI(true);
        }

        if (m_uiFrostNovaTimer <= uiDiff)
        {
            if (m_creature->getVictim()->CanReachWithMeleeAttack(m_creature))
            {
                m_creature->InterruptNonMeleeSpells(false);
                if (DoCastSpellIfCan(m_creature, SPELL_FROSTNOVA) == CAST_OK)
                    m_uiFrostNovaTimer = urand(15000, 20000);
            }
        }
        else
            m_uiFrostNovaTimer -= uiDiff;

        // This logic is now done in behavioral AI for a more proper experience:
        /*if (m_uiMainSpellTimer <= uiDiff)
        {
            bool fire, frost;
            uint32 spellId = 0;
            fire = !m_creature->IsSpellSchoolLocked(SPELL_SCHOOL_MASK_FIRE);
            frost = !m_creature->IsSpellSchoolLocked(SPELL_SCHOOL_MASK_FROST);
            if (fire || frost)
            {
                if (urand(fire ? 0 : 1, frost ? 1 : 0))
                    spellId = SPELL_FROSTBOLT;
                else
                    spellId = SPELL_FIREBALL;
            }
            if (DoCastSpellIfCan(m_creature->getVictim(), spellId) == CAST_OK)
                m_uiMainSpellTimer = urand(1000, 2000);
        }
        else
            m_uiMainSpellTimer -= uiDiff;*/

        if (m_uiBlinkTimer <= uiDiff)
        {
            m_creature->InterruptNonMeleeSpells(false);
            if (DoCastSpellIfCan(m_creature, SPELL_BLINK) == CAST_OK)
            {
                m_uiMainSpellTimer = 0; // Start a main spell right away
                m_uiBlinkTimer = 30000;
            }
        }
        else
            m_uiBlinkTimer -= uiDiff;

        if (m_uiBeaconTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_ETHEREAL_BEACON) == CAST_OK)
            {
                if (!urand(0, 3))
                    DoScriptText(SAY_SUMMON, m_creature);

                m_uiBeaconTimer = 10000;
            }
        }
        else
            m_uiBeaconTimer -= uiDiff;

        // Don't melee if target has frost nova
        if (!hasNova)
            DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_nexusprince_shaffar(Creature* pCreature)
{
    return new boss_nexusprince_shaffarAI(pCreature);
}

enum
{
    SPELL_ARCANE_BOLT = 15254,
    SPELL_ETHEREAL_APPRENTICE = 32372 // Summon 18430
};

struct MANGOS_DLL_DECL mob_ethereal_beaconAI : public ScriptedAI
{
    mob_ethereal_beaconAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();
        Reset();
    }

    bool m_bIsRegularMode;

    uint32 m_uiApprenticeTimer;
    uint32 m_uiArcaneBoltTimer;

    void Reset() override
    {
        m_uiApprenticeTimer = m_bIsRegularMode ? 20000 : 10000;
        m_uiArcaneBoltTimer = 1000;
    }

    void JustReachedHome() override
    {
        if (m_creature->IsTemporarySummon())
            m_creature->ForcedDespawn();
    }

    void JustSummoned(Creature* pSummoned) override
    {
        if (m_creature->getVictim())
            pSummoned->AI()->AttackStart(m_creature->getVictim());
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (m_uiArcaneBoltTimer <= uiDiff)
        {
            DoCastSpellIfCan(m_creature->getVictim(), SPELL_ARCANE_BOLT);
            m_uiArcaneBoltTimer = m_bIsRegularMode ? 8000 : 4000;
        }
        else
            m_uiArcaneBoltTimer -= uiDiff;

        if (m_uiApprenticeTimer)
        {
            if (m_uiApprenticeTimer <= uiDiff)
            {
                if (DoCastSpellIfCan(m_creature, SPELL_ETHEREAL_APPRENTICE) ==
                    CAST_OK)
                {
                    m_creature->ForcedDespawn(2000);
                    // despawn in 2 sec because of the spell visual
                    m_uiApprenticeTimer = 0;
                }
            }
            else
                m_uiApprenticeTimer -= uiDiff;
        }

        // should they do meele?
        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_mob_ethereal_beacon(Creature* pCreature)
{
    return new mob_ethereal_beaconAI(pCreature);
}

void AddSC_boss_nexusprince_shaffar()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_nexusprince_shaffar";
    pNewScript->GetAI = &GetAI_boss_nexusprince_shaffar;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "mob_ethereal_beacon";
    pNewScript->GetAI = &GetAI_mob_ethereal_beacon;
    pNewScript->RegisterSelf();
}
