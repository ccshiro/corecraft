/* Copyright (C) Corecraft
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
SDName: boss_swamplord_muselek
SD%Complete: 100
SDComment:
SDCategory: Coilfang Resevoir, Underbog
EndScriptData */

#include "precompiled.h"
#include "underbog.h"

enum
{
    SAY_AGGRO_1 = -1100101,
    SAY_AGGRO_2 = -1100102,
    SAY_AGGRO_3 = -1100103,
    SAY_KILL_1 = -1100104,
    SAY_KILL_2 = -1100105,
    SAY_DEATH = -1100106,
    SAY_BEAST = -1100107,

    SPELL_AIMED_SHOT = 31623,
    SPELL_BEAR_COMMAND = 34662,
    SPELL_DETERRENCE = 31567,
    SPELL_FREEZING_TRAP_EFFECT = 31932,
    SPELL_HUNTERS_MARK = 31615,
    SPELL_THROW_FREEZING_TRAP = 31946,

    // Handled by behavioral AI:
    SPELL_KNOCK_AWAY = 18813,
    SPELL_MULTISHOT = 34974,
    SPELL_RAPTOR_STRIKE = 31566,
    SPELL_SHOOT = 22907,

    SPELL_NOTIFY_OF_DEATH = 31547, // ??

    SPELL_FERAL_CHARGE = 39435,
    SPELL_ECHOING_ROAR = 31429,
    SPELL_FRENZY = 34971,
    SPELL_MAUL = 34298
};

struct MANGOS_DLL_DECL boss_swamplord_muselekAI : public Scripted_BehavioralAI
{
    boss_swamplord_muselekAI(Creature* pCreature)
      : Scripted_BehavioralAI(pCreature)
    {
        m_pInstance = (instance_underbog*)pCreature->GetInstanceData();

        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();
        Reset();
    }

    instance_underbog* m_pInstance;

    bool m_bIsRegularMode;
    ObjectGuid m_aimedShotVictim;

    uint32 m_uiFreezingTrapTimer;
    uint32 m_uiHuntersMarkTimer;
    uint32 m_uiAimedShotTimer;

    uint32 m_uiBearCommandTimer;
    uint32 m_uiDeterrenceTimer;

    void EvadeHome() { ScriptedAI::EnterEvadeMode(); }

    void EnterEvadeMode(bool by_group = false) override;

    void Reset() override
    {
        Scripted_BehavioralAI::Reset();

        m_aimedShotVictim = ObjectGuid();

        m_uiFreezingTrapTimer = 25000;
        m_uiHuntersMarkTimer = 0;
        m_uiAimedShotTimer = 0;

        m_uiBearCommandTimer = 7000;
        m_uiDeterrenceTimer = 30000;
    }

    void Aggro(Unit* pWho) override
    {
        if (m_pInstance)
        {
            if (Creature* pClaw =
                    m_pInstance->GetSingleCreatureFromStorage(NPC_CLAW))
                if (pClaw->AI())
                    pClaw->AI()->AttackStart(pWho);
        }

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
        DoKillSay(m_creature, victim, SAY_KILL_1, SAY_KILL_2);
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        DoScriptText(SAY_DEATH, m_creature);
    }

    void JustReachedHome() override
    {
        if (m_pInstance)
        {
            if (Creature* pClaw =
                    m_pInstance->GetSingleCreatureFromStorage(NPC_CLAW))
            {
                pClaw->UpdateEntry(NPC_CLAW, ALLIANCE);
                pClaw->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
                pClaw->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PASSIVE);
            }
        }
    }

    void DamageTaken(Unit* /*pDoneBy*/, uint32& /*ui*/) override
    {
        if (!m_uiDeterrenceTimer)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_DETERRENCE) == CAST_OK)
                m_uiDeterrenceTimer = 30000;
        }
    }

    Unit* PickAimedShotVictim(bool forHuntersMark)
    {
        if (forHuntersMark)
            m_aimedShotVictim = ObjectGuid();

        if (m_aimedShotVictim)
            if (Unit* pTarget =
                    m_creature->GetMap()->GetUnit(m_aimedShotVictim))
                if (pTarget->isAlive())
                    return pTarget;

        std::vector<Unit*> targets;
        const ThreatList& tl = m_creature->getThreatManager().getThreatList();
        for (const auto& elem : tl)
        {
            Unit* target = (elem)->getTarget();
            if (CanCastSpell(target, SPELL_AIMED_SHOT, false) != CAST_OK)
                continue;
            if (target->has_aura(SPELL_FREEZING_TRAP_EFFECT))
                targets.push_back(target);
        }

        if (targets.empty())
            return NULL;
        Unit* target = targets[urand(0, targets.size() - 1)];
        m_aimedShotVictim = target->GetObjectGuid();
        return target;
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        Scripted_BehavioralAI::UpdateInCombatAI(uiDiff);

        // Freezing Trap
        if (m_uiFreezingTrapTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_THROW_FREEZING_TRAP) ==
                CAST_OK)
            {
                m_uiHuntersMarkTimer = 1600;
                m_uiAimedShotTimer = 3200;
                m_uiFreezingTrapTimer = 40000;
            }
        }
        else
            m_uiFreezingTrapTimer -= uiDiff;

        if (m_uiHuntersMarkTimer)
        {
            if (m_uiHuntersMarkTimer <= uiDiff)
            {
                if (Unit* pTarget = PickAimedShotVictim(true))
                {
                    if (DoCastSpellIfCan(pTarget, SPELL_HUNTERS_MARK) ==
                        CAST_OK)
                        m_uiHuntersMarkTimer = 0;
                }
                else
                    m_uiHuntersMarkTimer = 0;
            }
            else
                m_uiHuntersMarkTimer -= uiDiff;
        }

        if (m_uiAimedShotTimer)
        {
            if (m_uiAimedShotTimer <= uiDiff)
            {
                if (m_creature->movement_gens.has(movement::gen::point))
                {
                    if (Unit* pTarget = PickAimedShotVictim(false))
                    {
                        if (DoCastSpellIfCan(pTarget, SPELL_AIMED_SHOT) ==
                            CAST_OK)
                            m_uiAimedShotTimer = 0;
                    }
                    else
                        m_uiAimedShotTimer = 0;
                }
            }
            else
                m_uiAimedShotTimer -= uiDiff;
        }

        // Bear Command
        if (m_uiBearCommandTimer <= uiDiff)
        {
            if (m_pInstance)
            {
                if (Creature* pClaw =
                        m_pInstance->GetSingleCreatureFromStorage(NPC_CLAW))
                {
                    if (pClaw->GetEntry() == NPC_CLAW && pClaw->isAlive())
                    {
                        DoScriptText(SAY_BEAST, m_creature);
                        pClaw->CastSpell(pClaw, SPELL_FRENZY, true);
                    }
                }

                m_uiBearCommandTimer = 30000;
            }
        }
        else
            m_uiBearCommandTimer -= uiDiff;

        // Update Deterrence timer
        if (m_uiDeterrenceTimer <= uiDiff)
            m_uiDeterrenceTimer = 0;
        else
            m_uiDeterrenceTimer -= uiDiff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_swamplord_muselek(Creature* pCreature)
{
    return new boss_swamplord_muselekAI(pCreature);
}

struct MANGOS_DLL_DECL boss_clawAI : public ScriptedAI
{
    boss_clawAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_pInstance = (instance_underbog*)pCreature->GetInstanceData();
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();
        Reset();
    }

    instance_underbog* m_pInstance;
    bool m_bIsRegularMode;
    uint32 m_uiFeralChargeTimer;
    uint32 m_uiMaulTimer;
    uint32 m_uiEchoingRoarTimer;

    void Aggro(Unit* pWho) override
    {
        if (m_pInstance)
        {
            if (auto muselek = m_pInstance->GetSingleCreatureFromStorage(
                    NPC_SWAMPLORD_MUSELEK))
                if (muselek->AI())
                    muselek->AI()->AttackStart(pWho);
        }
    }

    void EvadeHome() { ScriptedAI::EnterEvadeMode(); }

    void EnterEvadeMode(bool by_group = false) override
    {
        if (m_pInstance)
            if (auto muselek = m_pInstance->GetSingleCreatureFromStorage(
                    NPC_SWAMPLORD_MUSELEK))
                if (auto ai =
                        dynamic_cast<boss_swamplord_muselekAI*>(muselek->AI()))
                    ai->EvadeHome();
        ScriptedAI::EnterEvadeMode(by_group);
    }

    void MoveInLineOfSight(Unit* pWho) override
    {
        if (m_creature->GetEntry() == NPC_WINDCALLER_CLAW)
            return;

        ScriptedAI::MoveInLineOfSight(pWho);
    }

    void BecomeClaw()
    {
        m_creature->UpdateEntry(NPC_WINDCALLER_CLAW,
            ALLIANCE); // For some reason it should be ALLIANCE as team model id
        m_creature->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
        m_creature->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PASSIVE);
    }

    void Reset() override
    {
        m_uiMaulTimer = 10000;
        m_uiEchoingRoarTimer = 6000;
        m_uiFeralChargeTimer = 1000;
    }

    void JustReachedHome() override
    {
        // Sit if we're in druid form
        if (m_creature->GetEntry() == NPC_WINDCALLER_CLAW)
            m_creature->SetStandState(UNIT_STAND_STATE_SIT);
        else
        {
            if (m_pInstance)
                if (Creature* muselek =
                        m_pInstance->GetSingleCreatureFromStorage(
                            NPC_SWAMPLORD_MUSELEK))
                {
                    if (!muselek->isAlive())
                    {
                        BecomeClaw();
                        m_creature->SetStandState(UNIT_STAND_STATE_SIT);
                    }
                }
        }
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        // If below 20% hp, turn into windcaller claw
        if (m_creature->GetHealthPercent() <= 20)
        {
            // Become Windcaller Claw
            BecomeClaw();

            // Exit combat (call base class, our evade has special logic in it)
            ScriptedAI::EnterEvadeMode();

            return;
        }

        if (m_creature->has_aura(SPELL_FRENZY))
        {
            if (m_uiFeralChargeTimer <= uiDiff)
            {
                if (Unit* pTarget = m_creature->SelectAttackingTarget(
                        ATTACKING_TARGET_RANDOM, 1, SPELL_FRENZY))
                    if (DoCastSpellIfCan(pTarget, SPELL_FERAL_CHARGE) ==
                        CAST_OK)
                        m_uiFeralChargeTimer = urand(2000, 4000);
            }
            else
                m_uiFeralChargeTimer -= uiDiff;
        }

        // Echoing Roar
        if (m_uiEchoingRoarTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_ECHOING_ROAR) == CAST_OK)
                m_uiEchoingRoarTimer = urand(10000, 20000);
        }
        else
            m_uiEchoingRoarTimer -= uiDiff;

        // Maul
        if (m_uiMaulTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_MAUL) ==
                CAST_OK)
                m_uiMaulTimer = urand(10000, 20000);
        }
        else
            m_uiMaulTimer -= uiDiff;

        DoMeleeAttackIfReady();
    }
};

void boss_swamplord_muselekAI::EnterEvadeMode(bool by_group)
{
    if (m_pInstance)
        if (auto claw = m_pInstance->GetSingleCreatureFromStorage(NPC_CLAW))
            if (auto ai = dynamic_cast<boss_clawAI*>(claw->AI()))
                ai->EvadeHome();
    Scripted_BehavioralAI::EnterEvadeMode(by_group);
}

CreatureAI* GetAI_boss_claw(Creature* pCreature)
{
    return new boss_clawAI(pCreature);
}

void AddSC_boss_swamplord_muselek()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_swamplord_muselek";
    pNewScript->GetAI = &GetAI_boss_swamplord_muselek;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "boss_claw";
    pNewScript->GetAI = &GetAI_boss_claw;
    pNewScript->RegisterSelf();
}
