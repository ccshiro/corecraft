/* Copyright (C) 2013 Corecraft */

/* ScriptData
SDName: boss_zereketh_the_unbound
SD%Complete: 100
SDComment:
SDCategory: Tempest Keep, The Arcatraz
EndScriptData */

#include "arcatraz.h"
#include "precompiled.h"

enum
{
    SAY_AGGRO = -1530000,
    SAY_KILL_1 = -1530001,
    SAY_KILL_2 = -1530002,
    SAY_NOVA_1 = -1530003,
    SAY_NOVA_2 = -1530004,
    SAY_DEATH = -1530005,

    SPELL_VOID_ZONE = 36119,
    SPELL_SHADOW_NOVA_N = 36127,
    SPELL_SHADOW_NOVA_H = 39005,
    SPELL_SEED_OF_CORRUPTION_N = 36123,
    SPELL_SEED_OF_CORRUPTION_H = 39367,
};

struct MANGOS_DLL_DECL boss_zereketh_the_unboundAI : public ScriptedAI
{
    boss_zereketh_the_unboundAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_pInstance = (instance_arcatraz*)m_creature->GetInstanceData();
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();
        Reset();
    }

    instance_arcatraz* m_pInstance;
    bool m_bIsRegularMode;
    uint32 m_uiVoidZoneTimer;
    uint32 m_uiShadowNovaTimer;
    uint32 m_uiSeedTimer;
    ObjectGuid m_seedTarget;

    void Aggro(Unit* /*pWho*/) override
    {
        if (m_pInstance)
            m_pInstance->SetData(TYPE_ZEREKETH, IN_PROGRESS);

        DoScriptText(SAY_AGGRO, m_creature);
    }

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_KILL_1, SAY_KILL_2);
    }

    void JustReachedHome() override
    {
        if (m_pInstance)
            m_pInstance->SetData(TYPE_ZEREKETH, FAIL);
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        if (m_pInstance)
            m_pInstance->SetData(TYPE_ZEREKETH, DONE);

        DoScriptText(SAY_DEATH, m_creature);
    }

    void SpellDamageCalculation(const Unit* /*pDoneTo*/, int32& iDamage,
        const SpellEntry* pSpell, SpellEffectIndex effectIndex) override
    {
        if (pSpell->Id == SPELL_SHADOW_NOVA_N && effectIndex == EFFECT_INDEX_0)
        {
            // Damage taken from 2.1 dbc data:
            iDamage = urand(2082, 2418);
        }
        else if (pSpell->Id == SPELL_SHADOW_NOVA_H &&
                 effectIndex == EFFECT_INDEX_0)
        {
            // Damage taken from 2.1 dbc data:
            iDamage = urand(3700, 4300);
        }
        else if ((pSpell->Id == SPELL_SEED_OF_CORRUPTION_N ||
                     pSpell->Id == SPELL_SEED_OF_CORRUPTION_H) &&
                 effectIndex == EFFECT_INDEX_2)
        {
            // You were knockbacked further in 2.1
            iDamage = 150;
        }
    }

    void Reset() override
    {
        m_uiVoidZoneTimer = 15000;
        m_uiShadowNovaTimer = 15000;
        m_uiSeedTimer = urand(10000, 20000);
        m_seedTarget = ObjectGuid();
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (m_uiVoidZoneTimer <= uiDiff)
        {
            if (Unit* pTarget = m_creature->SelectAttackingTarget(
                    ATTACKING_TARGET_RANDOM, 0))
                if (DoCastSpellIfCan(pTarget, SPELL_VOID_ZONE) == CAST_OK)
                    m_uiVoidZoneTimer = urand(10000, 20000);
        }
        else
            m_uiVoidZoneTimer -= uiDiff;

        if (m_uiShadowNovaTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature,
                    m_bIsRegularMode ? SPELL_SHADOW_NOVA_N :
                                       SPELL_SHADOW_NOVA_H) == CAST_OK)
            {
                if (urand(0, 1))
                    DoScriptText(
                        urand(0, 1) ? SAY_NOVA_1 : SAY_NOVA_2, m_creature);

                m_uiShadowNovaTimer = urand(15000, 20000);
            }
        }
        else
            m_uiShadowNovaTimer -= uiDiff;

        if (m_uiSeedTimer <= uiDiff)
        {
            bool skipCast = false;
            // Make sure last seed target doesn't still have the spell on him
            if (Unit* lastSeedTar =
                    m_creature->GetMap()->GetCreature(m_seedTarget))
            {
                if (lastSeedTar->has_aura(m_bIsRegularMode ?
                                              SPELL_SEED_OF_CORRUPTION_N :
                                              SPELL_SEED_OF_CORRUPTION_H))
                {
                    m_uiSeedTimer = urand(10000, 20000);
                    skipCast = true;
                }
            }

            if (!skipCast)
            {
                if (Unit* pTarget = m_creature->SelectAttackingTarget(
                        ATTACKING_TARGET_RANDOM, 0, SPELL_SEED_OF_CORRUPTION_N))
                {
                    if (DoCastSpellIfCan(
                            pTarget, m_bIsRegularMode ?
                                         SPELL_SEED_OF_CORRUPTION_N :
                                         SPELL_SEED_OF_CORRUPTION_H) == CAST_OK)
                    {
                        m_uiSeedTimer = urand(10000, 20000);
                        m_seedTarget = pTarget->GetObjectGuid();
                    }
                }
            }
        }
        else
            m_uiSeedTimer -= uiDiff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_zereketh_the_unbound(Creature* pCreature)
{
    return new boss_zereketh_the_unboundAI(pCreature);
}

void AddSC_boss_zereketh_the_unbound()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_zereketh_the_unbound";
    pNewScript->GetAI = &GetAI_boss_zereketh_the_unbound;
    pNewScript->RegisterSelf();
}
