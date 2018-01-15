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
SDName: Boss_Warbringer_Omrogg
SD%Complete: 100
SDComment:
SDCategory: Hellfire Citadel, Shattered Halls
EndScriptData */

/* ContentData
mob_omrogg_heads
boss_warbringer_omrogg
EndContentData */

#include "precompiled.h"
#include "shattered_halls.h"

enum
{
    YELL_DIE_L = -1540039,
    YELL_DIE_R = -1540040,
    EMOTE_ENRAGE = -1540041,

    SPELL_BLAST_WAVE = 30600,
    SPELL_FEAR = 30584,
    SPELL_THUNDERCLAP = 30633,
    SPELL_BEATDOWN = 30618,
    SPELL_BURNING_MAUL = 30598,
    SPELL_BURNING_MAUL_H = 36056,

    NPC_LEFT_HEAD = 19523,
    NPC_RIGHT_HEAD = 19524
};

struct Yell
{
    int32 id;
    uint32 creature;
};

static Yell GoCombat[] = {
    {-1540018, NPC_LEFT_HEAD}, {-1540019, NPC_LEFT_HEAD},
    {-1540020, NPC_LEFT_HEAD},
};
static Yell GoCombatDelay[] = {
    {-1540021, NPC_RIGHT_HEAD}, {-1540022, NPC_RIGHT_HEAD},
    {-1540023, NPC_RIGHT_HEAD}, {-1540034, NPC_LEFT_HEAD},
};

static Yell Threat[] = {
    {-1540024, NPC_LEFT_HEAD}, {-1540025, NPC_RIGHT_HEAD},
    {-1540027, NPC_LEFT_HEAD}, {-1540026, NPC_LEFT_HEAD},
};
static Yell ThreatDelay1[] = {
    {-1540028, NPC_RIGHT_HEAD}, {-1540029, NPC_LEFT_HEAD},
    {-1540031, NPC_RIGHT_HEAD}, {-1540030, NPC_RIGHT_HEAD},
};
static Yell ThreatDelay2[] = {
    {-1540032, NPC_LEFT_HEAD}, {-1540033, NPC_RIGHT_HEAD},
    {-1540035, NPC_LEFT_HEAD},
};

static Yell Killing[] = {
    {-1540036, NPC_LEFT_HEAD}, {-1540037, NPC_RIGHT_HEAD},
};
static Yell KillingDelay[] = {
    {-1540038, NPC_RIGHT_HEAD},
};

struct MANGOS_DLL_DECL mob_omrogg_headsAI : public ScriptedAI
{
    mob_omrogg_headsAI(Creature* pCreature) : ScriptedAI(pCreature) { Reset(); }

    uint32 m_uiDeath_Timer;
    bool m_bDeathYell;

    void Reset() override
    {
        m_uiDeath_Timer = 4000;
        m_bDeathYell = false;
    }

    void DoDeathYell() { m_bDeathYell = true; }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_bDeathYell)
            return;

        if (m_uiDeath_Timer < uiDiff)
        {
            DoScriptText(YELL_DIE_R, m_creature);
            m_uiDeath_Timer = false;
            m_creature->SetDeathState(JUST_DIED);
        }
        else
            m_uiDeath_Timer -= uiDiff;
    }
};

struct MANGOS_DLL_DECL boss_warbringer_omroggAI : public ScriptedAI
{
    boss_warbringer_omroggAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_pInstance = (ScriptedInstance*)pCreature->GetInstanceData();
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();
        Reset();
    }

    ScriptedInstance* m_pInstance;
    bool m_bIsRegularMode;

    ObjectGuid m_leftHeadGuid;
    ObjectGuid m_rightHeadGuid;

    int m_iAggro;
    int m_iThreat;
    int m_iKilling;

    bool m_bAggroYell;
    bool m_bAggroYell2;
    bool m_bThreatYell;
    bool m_bThreatYell2;
    bool m_bKillingYell;

    uint32 m_uiDelay_Timer;
    uint32 m_uiBurningMaulActive_Timer;
    uint32 m_uiBlastWave_Timer;
    uint32 m_uiRoar_Timer;
    uint32 m_uiThunderClap_Timer;
    uint32 m_uiBeatDown_Timer;
    uint32 m_uiStunned_Timer;

    void Reset() override
    {
        if (Creature* pLeftHead =
                m_creature->GetMap()->GetCreature(m_leftHeadGuid))
        {
            pLeftHead->DealDamage(pLeftHead, pLeftHead->GetMaxHealth(), NULL,
                DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, NULL, false, false);
            m_leftHeadGuid.Clear();
        }

        if (Creature* pRightHead =
                m_creature->GetMap()->GetCreature(m_rightHeadGuid))
        {
            pRightHead->DealDamage(pRightHead, pRightHead->GetMaxHealth(), NULL,
                DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, NULL, false, false);
            m_rightHeadGuid.Clear();
        }

        m_bAggroYell = false;
        m_bAggroYell2 = false;
        m_bThreatYell = false;
        m_bThreatYell2 = false;
        m_bKillingYell = false;

        m_uiDelay_Timer = 4000;
        m_uiBlastWave_Timer = urand(2000, 5000);
        m_uiRoar_Timer = urand(20000, 30000); // Custom timer (2.4.3 adjusting),
                                              // 50,000 is the correct timer
        m_uiThunderClap_Timer = 13000;
        m_uiBeatDown_Timer = m_bIsRegularMode ? 30000 : 20000;
        m_uiStunned_Timer = 0;

        if (m_pInstance)
            m_pInstance->SetData(TYPE_OMROGG, NOT_STARTED);

        m_creature->SetAggroDistance(0.0f); // Reset aggro dist: Kargath ups it
                                            // for us if players try to pull
                                            // kargath before omrogg
    }

    void DoYellForThreat()
    {
        Creature* pLeftHead = m_creature->GetMap()->GetCreature(m_leftHeadGuid);
        Creature* pRightHead =
            m_creature->GetMap()->GetCreature(m_rightHeadGuid);

        if (!pLeftHead || !pRightHead)
            return;

        m_iThreat = irand(0, 3);

        Unit* pSource =
            (pLeftHead->GetEntry() == Threat[m_iThreat].creature ? pLeftHead :
                                                                   pRightHead);

        DoScriptText(Threat[m_iThreat].id, pSource);

        m_uiDelay_Timer = 3500;
        m_bThreatYell = true;
    }

    void Aggro(Unit* /*pWho*/) override
    {
        m_creature->SummonCreature(
            NPC_LEFT_HEAD, 0.0f, 0.0f, 0.0f, 0.0f, TEMPSUMMON_DEAD_DESPAWN, 0);
        m_creature->SummonCreature(
            NPC_RIGHT_HEAD, 0.0f, 0.0f, 0.0f, 0.0f, TEMPSUMMON_DEAD_DESPAWN, 0);

        if (Creature* pLeftHead =
                m_creature->GetMap()->GetCreature(m_leftHeadGuid))
        {
            m_iAggro = irand(0, 2);

            DoScriptText(GoCombat[m_iAggro].id, pLeftHead);

            m_uiDelay_Timer = 3500;
            m_bAggroYell = true;
        }

        if (m_pInstance)
            m_pInstance->SetData(TYPE_OMROGG, IN_PROGRESS);
    }

    void JustSummoned(Creature* pSummoned) override
    {
        if (pSummoned->GetEntry() == NPC_LEFT_HEAD)
            m_leftHeadGuid = pSummoned->GetObjectGuid();

        if (pSummoned->GetEntry() == NPC_RIGHT_HEAD)
            m_rightHeadGuid = pSummoned->GetObjectGuid();

        // summoned->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
        // summoned->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
        pSummoned->SetVisibility(VISIBILITY_OFF);
    }

    void KilledUnit(Unit* /*pVictim*/) override
    {
        Creature* pLeftHead = m_creature->GetMap()->GetCreature(m_leftHeadGuid);
        Creature* pRightHead =
            m_creature->GetMap()->GetCreature(m_rightHeadGuid);

        if (!pLeftHead || !pRightHead)
            return;

        m_iKilling = irand(0, 1);

        Creature* pSource =
            (pLeftHead->GetEntry() == Killing[m_iKilling].creature ?
                    pLeftHead :
                    pRightHead);

        switch (m_iKilling)
        {
        case 0:
            DoScriptText(Killing[m_iKilling].id, pSource);
            m_uiDelay_Timer = 3500;
            m_bKillingYell = true;
            break;
        case 1:
            DoScriptText(Killing[m_iKilling].id, pSource);
            m_bKillingYell = false;
            break;
        }
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        Creature* pLeftHead = m_creature->GetMap()->GetCreature(m_leftHeadGuid);
        Creature* pRightHead =
            m_creature->GetMap()->GetCreature(m_rightHeadGuid);

        if (!pLeftHead || !pRightHead)
            return;

        DoScriptText(YELL_DIE_L, pLeftHead);

        pLeftHead->DealDamage(pLeftHead, pLeftHead->GetMaxHealth(), NULL,
            DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, NULL, false, false);
        pRightHead->DealDamage(pLeftHead, pLeftHead->GetMaxHealth(), NULL,
            DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, NULL, false, false);

        if (mob_omrogg_headsAI* pHeadAI =
                dynamic_cast<mob_omrogg_headsAI*>(pRightHead->AI()))
            pHeadAI->DoDeathYell();

        if (m_pInstance)
            m_pInstance->SetData(TYPE_OMROGG, DONE);
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (m_uiDelay_Timer < uiDiff)
        {
            m_uiDelay_Timer = 3500;

            Creature* pLeftHead =
                m_creature->GetMap()->GetCreature(m_leftHeadGuid);
            Creature* pRightHead =
                m_creature->GetMap()->GetCreature(m_rightHeadGuid);

            if (!pLeftHead || !pRightHead)
                return;

            if (m_bAggroYell2)
            {
                DoScriptText(GoCombatDelay[3].id, pLeftHead);
                m_bAggroYell2 = false;
            }

            if (m_bAggroYell)
            {
                DoScriptText(GoCombatDelay[m_iAggro].id, pRightHead);
                m_bAggroYell = false;

                if (m_iAggro == 0)
                    m_bAggroYell2 = true;
            }

            if (m_bThreatYell2)
            {
                Creature* pSource =
                    (pLeftHead->GetEntry() == ThreatDelay2[m_iThreat].creature ?
                            pLeftHead :
                            pRightHead);

                DoScriptText(ThreatDelay2[m_iThreat].id, pSource);
                m_bThreatYell2 = false;
            }

            if (m_bThreatYell)
            {
                Creature* pSource =
                    (pLeftHead->GetEntry() == ThreatDelay1[m_iThreat].creature ?
                            pLeftHead :
                            pRightHead);

                DoScriptText(ThreatDelay1[m_iThreat].id, pSource);
                m_bThreatYell = false;
                if (!(m_iThreat == 3))
                    m_bThreatYell2 = true;
            }

            if (m_bKillingYell)
            {
                Creature* pSource = (pLeftHead->GetEntry() ==
                                             KillingDelay[m_iKilling].creature ?
                                         pLeftHead :
                                         pRightHead);

                DoScriptText(KillingDelay[m_iKilling].id, pSource);
                m_bKillingYell = false;
            }
        }
        else
            m_uiDelay_Timer -= uiDiff;

        // Stun the boss so he makes his Roar emote properly.
        if (m_uiStunned_Timer)
        {
            if (m_uiStunned_Timer <= uiDiff)
            {
                m_creature->clearUnitState(UNIT_STAT_STUNNED);
                m_uiStunned_Timer = 0;
            }
            else
                m_uiStunned_Timer -= uiDiff;
        }

        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (m_uiThunderClap_Timer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_THUNDERCLAP) ==
                CAST_OK)
                m_uiThunderClap_Timer = urand(10000, 60000);
        }
        else
            m_uiThunderClap_Timer -= uiDiff;

        if (m_uiRoar_Timer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_FEAR) ==
                CAST_OK)
            {
                DoScriptText(EMOTE_ENRAGE, m_creature);
                DoCastSpellIfCan(m_creature,
                    m_bIsRegularMode ? SPELL_BURNING_MAUL :
                                       SPELL_BURNING_MAUL_H,
                    CAST_TRIGGERED);
                m_creature->addUnitState(UNIT_STAT_STUNNED);

                m_uiRoar_Timer = urand(65000, 75000);
                m_uiStunned_Timer = 2000;
            }
        }
        else
            m_uiRoar_Timer -= uiDiff;

        // Only use Blast Wave while Burning Maul is active
        if (m_creature->has_aura(
                m_bIsRegularMode ? SPELL_BURNING_MAUL : SPELL_BURNING_MAUL_H))
        {
            if (m_uiBlastWave_Timer < uiDiff)
            {
                if (DoCastSpellIfCan(
                        m_creature->getVictim(), SPELL_BLAST_WAVE) == CAST_OK)
                    m_uiBlastWave_Timer = urand(8000, 10000);
            }
            else
                m_uiBlastWave_Timer -= uiDiff;
        }

        if (m_uiBeatDown_Timer < uiDiff)
        {
            ThreatList const& tList =
                m_creature->getThreatManager().getThreatList();

            // Check that there's more than one target
            if (tList.size() > 1)
            {
                if (DoCastSpellIfCan(
                        m_creature, SPELL_BEATDOWN, CAST_TRIGGERED) == CAST_OK)
                {
                    // Get first on threat
                    ThreatList::const_iterator itr = tList.begin();
                    // Get second on threat
                    ThreatList::const_iterator itr2 = tList.begin();
                    std::advance(itr2, 1);

                    if (Unit* secondTarget = m_creature->GetMap()->GetUnit(
                            (*itr2)->getUnitGuid()))
                    {
                        DoYellForThreat();

                        // Remove all threat from the person first on aggro
                        if (Unit* mainTarget = m_creature->GetMap()->GetUnit(
                                (*itr)->getUnitGuid()))
                        {
                            if (m_creature->getThreatManager().getThreat(
                                    mainTarget))
                                m_creature->getThreatManager()
                                    .modifyThreatPercent(mainTarget, -100);

                            // Add threat to the target that was second on
                            // threat before
                            m_creature->AddThreat(secondTarget, 100000);
                            m_uiBeatDown_Timer = m_uiBeatDown_Timer ?
                                                     urand(20000, 45000) :
                                                     urand(18000, 28000);
                        }
                    }
                }
            }
        }
        else
            m_uiBeatDown_Timer -= uiDiff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_warbringer_omrogg(Creature* pCreature)
{
    return new boss_warbringer_omroggAI(pCreature);
}

CreatureAI* GetAI_mob_omrogg_heads(Creature* pCreature)
{
    return new mob_omrogg_headsAI(pCreature);
}

void AddSC_boss_warbringer_omrogg()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_warbringer_omrogg";
    pNewScript->GetAI = &GetAI_boss_warbringer_omrogg;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "mob_omrogg_heads";
    pNewScript->GetAI = &GetAI_mob_omrogg_heads;
    pNewScript->RegisterSelf();
}
