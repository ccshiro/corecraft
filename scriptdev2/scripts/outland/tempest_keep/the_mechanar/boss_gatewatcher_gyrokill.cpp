/* Copyright (C) 2012 Corecraft */

/* ScriptData
SDName: boss_gatewatcher_gyrokill
SD%Complete: 100
SDComment:
SDCategory: Tempest Keep, The Mechanar
EndScriptData */

#include "mechanar.h"
#include "precompiled.h"

enum
{
    SAY_AGGRO = -1502000,
    SAY_SAWBLADE_ONE = -1502001,
    SAY_SAWBLADE_TWO = -1502002,
    SAY_KILL_1 = -1502003,
    SAY_KILL_2 = -1502004,
    SAY_DEATH = -1502005,

    SPELL_STREAM_OF_MACHINE_FLUID = 35311,
    SPELL_SAW_BLADE_N = 35318,
    SPELL_SAW_BLADE_H = 39192,
    SPELL_SHADOW_POWER_N = 35322,
    SPELL_SHADOW_POWER_H = 39193,
};

struct MANGOS_DLL_DECL boss_gatewatcher_gyrokillAI : public ScriptedAI
{
    boss_gatewatcher_gyrokillAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_pInstance = (instance_mechanar*)pCreature->GetInstanceData();
        m_bIsRegularDifficulty = pCreature->GetMap()->IsRegularDifficulty();
        Reset();
    }

    instance_mechanar* m_pInstance;
    bool m_bIsRegularDifficulty;

    uint32 m_uiFluidTimer;
    uint32 m_uiSawTimer;
    uint32 m_uiShadowPowerTimer;

    void Aggro(Unit* /*pWho*/) override { DoScriptText(SAY_AGGRO, m_creature); }

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_KILL_1, SAY_KILL_2);
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        DoScriptText(SAY_DEATH, m_creature);

        if (!m_pInstance)
            return;

        if (GameObject* pGo =
                m_pInstance->GetSingleGameObjectFromStorage(GO_MOARG_2_DOOR))
            pGo->SetGoState(GO_STATE_ACTIVE);
        m_pInstance->SetData(TYPE_GATEWATCHER_GYROKILL, DONE);
    }

    void Reset() override
    {
        m_uiFluidTimer = urand(13000, 17000);
        m_uiSawTimer = urand(20000, 25000);
        m_uiShadowPowerTimer = urand(15000, 25000);
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (m_uiFluidTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(),
                    SPELL_STREAM_OF_MACHINE_FLUID) == CAST_OK)
                m_uiFluidTimer = urand(13000, 17000);
        }
        else
            m_uiFluidTimer -= uiDiff;

        if (m_uiSawTimer <= uiDiff)
        {
            if (Unit* pTarget = m_creature->SelectAttackingTarget(
                    ATTACKING_TARGET_RANDOM, 0))
            {
                if (DoCastSpellIfCan(pTarget, m_bIsRegularDifficulty ?
                                                  SPELL_SAW_BLADE_N :
                                                  SPELL_SAW_BLADE_H) == CAST_OK)
                {
                    DoScriptText(
                        urand(0, 1) ? SAY_SAWBLADE_ONE : SAY_SAWBLADE_TWO,
                        m_creature);
                    m_uiSawTimer = urand(20000, 25000);
                }
            }
        }
        else
            m_uiSawTimer -= uiDiff;

        if (m_uiShadowPowerTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature,
                    m_bIsRegularDifficulty ? SPELL_SHADOW_POWER_N :
                                             SPELL_SHADOW_POWER_H) == CAST_OK)
                m_uiShadowPowerTimer = urand(15000, 25000);
        }
        else
            m_uiShadowPowerTimer -= uiDiff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_gatewatcher_gyrokill(Creature* pCreature)
{
    return new boss_gatewatcher_gyrokillAI(pCreature);
}

void AddSC_boss_gatewatcher_gyrokill()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_gatewatcher_gyrokill";
    pNewScript->GetAI = &GetAI_boss_gatewatcher_gyrokill;
    pNewScript->RegisterSelf();
}