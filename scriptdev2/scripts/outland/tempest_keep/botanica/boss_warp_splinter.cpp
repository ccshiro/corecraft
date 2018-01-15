/* Copyright (C) 2012 Corecraft */

/* ScriptData
SDName: Boss_Warp_Splinter
SD%Complete: 100
SDComment:
SDCategory: Tempest Keep, The Botanica
EndScriptData */

#include "precompiled.h"
#include <vector>

enum
{
    SAY_AGGRO = -1553007,
    SAY_KILL_1 = -1553008,
    SAY_KILL_2 = -1553009,
    SAY_SUMMON_1 = -1553010,
    SAY_SUMMON_2 = -1553011,
    SAY_DEATH = -1553012,

    ANCESTRAL_LIFE_TIMER = 25000,

    SPELL_WAR_STOMP = 34716,
    SPELL_ARCANE_VOLLEY_N = 36705,
    SPELL_ARCANE_VOLLEY_H = 39133,

    // Sapplings
    SPELL_SUMMON_SAPLINGS = 34741,
    SPELL_SAPLING_1 = 34727,
    SPELL_SAPLING_2 = 34730,
    SPELL_SAPLING_3 = 34731,
    SPELL_SAPLING_4 = 34733,
    SPELL_SAPLING_5 = 34732,
    SPELL_SAPLING_6 = 34734,
    SPELL_ANCESTRAL_LIFE = 34742,
};

struct MANGOS_DLL_DECL boss_warp_splinterAI : public ScriptedAI
{
    boss_warp_splinterAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_bIsRegularDifficulty = pCreature->GetMap()->IsRegularDifficulty();
        Reset();
    }

    bool m_bIsRegularDifficulty;

    uint32 m_uiArcaneVolleyTimer;
    uint32 m_uiWarStompTimer;
    uint32 m_uiSaplingTimer;
    uint32 m_ancestralTimer;

    void Aggro(Unit* /*pWho*/) override { DoScriptText(SAY_AGGRO, m_creature); }

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_KILL_1, SAY_KILL_2);
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        DoScriptText(SAY_DEATH, m_creature);
    }

    std::vector<ObjectGuid> m_summonedAdds;
    void JustSummoned(Creature* pCreature) override
    {
        m_summonedAdds.push_back(pCreature->GetObjectGuid());
        // Make them move towards me (in case they don't aggro someone when
        // summoned)
        pCreature->movement_gens.push(
            new movement::FollowMovementGenerator(m_creature));
    }

    void Reset() override
    {
        m_uiArcaneVolleyTimer = 17000;
        m_uiWarStompTimer = urand(5000, 10000);
        m_uiSaplingTimer = 25000;
        m_ancestralTimer = 0;

        for (auto guid : m_summonedAdds)
            if (Creature* c = m_creature->GetMap()->GetPet(guid))
                c->ForcedDespawn();
        m_summonedAdds.clear();
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (m_uiArcaneVolleyTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(),
                    m_bIsRegularDifficulty ? SPELL_ARCANE_VOLLEY_N :
                                             SPELL_ARCANE_VOLLEY_H) == CAST_OK)
                m_uiArcaneVolleyTimer = urand(20000, 25000);
        }
        else
            m_uiArcaneVolleyTimer -= uiDiff;

        if (m_uiWarStompTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_WAR_STOMP) == CAST_OK)
                m_uiWarStompTimer = urand(20000, 40000);
        }
        else
            m_uiWarStompTimer -= uiDiff;

        if (m_ancestralTimer <= uiDiff)
        {
            for (auto guid : m_summonedAdds)
                if (Creature* c = m_creature->GetMap()->GetPet(guid))
                {
                    c->CastSpell(m_creature, 31270, true); // Moonfire
                    c->CastSpell(m_creature, 20664, true); // Rejuvenation
                    c->ForcedDespawn(3000);
                }
        }
        else
            m_ancestralTimer -= uiDiff;

        if (m_uiSaplingTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_SUMMON_SAPLINGS) == CAST_OK)
            {
                DoScriptText(
                    urand(0, 1) ? SAY_SUMMON_1 : SAY_SUMMON_2, m_creature);
                m_creature->CastSpell(m_creature, SPELL_SAPLING_1, true);
                m_creature->CastSpell(m_creature, SPELL_SAPLING_2, true);
                m_creature->CastSpell(m_creature, SPELL_SAPLING_3, true);
                m_creature->CastSpell(m_creature, SPELL_SAPLING_4, true);
                m_creature->CastSpell(m_creature, SPELL_SAPLING_5, true);
                m_creature->CastSpell(m_creature, SPELL_SAPLING_6, true);
                m_uiSaplingTimer = urand(45000, 55000);
                m_ancestralTimer = ANCESTRAL_LIFE_TIMER;
            }
        }
        else
            m_uiSaplingTimer -= uiDiff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_warp_splinter(Creature* pCreature)
{
    return new boss_warp_splinterAI(pCreature);
}

void AddSC_boss_warp_splinter()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_warp_splinter";
    pNewScript->GetAI = &GetAI_boss_warp_splinter;
    pNewScript->RegisterSelf();
}
