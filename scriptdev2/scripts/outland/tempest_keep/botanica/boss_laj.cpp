/* Copyright (C) 2012 Corecraft */

/* ScriptData
SDName: Boss_Laj
SD%Complete: 100
SDComment:
SDCategory: Tempest Keep, The Botanica
EndScriptData */

#include "precompiled.h"

enum
{
    EMOTE_SUMMON = -1553006,

    SPELL_TELEPORT_LAJ = 34673,

    SPELL_ALLERGIC_REACTION = 34697,

    SPELL_SUMMON_LASHER_LEFT = 34681,
    SPELL_SUMMON_LASHER_RIGHT = 34684,
    SPELL_SUMMON_FLAYER_LEFT = 34685,
    SPELL_SUMMON_FLAYER_RIGHT = 34682,

    SPELL_LAJ_ARCANE = 34703,
    SPELL_LAJ_FIRE = 34704,
    SPELL_LAJ_FROST = 34705,
    SPELL_LAJ_NATURE = 34707,
    SPELL_LAJ_SHADOW = 34710,

    SPELL_PETRIFY = 34885,

    NPC_THORN_FLAYER = 19920,
    NPC_THORN_LASHER = 19919,
};

struct LajSpawnPos
{
    float X, Y, Z;
};

LajSpawnPos LeftPos = {-186.0, 376.6, -15.6517};
LajSpawnPos RightPos = {-185.9, 407.5, -15.7564};

struct MANGOS_DLL_DECL boss_lajAI : public ScriptedAI
{
    boss_lajAI(Creature* pCreature) : ScriptedAI(pCreature) { Reset(); }

    uint32 m_uiAllergicReaction;
    uint32 m_uiColorChange;
    uint32 m_uiTPTimer;

    void Reset() override
    {
        m_uiAllergicReaction = urand(2000, 8000);
        m_uiColorChange = urand(20000, 30000);
        m_uiTPTimer = 26000;

        m_creature->remove_auras(SPELL_LAJ_ARCANE);
        m_creature->remove_auras(SPELL_LAJ_FIRE);
        m_creature->remove_auras(SPELL_LAJ_FROST);
        m_creature->remove_auras(SPELL_LAJ_NATURE);
    }

    void Aggro(Unit*) override { m_creature->remove_auras(SPELL_PETRIFY); }

    void ChangeColor()
    {
        m_creature->remove_auras(SPELL_LAJ_ARCANE);
        m_creature->remove_auras(SPELL_LAJ_FIRE);
        m_creature->remove_auras(SPELL_LAJ_FROST);
        m_creature->remove_auras(SPELL_LAJ_NATURE);
        m_creature->remove_auras(SPELL_LAJ_SHADOW);

        switch (urand(0, 4))
        {
        case 0:
            m_creature->CastSpell(m_creature, SPELL_LAJ_ARCANE, true);
            break;
        case 1:
            m_creature->CastSpell(m_creature, SPELL_LAJ_FIRE, true);
            break;
        case 2:
            m_creature->CastSpell(m_creature, SPELL_LAJ_FROST, true);
            break;
        case 3:
            m_creature->CastSpell(m_creature, SPELL_LAJ_NATURE, true);
            break;
        case 4:
            m_creature->CastSpell(m_creature, SPELL_LAJ_SHADOW, true);
            break;
        }
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (m_uiAllergicReaction)
        {
            if (m_uiAllergicReaction <= uiDiff)
            {
                if (Unit* pTarget = m_creature->SelectAttackingTarget(
                        ATTACKING_TARGET_RANDOM, 0, SPELL_ALLERGIC_REACTION))
                {
                    if (DoCastSpellIfCan(pTarget, SPELL_ALLERGIC_REACTION) ==
                        CAST_OK)
                        m_uiAllergicReaction = 0;
                }
            }
            else
                m_uiAllergicReaction -= uiDiff;
        }

        if (m_uiColorChange <= uiDiff)
        {
            ChangeColor();
            m_uiColorChange = urand(20000, 30000);
        }
        else
            m_uiColorChange -= uiDiff;

        if (m_uiTPTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_TELEPORT_LAJ) == CAST_OK)
            {
                DoScriptText(EMOTE_SUMMON, m_creature);

                m_uiAllergicReaction = urand(2000, 8000);
                m_uiTPTimer = 26000;

                // Summon
                if (Creature* pCreature = m_creature->SummonCreature(
                        urand(0, 1) ? NPC_THORN_LASHER : NPC_THORN_FLAYER,
                        LeftPos.X, LeftPos.Y, LeftPos.Z, 0.0f,
                        TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 2500))
                    pCreature->AI()->AttackStart(m_creature->getVictim());
                if (Creature* pCreature = m_creature->SummonCreature(
                        urand(0, 1) ? NPC_THORN_LASHER : NPC_THORN_FLAYER,
                        RightPos.X, RightPos.Y, RightPos.Z, 0.0f,
                        TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 2500))
                    pCreature->AI()->AttackStart(m_creature->getVictim());
            }
        }
        else
            m_uiTPTimer -= uiDiff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_laj(Creature* pCreature)
{
    return new boss_lajAI(pCreature);
}

void AddSC_boss_laj()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_laj";
    pNewScript->GetAI = &GetAI_boss_laj;
    pNewScript->RegisterSelf();
}
