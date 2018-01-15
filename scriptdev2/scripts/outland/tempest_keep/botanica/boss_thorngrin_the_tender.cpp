/* Copyright (C) 2012 Corecraft */

/* ScriptData
SDName: boss_thorngrin_the_tender
SD%Complete: 100
SDComment:
SDCategory: Tempest Keep, The Botanica
EndScriptData */

#include "precompiled.h"

enum
{
    SPELL_ENRAGE = 34670,
    SPELL_SACRIFICE = 34661,
    SPELL_HELLFIRE_N = 34659,
    SPELL_HELLFIRE_H = 39131,

    SAY_ENTER_ROOM = -1553013,

    SAY_AGGRO = -1553014,
    SAY_DEATH = -1553021,
    SAY_KILL = -1553016,

    SAY_YELL_1 = -1553015,
    SAY_YELL_2 = -1553018,

    SAY_SACRIFICE = -1553017,
    SAY_HELLFIRE_1 = -1553019,
    SAY_HELLFIRE_2 = -1553020,
};

struct boss_thorngrin_the_tenderAI : public ScriptedAI
{
    bool m_bIsRegularMode;
    boss_thorngrin_the_tenderAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_bIsRegularMode = m_creature->GetMap()->IsRegularDifficulty();
        m_bDoYell = true;
        Reset();
    }

    uint32 m_uiSacrificeTimer;
    uint32 m_uiEnrageTimer;
    uint32 m_uiHellfireTimer;
    uint32 m_uiAggroWipe;
    bool m_bDoYell;
    float m_fhpYell;
    bool m_bMmapsOn;
    void Reset() override
    {
        m_uiEnrageTimer = 20000;
        m_uiSacrificeTimer = 10000;
        m_uiHellfireTimer = 7000;
        m_uiAggroWipe = 0;
        m_fhpYell = 50.0f;
        m_bMmapsOn = false;
    }

    void Aggro(Unit* /*pWho*/) override { DoScriptText(SAY_AGGRO, m_creature); }

    void MoveInLineOfSight(Unit* pWho) override
    {
        ScriptedAI::MoveInLineOfSight(pWho);

        if (pWho->GetTypeId() != TYPEID_PLAYER || !m_bDoYell)
            return;

        DoScriptText(SAY_ENTER_ROOM, m_creature);
        m_bDoYell = false;
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        DoScriptText(SAY_DEATH, m_creature);
    }

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_KILL);
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        // He stops hellfire and continues chasing if no one is in proximity of
        // it
        if (m_creature->has_aura(
                m_bIsRegularMode ? SPELL_HELLFIRE_N : SPELL_HELLFIRE_H))
        {
            bool stop = true;
            if (m_creature->FindNearestPlayer(15.0f) != nullptr)
                stop = false;
            if (stop)
                m_creature->InterruptNonMeleeSpells(false);
        }

        // Wipes aggro after sacrifice cast
        if (m_uiAggroWipe)
        {
            if (m_uiAggroWipe <= uiDiff)
            {
                DoResetThreat();
                m_uiAggroWipe = 0;
            }
            else
                m_uiAggroWipe -= uiDiff;
        }

        if (m_uiEnrageTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_ENRAGE) == CAST_OK)
            {
                m_creature->MonsterTextEmote("%s becomes enraged.", m_creature);
                m_uiEnrageTimer =
                    m_bIsRegularMode ? 30000 : urand(14000, 24000);
            }
        }
        else
            m_uiEnrageTimer -= uiDiff;

        if (m_uiSacrificeTimer <= uiDiff)
        {
            if (Unit* pTarget = m_creature->SelectAttackingTarget(
                    ATTACKING_TARGET_RANDOM, 1))
            {
                if (DoCastSpellIfCan(pTarget, SPELL_SACRIFICE) == CAST_OK)
                {
                    if (urand(0, 1))
                        DoScriptText(SAY_SACRIFICE, m_creature);
                    m_uiSacrificeTimer = m_bIsRegularMode ? 30000 : 14000;
                    m_uiAggroWipe = 1000;
                }
            }
        }
        else
            m_uiSacrificeTimer -= uiDiff;

        if (m_uiHellfireTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature,
                    m_bIsRegularMode ? SPELL_HELLFIRE_N : SPELL_HELLFIRE_H) ==
                CAST_OK)
            {
                if (urand(0, 1))
                {
                    if (urand(0, 1))
                        DoScriptText(SAY_HELLFIRE_1, m_creature);
                    else
                        DoScriptText(SAY_HELLFIRE_2, m_creature);
                }
            }
            m_uiHellfireTimer = urand(15000, 30000);
        }
        else
            m_uiHellfireTimer -= uiDiff;

        if (m_fhpYell >= m_creature->GetHealthPercent())
        {
            if (m_fhpYell == 50.0f)
            {
                DoScriptText(SAY_YELL_1, m_creature);
                m_fhpYell = 20.0f;
            }
            else
            {
                DoScriptText(SAY_YELL_2, m_creature);
                m_fhpYell = -1.0f;
            }
        }

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_thorngrin_the_tender(Creature* pCreature)
{
    return new boss_thorngrin_the_tenderAI(pCreature);
}

void AddSC_boss_thorngrin_the_tender()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_thorngrin_the_tender";
    pNewScript->GetAI = &GetAI_boss_thorngrin_the_tender;
    pNewScript->RegisterSelf();
}
