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
SDName: Boss_Nethermancer_Sepethrea
SD%Complete: 100
SDComment:
SDCategory: Tempest Keep, The Mechanar
EndScriptData */

#include "mechanar.h"
#include "precompiled.h"

#define SAY_AGGRO -1554013
#define SAY_SUMMON -1554014
#define SAY_DRAGONS_BREATH_1 -1554015
#define SAY_DRAGONS_BREATH_2 -1554016
#define SAY_KILL_1 -1554017
#define SAY_KILL_2 -1554018
#define SAY_DEATH -1554019

#define SPELL_SUMMON_RAGIN_FLAMES 35275
#define SPELL_SUMMON_RAGIN_FLAMES_H 39084

#define SPELL_FROST_ATTACK 35263
#define SPELL_ARCANE_BLAST 35314
#define SPELL_DRAGONS_BREATH 35250
#define SPELL_KNOCKBACK 37317
#define SPELL_SOLARBURN 35267

struct MANGOS_DLL_DECL boss_nethermancer_sepethreaAI : public ScriptedAI
{
    boss_nethermancer_sepethreaAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_pInstance = (instance_mechanar*)pCreature->GetInstanceData();
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();
        Reset();
    }

    instance_mechanar* m_pInstance;
    bool m_bIsRegularMode;
    bool m_bHasSummons;

    uint32 frost_attack_Timer;
    uint32 arcane_blast_Timer;
    uint32 dragons_breath_Timer;
    uint32 knockback_Timer;
    uint32 solarburn_Timer;

    std::list<ObjectGuid> m_summonedAdds;

    void Reset() override
    {
        frost_attack_Timer = urand(7000, 10000);
        arcane_blast_Timer = urand(12000, 18000);
        dragons_breath_Timer = urand(18000, 22000);
        knockback_Timer = urand(22000, 28000);
        solarburn_Timer = 30000;
        m_bHasSummons = false;

        if (m_pInstance)
        {
            if (GameObject* pGo = m_pInstance->GetSingleGameObjectFromStorage(
                    GO_NETHERMANCER_ENCOUNTER_DOOR))
                pGo->SetGoState(GO_STATE_ACTIVE);
            m_pInstance->SetData(TYPE_SEPETHREA, NOT_STARTED);
        }
    }

    void Aggro(Unit* /*pWho*/) override
    {
        DoScriptText(SAY_AGGRO, m_creature);

        if (m_pInstance)
        {
            if (GameObject* pGo = m_pInstance->GetSingleGameObjectFromStorage(
                    GO_NETHERMANCER_ENCOUNTER_DOOR))
                pGo->SetGoState(GO_STATE_READY);
            m_pInstance->SetData(TYPE_SEPETHREA, IN_PROGRESS);
        }

        // TODO: Should probably be handled in core, but much needed for this
        // script
        m_creature->SetInCombatWithZone();
    }

    void DespawnAdds()
    {
        for (auto& elem : m_summonedAdds)
        {
            if (Creature* pAdd = m_creature->GetMap()->GetCreature(elem))
            {
                if (pAdd->isDead())
                    continue;

                pAdd->ForcedDespawn();
            }
        }
    }

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_KILL_1, SAY_KILL_2);
    }

    void JustDied(Unit* /*Killer*/) override
    {
        DoScriptText(SAY_DEATH, m_creature);

        if (m_pInstance)
        {
            if (GameObject* pGo = m_pInstance->GetSingleGameObjectFromStorage(
                    GO_NETHERMANCER_ENCOUNTER_DOOR))
                pGo->SetGoState(GO_STATE_ACTIVE);

            m_pInstance->SetData(TYPE_SEPETHREA, DONE);
        }

        DespawnAdds();
    }

    void EnterEvadeMode(bool by_group = false) override
    {
        DespawnAdds();

        ScriptedAI::EnterEvadeMode(by_group);
    }

    void JustSummoned(Creature* pCreature) override
    {
        if (Unit* pTarget =
                m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0))
        {
            pCreature->AddThreat(pTarget, 500000);
            if (pCreature->AI())
                pCreature->AI()->AttackStart(pTarget);
        }
        pCreature->SetInCombatWithZone();

        m_summonedAdds.push_back(pCreature->GetObjectGuid());
    }

    void UpdateAI(const uint32 diff) override
    {
        // Return since we have no target
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (!m_bHasSummons)
        {
            if (DoCastSpellIfCan(m_creature,
                    m_bIsRegularMode ? SPELL_SUMMON_RAGIN_FLAMES :
                                       SPELL_SUMMON_RAGIN_FLAMES_H) == CAST_OK)
                m_bHasSummons = true;
        }

        // Frost Attack
        if (frost_attack_Timer <= diff)
        {
            DoCastSpellIfCan(m_creature->getVictim(), SPELL_FROST_ATTACK);
            frost_attack_Timer = urand(7000, 10000);
        }
        else
            frost_attack_Timer -= diff;

        // Arcane Blast
        if (arcane_blast_Timer <= diff)
        {
            DoCastSpellIfCan(m_creature->getVictim(), SPELL_ARCANE_BLAST);
            arcane_blast_Timer = 15000;
        }
        else
            arcane_blast_Timer -= diff;

        // Dragons Breath
        if (dragons_breath_Timer <= diff)
        {
            DoCastSpellIfCan(m_creature->getVictim(), SPELL_DRAGONS_BREATH);

            if (urand(0, 1))
                DoScriptText(
                    urand(0, 1) ? SAY_DRAGONS_BREATH_1 : SAY_DRAGONS_BREATH_2,
                    m_creature);

            dragons_breath_Timer = urand(12000, 22000);
        }
        else
            dragons_breath_Timer -= diff;

        // Check for Knockback
        if (knockback_Timer <= diff)
        {
            DoCastSpellIfCan(m_creature->getVictim(), SPELL_KNOCKBACK);
            knockback_Timer = urand(15000, 25000);
        }
        else
            knockback_Timer -= diff;

        // Check for Solarburn
        if (solarburn_Timer <= diff)
        {
            DoCastSpellIfCan(m_creature->getVictim(), SPELL_SOLARBURN);
            solarburn_Timer = 30000;
        }
        else
            solarburn_Timer -= diff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_nethermancer_sepethrea(Creature* pCreature)
{
    return new boss_nethermancer_sepethreaAI(pCreature);
}

#define SPELL_INFERNO 35268
#define SPELL_INFERNO_H 39346
#define SPELL_FIRE_TAIL 35278

struct MANGOS_DLL_DECL mob_ragin_flamesAI : public ScriptedAI
{
    mob_ragin_flamesAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_pInstance = (ScriptedInstance*)pCreature->GetInstanceData();
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();
        Reset();
    }

    ScriptedInstance* m_pInstance;
    bool m_bIsRegularMode;

    uint32 inferno_Timer;
    uint32 flame_timer;
    uint32 m_uiCastInfernoTimer;

    void Reset() override
    {
        inferno_Timer = urand(8000, 14000);
        flame_timer = 500;
        m_creature->ApplySpellImmune(
            0, IMMUNITY_DAMAGE, SPELL_SCHOOL_MASK_MAGIC, true);
        m_creature->ApplySpellImmune(
            0, IMMUNITY_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, true);
        m_uiCastInfernoTimer = 0;
    }

    void UpdateAI(const uint32 diff) override
    {
        // Cancel normal AI when casting inferno
        if (m_uiCastInfernoTimer != 0)
        {
            if (m_uiCastInfernoTimer <= diff)
            {
                // Resume movement
                m_creature->remove_auras(SPELL_INFERNO);
                if (Unit* pTarget = m_creature->SelectAttackingTarget(
                        ATTACKING_TARGET_RANDOM, 0))
                {
                    DoResetThreat();
                    AttackStart(pTarget);
                    m_creature->AddThreat(pTarget, 500000);
                    m_creature->SetTargetGuid(pTarget->GetObjectGuid());
                }
                m_uiCastInfernoTimer = 0;
                inferno_Timer = urand(8000, 14000);
            }
            else
            {
                m_creature->SetTargetGuid(
                    ObjectGuid()); // Prevent facing rotation
                m_uiCastInfernoTimer -= diff;
                return;
            }
        }

        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (inferno_Timer <= diff)
        {
            // We're immune to spells, so can't cast it. (INFERNO scales damage
            // wise)
            // Skipping heroic version, they tick the same spell that does the
            // normal/heroic amount
            // depending on if it's normal or heroic
            m_creature->AddAuraThroughNewHolder(SPELL_INFERNO, m_creature);

            m_uiCastInfernoTimer = 8000;
            return;
        }
        else
            inferno_Timer -= diff;

        if (flame_timer <= diff)
        {
            DoCastSpellIfCan(m_creature, SPELL_FIRE_TAIL);
            flame_timer = 500;
        }
        else
            flame_timer -= diff;

        DoMeleeAttackIfReady();
    }
};
CreatureAI* GetAI_mob_ragin_flames(Creature* pCreature)
{
    return new mob_ragin_flamesAI(pCreature);
}
void AddSC_boss_nethermancer_sepethrea()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_nethermancer_sepethrea";
    pNewScript->GetAI = &GetAI_boss_nethermancer_sepethrea;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "mob_ragin_flames";
    pNewScript->GetAI = &GetAI_mob_ragin_flames;
    pNewScript->RegisterSelf();
}
