/* Copyright (C) 2013 Corecraft */

/* ScriptData
SDName: boss_rokmar_the_crackler
SD%Complete: 100%
SDComment:
SDCategory: Coilfang Resevoir, Slave Pens
EndScriptData */

#include "precompiled.h"

enum
{
    SPELL_GRIEVOUS_WOUND_N = 31956,
    SPELL_GRIEVOUS_WOUND_H = 38801,
    SPELL_WATER_SPIT = 35008,
    SPELL_ENSNARING_MOSS = 31948,
    SPELL_FRENZY = 34970,
};

struct MANGOS_DLL_DECL boss_rokmar_the_cracklerAI : public ScriptedAI
{
    boss_rokmar_the_cracklerAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();
        Reset();
    }

    bool m_bIsRegularMode;
    uint32 m_uiSpitTimer;
    uint32 m_uiMossTimer;
    uint32 m_uiBleedTimer;
    bool m_bEnraged;

    void Reset() override
    {
        m_uiSpitTimer = 10000;
        m_uiMossTimer = urand(15000, 20000);
        m_uiBleedTimer = 15000;
        m_bEnraged = false;
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (m_uiSpitTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_WATER_SPIT) ==
                CAST_OK)
                m_uiSpitTimer = urand(10000, 20000);
        }
        else
            m_uiSpitTimer -= uiDiff;

        if (m_uiBleedTimer <= uiDiff)
        {
            uint32 id = m_bIsRegularMode ? SPELL_GRIEVOUS_WOUND_N :
                                           SPELL_GRIEVOUS_WOUND_H;
            Unit* target =
                m_creature->getVictim()->has_aura(id) ?
                    m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM,
                        1, id, SELECT_FLAG_IN_MELEE_RANGE) :
                    m_creature->getVictim();
            if (target)
                if (DoCastSpellIfCan(target, id) == CAST_OK)
                    m_uiBleedTimer = urand(20000, 30000);
        }
        else
            m_uiBleedTimer -= uiDiff;

        if (m_uiMossTimer <= uiDiff)
        {
            if (Unit* target = m_creature->SelectAttackingTarget(
                    ATTACKING_TARGET_RANDOM, 0))
                if (DoCastSpellIfCan(target, SPELL_ENSNARING_MOSS) == CAST_OK)
                    m_uiMossTimer = urand(20000, 30000);
        }
        else
            m_uiMossTimer -= uiDiff;

        if (m_uiSpitTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_WATER_SPIT) ==
                CAST_OK)
                m_uiSpitTimer = urand(10000, 20000);
        }
        else
            m_uiSpitTimer -= uiDiff;

        if (!m_bEnraged && m_creature->GetHealthPercent() <= 20)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_FRENZY) == CAST_OK)
                m_bEnraged = true;
        }

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_rokmar_the_cracklerAI(Creature* pCreature)
{
    return new boss_rokmar_the_cracklerAI(pCreature);
}

void AddSC_boss_rokmar_the_crackler()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_rokmar_the_crackler";
    pNewScript->GetAI = &GetAI_boss_rokmar_the_cracklerAI;
    pNewScript->RegisterSelf();
}