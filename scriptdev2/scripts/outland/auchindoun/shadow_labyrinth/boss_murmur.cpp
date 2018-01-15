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
SDName: Boss_Murmur
SD%Complete: 100
SDComment:
SDCategory: Auchindoun, Shadow Labyrinth
EndScriptData */

#include "precompiled.h"
#include "shadow_labyrinth.h"

enum
{
    EMOTE_SONIC_BOOM = -1555036,
    SPELL_SONIC_BOOM_N = 33923,
    SPELL_SONIC_BOOM_H = 38796,

    SPELL_MAGNETIC_PULL = 37370,   // Wrong Spell most likely. (Core casts this)
    SPELL_MURMURS_TOUCH_N = 33711, // Changed to be a dummy aura
    SPELL_MURMURS_TOUCH_H = 38794,

    SPELL_RESONANCE = 33657,
    SPELL_SHOCKWAVE = 33686, // Core casts this

    // Heroic only spells:
    SPELL_SONIC_SHOCK_H = 38797,
    SPELL_THUNDERING_STORM_H = 39365, // We changed this to single target to
                                      // avoid a core mod; use the script to
                                      // trigger it at viable targets

    // RP Spells
    SPELL_RP_CABAL_SHADOW_BOLT = 33335,
    SPELL_RP_MURMURS_WRATH = 33331,
    SPELL_RP_SUPPRESSION_BLAST = 33332,

    NPC_CABAL_SUMMONER = 18634,
    NPC_CABAL_SPELLBINDER = 18639,

    SPELL_SELF_ROOT_FOREVER_NO_VISUAL =
        42716, // Use root to allow retargeting if main tank is out of range

    NPC_CHECK_GUID = 66890, // GUID of one of the spellbinders in the front, to
                            // know when to stop the pre-event

    // Summoners from each pack
    NPC_ROW_1_GUID = 66846,
    NPC_ROW_2_GUID = 66848,
    NPC_ROW_3_GUID = 66850
};

struct MANGOS_DLL_DECL boss_murmurAI : public ScriptedAI
{
    boss_murmurAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_pInstance = (ScriptedInstance*)pCreature->GetInstanceData();
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();

        // FIXME: Murmur will reset on each wrath shot, and all I've tried does
        // not work so we take the easy way out and not reset these timers
        m_uiRpBlastTimer = 12000;
        m_uiRpWrathTimer = 12000;
        m_uiRpSummonTimer = 4000;
        m_bPreEvent = true;

        Reset();
    }

    ScriptedInstance* m_pInstance;
    bool m_bIsRegularMode;
    bool m_bPreEvent;

    // Combat Timer
    uint32 m_uiSonicBoomTimer;
    uint32 m_uiMurmursTouchTimer;
    uint32 m_uiResonanceTimer;
    // Heroic Only:
    uint32 m_uiStormTimer;
    uint32 m_uiShockTimer;

    // RP Timers
    uint32 m_uiRpBlastTimer;
    uint32 m_uiRpWrathTimer;
    uint32 m_uiRpSummonTimer;
    std::vector<ObjectGuid> m_summons;

    void Reset() override
    {
        m_uiSonicBoomTimer = 25000;
        m_uiMurmursTouchTimer = urand(30000, 45000);
        m_uiResonanceTimer = 15000;
        m_uiStormTimer = urand(5000, 15000);
        m_uiShockTimer = urand(3000, 10000);

        m_creature->SetHealthPercent(40.0f);

        SetCombatMovement(false);
    }

    void JustSummoned(Creature* pCreature) override
    {
        m_summons.push_back(pCreature->GetObjectGuid());
    }

    void Aggro(Unit* /*pWho*/) override
    {
        if (m_pInstance)
            m_pInstance->SetData(TYPE_MURMUR, IN_PROGRESS);
    }

    void JustReachedHome() override
    {
        if (m_pInstance)
            m_pInstance->SetData(TYPE_MURMUR, FAIL);
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        if (m_pInstance)
            m_pInstance->SetData(TYPE_MURMUR, DONE);
    }

    void KillAllSummons()
    {
        for (auto& elem : m_summons)
            if (Creature* c = m_creature->GetMap()->GetCreature(elem))
                c->ForcedDespawn();
    }

    Unit* GetWrathTarget();
    void SummonRPMob();

    void UpdateAI(const uint32 uiDiff) override
    {
        // Return since we have no target
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
        {
            if (!m_bPreEvent)
                return;
            if (Creature* c =
                    m_creature->GetMap()->GetCreature(ObjectGuid(HIGHGUID_UNIT,
                        (uint32)NPC_CABAL_SPELLBINDER, (uint32)NPC_CHECK_GUID)))
            {
                if (!c->isAlive())
                {
                    KillAllSummons();
                    m_bPreEvent = false;
                    return;
                }
            }

            // RP STUFF
            if (m_uiRpBlastTimer <= uiDiff)
            {
                if (DoCastSpellIfCan(m_creature, SPELL_RP_SUPPRESSION_BLAST) ==
                    CAST_OK)
                    m_uiRpBlastTimer = 12000;
            }
            else
                m_uiRpBlastTimer -= uiDiff;

            if (m_uiRpWrathTimer <= uiDiff)
            {
                if (Unit* pTarget = GetWrathTarget())
                {
                    if (DoCastSpellIfCan(pTarget, SPELL_RP_MURMURS_WRATH) ==
                        CAST_OK)
                    {
                        if (pTarget->GetTypeId() == TYPEID_UNIT)
                            ((Creature*)pTarget)
                                ->ForcedDespawn(3000); // Despawn body

                        if (m_summons.empty())
                            m_uiRpWrathTimer = 12000;
                        else
                            m_uiRpWrathTimer = urand(5000, 6000);
                    }
                }
            }
            else
                m_uiRpWrathTimer -= uiDiff;

            if (m_uiRpSummonTimer <= uiDiff)
            {
                SummonRPMob();
                m_uiRpSummonTimer = 6000;
            }
            else
                m_uiRpSummonTimer -= uiDiff;
            // END OF RP STUFF

            return;
        }

        // COMBAT LOGIC:

        if (!m_creature->has_aura(SPELL_SELF_ROOT_FOREVER_NO_VISUAL))
            m_creature->AddAuraThroughNewHolder(
                SPELL_SELF_ROOT_FOREVER_NO_VISUAL, m_creature);

        if (m_uiSonicBoomTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, m_bIsRegularMode ?
                                                 SPELL_SONIC_BOOM_N :
                                                 SPELL_SONIC_BOOM_H) == CAST_OK)
            {
                DoScriptText(EMOTE_SONIC_BOOM, m_creature);
                m_uiSonicBoomTimer = urand(30000, 45000);
            }
        }
        else
            m_uiSonicBoomTimer -= uiDiff;

        if (m_uiMurmursTouchTimer <= uiDiff)
        {
            if (Unit* pTarget = m_creature->SelectAttackingTarget(
                    ATTACKING_TARGET_RANDOM, 0, SPELL_MURMURS_TOUCH_N))
                if (DoCastSpellIfCan(pTarget,
                        m_bIsRegularMode ? SPELL_MURMURS_TOUCH_N :
                                           SPELL_MURMURS_TOUCH_H) == CAST_OK)
                    m_uiMurmursTouchTimer = urand(30000, 45000);
        }
        else
            m_uiMurmursTouchTimer -= uiDiff;

        // Reset resonance timer as long as we can reach someone (the fact we're
        // rooted
        // means victim gets retargeted for us, if someone but the tank is
        // close)
        if (m_creature->CanReachWithMeleeAttack(m_creature->getVictim()))
            m_uiResonanceTimer = 15000;
        if (m_uiResonanceTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_RESONANCE) == CAST_OK)
                m_uiResonanceTimer = 15000;
        }
        else
            m_uiResonanceTimer -= uiDiff;

        if (!m_bIsRegularMode)
        {
            if (m_uiStormTimer <= uiDiff)
            {
                const ThreatList& tl =
                    m_creature->getThreatManager().getThreatList();
                for (const auto& elem : tl)
                {
                    Unit* tar =
                        m_creature->GetMap()->GetUnit((elem)->getUnitGuid());
                    if (tar && tar->isAlive())
                    {
                        float dist = tar->GetDistance(m_creature);
                        if (dist >= 25.0f && dist <= 100.0f)
                            DoCastSpellIfCan(
                                tar, SPELL_THUNDERING_STORM_H, CAST_TRIGGERED);
                    }
                }
                m_uiStormTimer = urand(5000, 15000);
            }
            else
                m_uiStormTimer -= uiDiff;

            if (m_uiShockTimer <= uiDiff)
            {
                if (DoCastSpellIfCan(m_creature->getVictim(),
                        SPELL_SONIC_SHOCK_H) == CAST_OK)
                    m_uiShockTimer = urand(3000, 10000);
            }
            else
                m_uiShockTimer -= uiDiff;
        }

        DoMeleeAttackIfReady();
    }
};

Unit* boss_murmurAI::GetWrathTarget()
{
    // Get any temporary summoner or spellbinder
    if (m_summons.empty())
        return NULL;
    uint32 index = urand(0, m_summons.size() - 1);
    Unit* tar = m_creature->GetMap()->GetUnit(m_summons[index]);
    if (tar && (CanCastSpell(tar, SPELL_RP_MURMURS_WRATH, true) == CAST_OK ||
                   !tar->isAlive()))
        m_summons.erase(m_summons.begin() + index);
    return tar;
}

void boss_murmurAI::SummonRPMob()
{
    // 100 in MovePoint is used to let SmartAI know this mob should fire fake
    // shadow bolts

    uint32 lowRand = 1;
    Creature* row_three = m_creature->GetMap()->GetCreature(ObjectGuid(
        HIGHGUID_UNIT, (uint32)NPC_CABAL_SUMMONER, (uint32)NPC_ROW_3_GUID));
    Creature* row_two = m_creature->GetMap()->GetCreature(ObjectGuid(
        HIGHGUID_UNIT, (uint32)NPC_CABAL_SUMMONER, (uint32)NPC_ROW_2_GUID));
    Creature* row_one = m_creature->GetMap()->GetCreature(ObjectGuid(
        HIGHGUID_UNIT, (uint32)NPC_CABAL_SUMMONER, (uint32)NPC_ROW_1_GUID));
    if (!row_three || row_three->isDead())
        return; // Do not summon
    else if (!row_two || row_two->isDead())
        lowRand = 5;
    else if (!row_one || row_one->isDead())
        lowRand = 3;

    switch (urand(lowRand, 6))
    {
    case 1:
        if (Creature* sum = m_creature->SummonCreature(
                urand(0, 1) ? NPC_CABAL_SPELLBINDER : NPC_CABAL_SUMMONER,
                -135.4f, -359.8f, 17.1f, 3.4f, TEMPSUMMON_DEAD_DESPAWN, 0))
        {
            sum->movement_gens.push(new movement::PointMovementGenerator(100,
                                        -150.7f, -366.5f, 17.1f, true, true),
                movement::EVENT_ENTER_COMBAT);
        }
        break;
    case 2:
        if (Creature* sum = m_creature->SummonCreature(
                urand(0, 1) ? NPC_CABAL_SPELLBINDER : NPC_CABAL_SUMMONER,
                -177.1f, -371.7f, 17.1f, 0.4f, TEMPSUMMON_DEAD_DESPAWN, 0))
        {
            sum->movement_gens.push(new movement::PointMovementGenerator(100,
                                        -163.0f, -363.9f, 17.1f, true, true),
                movement::EVENT_ENTER_COMBAT);
        }
        break;
    case 3:
        if (Creature* sum = m_creature->SummonCreature(
                urand(0, 1) ? NPC_CABAL_SPELLBINDER : NPC_CABAL_SUMMONER,
                -134.2f, -389.5f, 17.1f, 3.8f, TEMPSUMMON_DEAD_DESPAWN, 0))
        {
            sum->movement_gens.push(new movement::PointMovementGenerator(100,
                                        -150.7f, -396.8f, 17.1f, true, true),
                movement::EVENT_ENTER_COMBAT);
        }
        break;
    case 4:
        if (Creature* sum = m_creature->SummonCreature(
                urand(0, 1) ? NPC_CABAL_SPELLBINDER : NPC_CABAL_SUMMONER,
                -176.5f, -402.7f, 17.1f, 0.2f, TEMPSUMMON_DEAD_DESPAWN, 0))
        {
            sum->movement_gens.push(new movement::PointMovementGenerator(100,
                                        -162.4f, -395.5f, 17.1f, true, true),
                movement::EVENT_ENTER_COMBAT);
        }
        break;
    case 5:
        if (Creature* sum = m_creature->SummonCreature(
                urand(0, 1) ? NPC_CABAL_SPELLBINDER : NPC_CABAL_SUMMONER,
                -135.5f, -443.2f, 17.1f, 3.2f, TEMPSUMMON_DEAD_DESPAWN, 0))
        {
            sum->movement_gens.push(new movement::PointMovementGenerator(100,
                                        -150.9f, -439.6f, 17.1f, true, true),
                movement::EVENT_ENTER_COMBAT);
        }
        break;
    case 6:
        if (Creature* sum = m_creature->SummonCreature(
                urand(0, 1) ? NPC_CABAL_SPELLBINDER : NPC_CABAL_SUMMONER,
                -176.7f, -433.2f, 17.1f, 5.9f, TEMPSUMMON_DEAD_DESPAWN, 0))
        {
            sum->movement_gens.push(new movement::PointMovementGenerator(100,
                                        -162.4f, -439.7f, 17.1f, true, true),
                movement::EVENT_ENTER_COMBAT);
        }
        break;
    }
}

CreatureAI* GetAI_boss_murmur(Creature* pCreature)
{
    return new boss_murmurAI(pCreature);
}

enum
{
    // These damage spells have been remade to be AoE as well
    SONIC_BOOM_DMG_N = 36841,
    SONIC_BOOM_DMG_H = 38897,

    SONIC_BOOM_AURA_N = 33666,
    SONIC_BOOM_AURA_H = 38795,
};
bool EffectDummy_MurmurHandler(Unit* pCaster, uint32 spellId,
    SpellEffectIndex /*effIndex*/, Creature* /*pTarget*/)
{
    if (spellId == SPELL_SONIC_BOOM_N)
    {
        pCaster->CastSpell(pCaster, SONIC_BOOM_AURA_N, true);
        pCaster->CastSpell(pCaster, SONIC_BOOM_DMG_N, true);
    }
    else if (spellId == SPELL_SONIC_BOOM_H)
    {
        pCaster->CastSpell(pCaster, SONIC_BOOM_AURA_H, true);
        pCaster->CastSpell(pCaster, SONIC_BOOM_DMG_H, true);
    }

    return true;
}

void AddSC_boss_murmur()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_murmur";
    pNewScript->GetAI = &GetAI_boss_murmur;
    pNewScript->pEffectDummyNPC = &EffectDummy_MurmurHandler;
    pNewScript->RegisterSelf();
}
