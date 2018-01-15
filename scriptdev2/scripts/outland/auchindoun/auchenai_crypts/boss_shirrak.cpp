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
SDName: boss_shirrak
SD%Complete: 100
SDComment:
SDCategory: Auchindoun, Auchenai Crypts
EndScriptData */

#include "auchenai_crypts.h"
#include "precompiled.h"

enum
{
    EMOTE_FOCUS = -1558010,

    SPELL_CARNIVOROUS_BITE = 36383,
    SPELL_CARNIVOROUS_BITE_H = 39382,
    SPELL_INHIBIT_MAGIC = 32264,
    SPELL_ATTRACT_MAGIC = 32265,

    SPELL_FOCUS_TARGET_VISUAL = 32286,
    SPELL_PING_SHIRRAK = 32301,
    NPC_FOCUS_FIRE = 18374,

    INHIBIT_MAGIC_RANGE = 55,

    NPC_SHIRRAK = 18371
};

struct MANGOS_DLL_DECL boss_shirrakAI : public ScriptedAI
{
    boss_shirrakAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_pInstance = (instance_auchenai_crypts*)pCreature->GetInstanceData();
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();
        Reset();
    }

    instance_auchenai_crypts* m_pInstance;
    bool m_bIsRegularMode;

    uint32 m_uiCarnivorousBiteTimer;
    uint32 m_uiFocusFireTimer;
    uint32 m_uiAttractMagicTimer;
    uint32 m_uiInhibitMagicTimer;

    uint8 m_uiFocusFireCount;

    ObjectGuid m_focusTargetGuid;

    void Reset() override
    {
        m_uiCarnivorousBiteTimer = 10000;
        m_uiFocusFireTimer = urand(15000, 30000);
        m_uiAttractMagicTimer = urand(25000, 30000);
        m_uiInhibitMagicTimer = 100;
        m_uiFocusFireCount = 0;
    }

    void Aggro(Unit* /*pWho*/) override
    {
        if (m_pInstance)
            m_pInstance->SetData(TYPE_SHIRRAK, IN_PROGRESS);
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        if (m_pInstance)
            m_pInstance->SetData(TYPE_SHIRRAK, DONE);
    }

    void JustReachedHome() override
    {
        if (m_pInstance)
            m_pInstance->SetData(TYPE_SHIRRAK, FAIL);
    }

    void JustSummoned(Creature* pSummoned) override
    {
        // The focus fire creature casts the focus fire visual
        if (pSummoned->GetEntry() == NPC_FOCUS_FIRE)
            pSummoned->CastSpell(pSummoned, SPELL_FOCUS_TARGET_VISUAL, true);
    }

    void InhibitMagic()
    {
        // Spell has been changed to single target
        auto targets =
            AllEnemiesInRange(m_creature, m_creature, INHIBIT_MAGIC_RANGE);

        for (auto u : targets)
        {
            float dist = m_creature->GetDistance(u);
            int stacks = 4 - int(dist / 10);
            if (stacks <= 0)
                stacks = 1;

            m_creature->CastSpell(u, SPELL_INHIBIT_MAGIC, true);
            if (AuraHolder* inhib = u->get_aura(
                    SPELL_INHIBIT_MAGIC, m_creature->GetObjectGuid()))
                inhib->SetStackAmount(stacks);
        }
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        // Inhibit magic is cast even when out of combat
        if (m_uiInhibitMagicTimer <= uiDiff)
        {
            InhibitMagic();
            m_uiInhibitMagicTimer = 3000;
        }
        else
            m_uiInhibitMagicTimer -= uiDiff;

        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (m_uiCarnivorousBiteTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature,
                    m_bIsRegularMode ? SPELL_CARNIVOROUS_BITE :
                                       SPELL_CARNIVOROUS_BITE_H) == CAST_OK)
                m_uiCarnivorousBiteTimer = urand(7000, 10000);
        }
        else
            m_uiCarnivorousBiteTimer -= uiDiff;

        if (m_uiAttractMagicTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_ATTRACT_MAGIC) == CAST_OK)
                m_uiAttractMagicTimer = urand(35000, 45000);
        }
        else
            m_uiAttractMagicTimer -= uiDiff;

        if (m_uiFocusFireTimer <= uiDiff)
        {
            ++m_uiFocusFireCount;
            Unit* pTarget = NULL;

            switch (m_uiFocusFireCount)
            {
            case 1:
            {
                // engage the target
                pTarget = m_creature->SelectAttackingTarget(
                    ATTACKING_TARGET_RANDOM, 1, uint32(0), SELECT_FLAG_PLAYER);

                if (!pTarget)
                    pTarget = m_creature->getVictim();

                DoScriptText(EMOTE_FOCUS, m_creature, pTarget);
                m_focusTargetGuid = pTarget->GetObjectGuid();
                // no break;
            }
            case 2:
                // we have a delay of 1 sec between the summons
                m_uiFocusFireTimer = 1000;
                break;
            case 3:
                // reset the timers and the summon count
                m_uiFocusFireCount = 0;
                m_uiFocusFireTimer = urand(15000, 30000);
                break;
            }

            if (!pTarget)
                pTarget = m_creature->GetMap()->GetUnit(m_focusTargetGuid);

            // Summon focus fire at target location
            if (pTarget)
                m_creature->SummonCreature(NPC_FOCUS_FIRE, pTarget->GetX(),
                    pTarget->GetY(), pTarget->GetZ(), 0,
                    TEMPSUMMON_TIMED_DESPAWN, 10000);
        }
        else
            m_uiFocusFireTimer -= uiDiff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_shirrak(Creature* pCreature)
{
    return new boss_shirrakAI(pCreature);
}

struct MANGOS_DLL_DECL npc_focus_fireAI : public ScriptedAI
{
    npc_focus_fireAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();
        m_uiPingShirrakTimer = 3000;
        Reset();
    }

    bool m_bIsRegularMode;
    uint32 m_uiPingShirrakTimer;

    void SpellDamageCalculation(const Unit* /*pDoneTo*/, int32& iDamage,
        const SpellEntry* /*pSpell*/, SpellEffectIndex /*index*/) override
    {
        // Damage was nerfed in 2.1.
        // Damage in 2.0.3 is 1600 on normal and 3800 on heroic
        if (m_bIsRegularMode)
            iDamage = urand(1550, 1650);
        else
            iDamage = urand(3750, 3850);
    }

    void Reset() override {}

    void UpdateAI(const uint32 uiDiff) override
    {
        if (m_uiPingShirrakTimer)
        {
            if (m_uiPingShirrakTimer <= uiDiff)
            {
                if (Creature* shirrak = GetClosestCreatureWithEntry(
                        m_creature, NPC_SHIRRAK, 80.0f, true))
                    m_creature->CastSpell(shirrak, SPELL_PING_SHIRRAK, true);

                m_uiPingShirrakTimer = 0;
            }
            else
                m_uiPingShirrakTimer -= uiDiff;
        }
    }
};

CreatureAI* GetAI_npc_focus_fire(Creature* pCreature)
{
    return new npc_focus_fireAI(pCreature);
}

void AddSC_boss_shirrak()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_shirrak";
    pNewScript->GetAI = &GetAI_boss_shirrak;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_focus_fire";
    pNewScript->GetAI = &GetAI_npc_focus_fire;
    pNewScript->RegisterSelf();
}
