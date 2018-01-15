/* Copyright (C) 2013 Corecraft */

/* ScriptData
SDName: boss_dalliah_the_doomsayer
SD%Complete: 100
SDComment:
SDCategory: Tempest Keep, The Arcatraz
EndScriptData */

#include "arcatraz.h"
#include "precompiled.h"

enum
{
    SAY_AGGRO = -1530006,
    SAY_KILL_1 = -1530007,
    SAY_KILL_2 = -1530008,
    SAY_DEATH = -1530009,
    SAY_HEAL_1 = -1530010,
    SAY_HEAL_2 = -1530011,
    SAY_WHIRLDWIND_1 = -1530012,
    SAY_WHIRLDWIND_2 = -1530013,

    SPELL_WHIRLWIND = 36142,
    SPELL_HEAL_N = 36144,
    SPELL_HEAL_H = 39013,
    SPELL_SHADOW_WAVE_H = 39016,
    SPELL_GIFT_DOOMSAYER_N = 36173,
    SPELL_GIFT_DOOMSAYER_H = 39009,

    // SPELL_DOUBLE_ATTACK AURA = 18943 -- Added to creature_template_addon
};

struct MANGOS_DLL_DECL boss_dalliah_the_doomsayerAI : public ScriptedAI
{
    boss_dalliah_the_doomsayerAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_pInstance = (instance_arcatraz*)m_creature->GetInstanceData();
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();
        Reset();
    }

    instance_arcatraz* m_pInstance;
    bool m_bIsRegularMode;
    uint32 m_uiWhirlwindTimer;
    bool m_bDoHeal;
    uint32 m_uiWaveTimer;
    uint32 m_uiGiftTimer;
    int32 m_nextSayPct;

    void MovementInform(movement::gen uiMoveType, uint32 uiPointId) override
    {
        if (uiMoveType == movement::gen::point && uiPointId == 100)
        {
            m_creature->SetFacingTo(1.0f);
            m_creature->SetOrientation(1.0f);
            m_creature->movement_gens.remove_all(movement::gen::idle);
            m_creature->movement_gens.push(
                new movement::IdleMovementGenerator());
        }
    }

    void Aggro(Unit* /*pWho*/) override
    {
        if (m_pInstance)
            m_pInstance->SetData(TYPE_DALLIAH, IN_PROGRESS);

        DoScriptText(SAY_AGGRO, m_creature);
    }

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_KILL_1, SAY_KILL_2);
    }

    void JustReachedHome() override
    {
        if (m_pInstance)
            m_pInstance->SetData(TYPE_DALLIAH, FAIL);
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        if (m_pInstance)
            m_pInstance->SetData(TYPE_DALLIAH, DONE);

        DoScriptText(SAY_DEATH, m_creature);
    }

    void Reset() override
    {
        m_uiWhirlwindTimer = 10000;
        m_bDoHeal = false;
        m_uiWaveTimer = 20000;
        m_uiGiftTimer = 7000;
        m_nextSayPct = 75;
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (m_uiWhirlwindTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_WHIRLWIND) == CAST_OK)
            {
                DoScriptText(urand(0, 1) ? SAY_WHIRLDWIND_1 : SAY_WHIRLDWIND_2,
                    m_creature);
                m_uiWhirlwindTimer = urand(25000, 35000);
                m_bDoHeal = true;
            }
        }
        else
            m_uiWhirlwindTimer -= uiDiff;

        if (m_bDoHeal)
        {
            if (DoCastSpellIfCan(m_creature,
                    m_bIsRegularMode ? SPELL_HEAL_N : SPELL_HEAL_H) == CAST_OK)
            {
                DoScriptText(urand(0, 1) ? SAY_HEAL_1 : SAY_HEAL_2, m_creature);
                m_bDoHeal = false;
            }
        }

        if (m_uiGiftTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(),
                    m_bIsRegularMode ? SPELL_GIFT_DOOMSAYER_N :
                                       SPELL_GIFT_DOOMSAYER_H) == CAST_OK)
                m_uiGiftTimer = urand(15000, 20000);
        }
        else
            m_uiGiftTimer -= uiDiff;

        if (!m_bIsRegularMode)
        {
            if (m_uiWaveTimer <= uiDiff)
            {
                if (Unit* pTarget = m_creature->SelectAttackingTarget(
                        ATTACKING_TARGET_RANDOM, 0, SPELL_SHADOW_WAVE_H))
                {
                    if (DoCastSpellIfCan(pTarget, SPELL_SHADOW_WAVE_H) ==
                        CAST_OK)
                        m_uiWaveTimer = 10000;
                }
            }
            else
                m_uiWaveTimer -= uiDiff;
        }

        if (m_creature->GetHealthPercent() <= m_nextSayPct)
        {
            Creature* soccothrates =
                m_pInstance ? m_pInstance->GetSoccothrates() : nullptr;
            if (soccothrates)
            {
                if (m_nextSayPct == 75)
                    DoScriptText(SAY_SOC_DH_TAUNT_1, soccothrates);
                else if (m_nextSayPct == 50)
                    DoScriptText(SAY_SOC_DH_TAUNT_2, soccothrates);
                else if (m_nextSayPct == 25)
                    DoScriptText(SAY_SOC_DH_TAUNT_3, soccothrates);
            }
            m_nextSayPct -= 25;
        }

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_dalliah_the_doomsayer(Creature* pCreature)
{
    return new boss_dalliah_the_doomsayerAI(pCreature);
}

void AddSC_boss_dalliah_the_doomsayer()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_dalliah_the_doomsayer";
    pNewScript->GetAI = &GetAI_boss_dalliah_the_doomsayer;
    pNewScript->RegisterSelf();
}
