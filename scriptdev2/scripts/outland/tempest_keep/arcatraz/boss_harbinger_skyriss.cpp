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
SDName: Boss_Harbinger_Skyriss
SD%Complete: 100
SDComment:
SDCategory: Tempest Keep, The Arcatraz
EndScriptData */

/* ContentData
boss_harbinger_skyriss
boss_harbinger_skyriss_illusion -- TODO implement or move to ACID
EndContentData */

#include "arcatraz.h"
#include "precompiled.h"

enum
{
    SAY_INTRO = -1552000,
    SAY_AGGRO = -1552001,
    SAY_KILL_1 = -1552002,
    SAY_KILL_2 = -1552003,
    SAY_MIND_1 = -1552004,
    SAY_MIND_2 = -1552005,
    SAY_FEAR_1 = -1552006,
    SAY_FEAR_2 = -1552007,
    SAY_IMAGE = -1552008,
    SAY_DEATH = -1552009,

    SPELL_FEAR = 39415,
    SPELL_MIND_REND = 36924,
    SPELL_MIND_REND_H = 39017,
    SPELL_DOMINATION = 37162,
    SPELL_DOMINATION_H = 39019,
    SPELL_MANA_BURN_H = 39020,
    SPELL_66_ILLUSION = 36931, // Summons 21466
    SPELL_33_ILLUSION = 36932, // Summons 21467

    NPC_66_ILLUSION = 21466,
    NPC_33_ILLUSION = 21467,
};

struct MANGOS_DLL_DECL boss_harbinger_skyrissAI : public ScriptedAI
{
    boss_harbinger_skyrissAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_pInstance = (ScriptedInstance*)pCreature->GetInstanceData();
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();
        m_bIntroFinished = false;
        m_creature->SetFlag(
            UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE);
        Reset();
    }

    ScriptedInstance* m_pInstance;
    bool m_bIsRegularMode;

    bool m_bIntroFinished;
    bool m_bDidSplitImage33;
    bool m_bDidSplitImage66;

    uint32 m_uiIntroPhase;
    uint32 m_uiIntroTimer;
    uint32 m_uiMindRendTimer;
    uint32 m_uiFearTimer;
    uint32 m_uiDominationTimer;
    uint32 m_uiManaBurnTimer;

    void SpellDamageCalculation(const Unit* /*pDoneTo*/, int32& iDamage,
        const SpellEntry* pSpell, SpellEffectIndex effectIndex) override
    {
        if (pSpell->Id == SPELL_MIND_REND_H && effectIndex == EFFECT_INDEX_0)
            iDamage = 2000;
        else if (pSpell->Id == SPELL_MANA_BURN_H)
            iDamage = urand(
                1900, 2100); // Reduced in 2.1: Values extracted from 2.0.3 dbc
    }

    void Reset() override
    {
        m_bDidSplitImage33 = false;
        m_bDidSplitImage66 = false;

        m_uiIntroPhase = 1;
        m_uiIntroTimer = 5000;
        m_uiMindRendTimer = 5000;
        m_uiFearTimer = 10000;
        m_uiDominationTimer = 15000;
        m_uiManaBurnTimer = 9000;
    }

    void JustReachedHome() override
    {
        if (m_pInstance)
            m_pInstance->SetData(TYPE_HARBINGERSKYRISS, FAIL);
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        DoScriptText(SAY_DEATH, m_creature);

        if (m_pInstance)
        {
            m_pInstance->SetData(TYPE_HARBINGERSKYRISS, DONE);
            m_pInstance->SetData(TYPE_WARDEN_5, DONE);
        }
    }

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_KILL_1, SAY_KILL_2);
    }

    void JustSummoned(Creature* pSummoned) override
    {
        if (m_creature->getVictim() && pSummoned->AI())
            pSummoned->AI()->AttackStart(m_creature->getVictim());
    }

    Unit* SelectTarget(uint32 spellId, bool manaTargetsOnly = false)
    {
        // Include Millhouse in Target Selection:
        std::vector<Unit*> potentialTargets;
        if (m_pInstance)
            if (Unit* millhouse =
                    m_pInstance->GetSingleCreatureFromStorage(NPC_MILLHOUSE))
                if (millhouse->isAlive())
                    potentialTargets.push_back(millhouse);
        const ThreatList& tl = m_creature->getThreatManager().getThreatList();
        for (const auto& elem : tl)
        {
            if (Unit* tar =
                    m_creature->GetMap()->GetUnit((elem)->getUnitGuid()))
            {
                if (tar->GetTypeId() != TYPEID_PLAYER ||
                    (manaTargetsOnly && !tar->GetPower(POWER_MANA)) ||
                    CanCastSpell(tar, spellId, false) != CAST_OK)
                    continue;
                potentialTargets.push_back(tar);
            }
        }
        if (potentialTargets.empty())
            return NULL;
        return potentialTargets[urand(0, potentialTargets.size() - 1)];
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_bIntroFinished && !m_creature->isInCombat())
        {
            if (!m_pInstance)
                return;

            if (m_uiIntroTimer <= uiDiff)
            {
                switch (m_uiIntroPhase)
                {
                case 1:
                    DoScriptText(SAY_INTRO, m_creature);
                    ++m_uiIntroPhase;
                    m_uiIntroTimer = 30000;
                    break;
                case 2:
                    DoScriptText(SAY_AGGRO, m_creature);
                    if (Creature* pMellic =
                            m_pInstance->GetSingleCreatureFromStorage(
                                NPC_MELLICHAR))
                    {
                        // should have a better way to do this. possibly spell
                        // exist.
                        pMellic->SetDeathState(JUST_DIED);
                        pMellic->SetHealth(0);
                    }
                    if (GameObject* pSphere =
                            m_pInstance->GetSingleGameObjectFromStorage(
                                GO_SEAL_SPHERE))
                        pSphere->SetGoState(GO_STATE_ACTIVE);
                    ++m_uiIntroPhase;
                    m_uiIntroTimer = 3000;
                    break;
                case 3:
                    m_bIntroFinished = true;
                    m_creature->RemoveFlag(UNIT_FIELD_FLAGS,
                        UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE);
                    if (Player* nearest = m_creature->FindNearestPlayer(120.0f))
                        AttackStart(nearest);
                    break;
                }
            }
            else
                m_uiIntroTimer -= uiDiff;
        }

        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (!m_bDidSplitImage66 && m_creature->GetHealthPercent() <= 66.0f)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_66_ILLUSION) == CAST_OK)
            {
                m_bDidSplitImage66 = true;
                DoScriptText(SAY_IMAGE, m_creature);
            }
        }

        if (!m_bDidSplitImage33 && m_creature->GetHealthPercent() <= 33.0f)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_33_ILLUSION) == CAST_OK)
            {
                m_bDidSplitImage33 = true;
                DoScriptText(SAY_IMAGE, m_creature);
            }
        }

        if (m_uiFearTimer <= uiDiff)
        {
            if (Unit* pTarget = SelectTarget(SPELL_FEAR))
            {
                if (DoCastSpellIfCan(pTarget, SPELL_FEAR) == CAST_OK)
                {
                    if (urand(0, 1))
                        DoScriptText(
                            urand(0, 1) ? SAY_FEAR_1 : SAY_FEAR_2, m_creature);
                    m_uiFearTimer = urand(10000, 20000);
                }
            }
        }
        else
            m_uiFearTimer -= uiDiff;

        if (m_uiDominationTimer <= uiDiff)
        {
            if (Unit* pTarget =
                    m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM,
                        0, SPELL_DOMINATION, SELECT_FLAG_PLAYER))
            {
                if (DoCastSpellIfCan(pTarget,
                        m_bIsRegularMode ? SPELL_DOMINATION :
                                           SPELL_DOMINATION_H) == CAST_OK)
                {
                    DoScriptText(
                        urand(0, 1) ? SAY_MIND_1 : SAY_MIND_2, m_creature);
                    m_uiDominationTimer = urand(30000, 60000);
                }
            }
        }
        else
            m_uiDominationTimer -= uiDiff;

        if (!m_bIsRegularMode)
        {
            if (m_uiManaBurnTimer <= uiDiff)
            {
                if (Unit* pTarget = SelectTarget(SPELL_MANA_BURN_H, true))
                    if (DoCastSpellIfCan(pTarget, SPELL_MANA_BURN_H) == CAST_OK)
                        m_uiManaBurnTimer = urand(9000, 15000);
            }
            else
                m_uiManaBurnTimer -= uiDiff;
        }

        if (m_uiMindRendTimer <= uiDiff)
        {
            if (Unit* pTarget = SelectTarget(SPELL_MIND_REND))
                if (DoCastSpellIfCan(pTarget, m_bIsRegularMode ?
                                                  SPELL_MIND_REND :
                                                  SPELL_MIND_REND_H) == CAST_OK)
                    m_uiMindRendTimer = 5000;
        }
        else
            m_uiMindRendTimer -= uiDiff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_harbinger_skyriss(Creature* pCreature)
{
    return new boss_harbinger_skyrissAI(pCreature);
}

enum
{
    SPELL_MIND_REND_IMAGE = 36929,
    SPELL_MIND_REND_IMAGE_H = 39021,
};

struct MANGOS_DLL_DECL boss_harbinger_skyriss_illusionAI : public ScriptedAI
{
    boss_harbinger_skyriss_illusionAI(Creature* pCreature)
      : ScriptedAI(pCreature)
    {
        m_pInstance = (ScriptedInstance*)pCreature->GetInstanceData();
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();
        Reset();
    }

    ScriptedInstance* m_pInstance;
    bool m_bIsRegularMode;
    uint32 m_uiMindRendTimer;

    void Reset() override
    {
        m_uiMindRendTimer = urand(5000, 10000);
        if (m_creature->GetEntry() == NPC_33_ILLUSION)
            m_creature->SetHealthPercent(33.0f);
        else
            m_creature->SetHealthPercent(66.0f);
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (m_uiMindRendTimer <= uiDiff)
        {
            if (Unit* pTarget = m_creature->SelectAttackingTarget(
                    ATTACKING_TARGET_RANDOM, 0, SPELL_MIND_REND_IMAGE))
                if (DoCastSpellIfCan(pTarget,
                        m_bIsRegularMode ? SPELL_MIND_REND_IMAGE :
                                           SPELL_MIND_REND_IMAGE_H) == CAST_OK)
                    m_uiMindRendTimer = urand(5000, 10000);
        }
        else
            m_uiMindRendTimer -= uiDiff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_harbinger_skyriss_illusion(Creature* pCreature)
{
    return new boss_harbinger_skyriss_illusionAI(pCreature);
}

void AddSC_boss_harbinger_skyriss()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_harbinger_skyriss";
    pNewScript->GetAI = &GetAI_boss_harbinger_skyriss;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "boss_harbinger_skyriss_illusion";
    pNewScript->GetAI = &GetAI_boss_harbinger_skyriss_illusion;
    pNewScript->RegisterSelf();
}
