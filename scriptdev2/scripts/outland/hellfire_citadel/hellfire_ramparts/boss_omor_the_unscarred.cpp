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
SDName: Boss_Omar_The_Unscarred
SD%Complete: 100
SDComment:
SDCategory: Hellfire Citadel, Hellfire Ramparts
EndScriptData */

#include "precompiled.h"

enum
{
    SAY_AGGRO_1 = -1543009,
    SAY_AGGRO_2 = -1543010,
    SAY_AGGRO_3 = -1543011,
    SAY_SUMMON = -1543012,
    SAY_CURSE = -1543013,
    SAY_KILL_1 = -1543014,
    SAY_DIE = -1543015,
    SAY_WIPE = -1543016,

    SPELL_ORBITAL_STRIKE = 30637,
    SPELL_SHADOW_WHIP = 30638,
    SPELL_TREACHEROUS_AURA = 30695,
    SPELL_BANE_OF_TREACHERY_H = 37566,
    SPELL_DEMONIC_SHIELD = 31901,
    SPELL_SHADOW_BOLT = 30686,
    SPELL_SHADOW_BOLT_H = 39297,
    SPELL_SUMMON_FIENDISH_HOUND = 30707,

    // Hounds
    SPELL_DRAIN_LIFE = 35748,
    SPELL_MANA_BURN = 15785,
};

struct MANGOS_DLL_DECL boss_omor_the_unscarredAI : public Scripted_BehavioralAI
{
    boss_omor_the_unscarredAI(Creature* pCreature)
      : Scripted_BehavioralAI(pCreature)
    {
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();
        Reset();
    }

    bool m_bIsRegularMode;

    uint32 m_uiOrbitalStrikeTimer;
    uint32 m_uiShadowWhipTimer;
    uint32 m_uiAuraTimer;
    uint32 m_uiSummonTimer;
    ObjectGuid m_playerGuid;
    bool m_bCanPullBack;
    bool m_bHasUsedShieldWall;

    std::list<ObjectGuid> m_summonedHounds;

    void Reset() override
    {
        DoScriptText(SAY_WIPE, m_creature);

        m_uiOrbitalStrikeTimer = 25000;
        m_uiShadowWhipTimer = 2000;
        m_uiAuraTimer = 18000;
        m_uiSummonTimer = m_bIsRegularMode ? urand(19600, 23100) : 4000;
        m_playerGuid.Clear();
        m_bCanPullBack = false;
        m_bHasUsedShieldWall = false;

        ClearSummons();

        Scripted_BehavioralAI::Reset();
    }

    void ClearSummons()
    {
        for (auto& elem : m_summonedHounds)
        {
            if (Creature* pCreature = m_creature->GetMap()->GetCreature(elem))
            {
                if (pCreature->isAlive())
                    pCreature->ForcedDespawn();
            }
        }
        m_summonedHounds.clear();
    }

    void Aggro(Unit* /*pWho*/) override
    {
        switch (urand(0, 2))
        {
        case 0:
            DoScriptText(SAY_AGGRO_1, m_creature);
            break;
        case 1:
            DoScriptText(SAY_AGGRO_2, m_creature);
            break;
        case 2:
            DoScriptText(SAY_AGGRO_3, m_creature);
            break;
        }
    }

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_KILL_1);
    }

    void JustSummoned(Creature* pSummoned) override
    {
        DoScriptText(SAY_SUMMON, m_creature);

        m_summonedHounds.push_back(pSummoned->GetObjectGuid());

        if (Unit* pTarget =
                m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0))
            pSummoned->AI()->AttackStart(pTarget);
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        DoScriptText(SAY_DIE, m_creature);
        ClearSummons();
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (m_uiSummonTimer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_SUMMON_FIENDISH_HOUND) ==
                CAST_OK)
                m_uiSummonTimer =
                    m_bIsRegularMode ? urand(20000, 30000) : urand(8000, 14000);
        }
        else
            m_uiSummonTimer -= uiDiff;

        if (m_bCanPullBack)
        {
            if (m_uiShadowWhipTimer < uiDiff)
            {
                if (Player* pPlayer =
                        m_creature->GetMap()->GetPlayer(m_playerGuid))
                {
                    // script will attempt cast, even if orbital strike was
                    // resisted
                    if (pPlayer->GetZ() - 10.0f > m_creature->GetZ())
                        DoCastSpellIfCan(pPlayer, SPELL_SHADOW_WHIP,
                            CAST_INTERRUPT_PREVIOUS);
                }
                m_playerGuid.Clear();
                m_uiShadowWhipTimer = 2000;
                m_bCanPullBack = false;
            }
            else
                m_uiShadowWhipTimer -= uiDiff;
        }
        else if (m_uiOrbitalStrikeTimer < uiDiff)
        {
            if (DoCastSpellIfCan(
                    m_creature->getVictim(), SPELL_ORBITAL_STRIKE) == CAST_OK)
            {
                m_uiOrbitalStrikeTimer = 20000;
                m_playerGuid = m_creature->getVictim()->GetObjectGuid();

                m_bCanPullBack = true;
            }
        }
        else
            m_uiOrbitalStrikeTimer -= uiDiff;

        if (m_creature->GetHealthPercent() < 20.0f)
        {
            if (!m_bHasUsedShieldWall)
            {
                if (DoCastSpellIfCan(m_creature, SPELL_DEMONIC_SHIELD) ==
                    CAST_OK)
                    m_bHasUsedShieldWall = true;
            }
        }

        if (m_uiAuraTimer < uiDiff)
        {
            if (Unit* pTarget = m_creature->SelectAttackingTarget(
                    ATTACKING_TARGET_RANDOM, 0))
            {
                if (DoCastSpellIfCan(pTarget, m_bIsRegularMode ?
                                                  SPELL_TREACHEROUS_AURA :
                                                  SPELL_BANE_OF_TREACHERY_H) ==
                    CAST_OK)
                {
                    m_uiAuraTimer = urand(18000, 20000);
                    DoScriptText(SAY_CURSE, m_creature);
                }
            }
        }
        else
            m_uiAuraTimer -= uiDiff;

        Scripted_BehavioralAI::UpdateInCombatAI(uiDiff);

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_omor_the_unscarredAI(Creature* pCreature)
{
    return new boss_omor_the_unscarredAI(pCreature);
}

/* Hound */
struct MANGOS_DLL_DECL mob_add_fiendish_houndAI : public ScriptedAI
{
    mob_add_fiendish_houndAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();
        Reset();
    }

    bool m_bIsRegularMode;
    uint32 m_uiSpellTimer;

    void Reset() override { m_uiSpellTimer = m_bIsRegularMode ? 8000 : 3000; }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (m_uiSpellTimer <= uiDiff)
        {
            // Drain Life in normal, Mana Burn in heroic
            if (m_bIsRegularMode)
            {
                if (DoCastSpellIfCan(
                        m_creature->getVictim(), SPELL_DRAIN_LIFE) == CAST_OK)
                    m_uiSpellTimer = urand(8000, 16000);
            }
            else
            {
                if (m_creature->getVictim()->getPowerType() == POWER_MANA)
                {
                    if (DoCastSpellIfCan(m_creature->getVictim(),
                            SPELL_MANA_BURN) == CAST_OK)
                        m_uiSpellTimer = urand(6000, 8000);
                }
            }
        }
        else
            m_uiSpellTimer -= uiDiff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_mob_add_fiendish_hound(Creature* pCreature)
{
    return new mob_add_fiendish_houndAI(pCreature);
}

void AddSC_boss_omor_the_unscarred()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_omor_the_unscarred";
    pNewScript->GetAI = &GetAI_boss_omor_the_unscarredAI;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "mob_add_fiendish_hound";
    pNewScript->GetAI = &GetAI_mob_add_fiendish_hound;
    pNewScript->RegisterSelf();
}
