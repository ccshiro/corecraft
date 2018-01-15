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
SDName: Boss_Mekgineer_Steamrigger
SD%Complete: 100
SDComment:
SDCategory: Coilfang Resevoir, The Steamvault
EndScriptData */

/* ContentData
boss_mekgineer_steamrigger
mob_steamrigger_mechanic
EndContentData */

#include "precompiled.h"
#include "steam_vault.h"

enum
{
    SAY_MECHANICS = -1545007,
    SAY_AGGRO_1 = -1545008,
    SAY_AGGRO_2 = -1545009,
    SAY_AGGRO_3 = -1545010,
    SAY_AGGRO_4 = -1545011,
    SAY_KILL_1 = -1545012,
    SAY_KILL_2 = -1545013,
    SAY_KILL_3 = -1545014,
    SAY_DEATH = -1545015,

    SPELL_SUPER_SHRINK_RAY = 31485,
    SPELL_SAW_BLADE = 31486,
    SPELL_ELECTRIFIED_NET = 35107,
    SPELL_BERSERK = 26662,

    NPC_STEAMRIGGER_MECHANIC = 17951,

    // Mechanic spells
    SPELL_REPAIR_N = 31532,
    SPELL_REPAIR_H = 37936,
};

// Mekgineer, need class declaration available in boss script
struct MANGOS_DLL_DECL mob_steamrigger_mechanicAI : public Scripted_BehavioralAI
{
    mob_steamrigger_mechanicAI(Creature* pCreature)
      : Scripted_BehavioralAI(pCreature)
    {
        m_pInstance = (ScriptedInstance*)pCreature->GetInstanceData();
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();
        Reset();
    }

    ScriptedInstance* m_pInstance;
    bool m_bIsRegularMode;
    uint32 m_uiRepairTimer;

    void Reset() override;
    void DamageTaken(Unit* pDealer, uint32& uiDamage) override;
    void WasJustSummoned(Creature* mekgineer);

    void UpdateAI(const uint32 uiDiff) override;
};

struct SummonLocation
{
    float m_fX, m_fY, m_fZ;
};

// Spawn locations
static const SummonLocation aSteamriggerSpawnLocs[] = {
    {-316.101f, -166.444f, -7.66f}, {-348.497f, -161.718f, -7.66f},
    {-331.161f, -112.212f, -7.66f},
};

struct MANGOS_DLL_DECL boss_mekgineer_steamriggerAI : public ScriptedAI
{
    boss_mekgineer_steamriggerAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_pInstance = (ScriptedInstance*)pCreature->GetInstanceData();
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();
        Reset();
    }

    ScriptedInstance* m_pInstance;
    bool m_bIsRegularMode;

    uint32 m_uiShrinkTimer;
    uint32 m_uiSawBladeTimer;
    uint32 m_uiElectrifiedNetTimer;
    float m_nextMechanicPct;
    uint32 m_uiMechanicTimer;
    uint32 m_uiBerserkTimer;

    void Reset() override
    {
        m_uiShrinkTimer = 25000;
        m_uiSawBladeTimer = urand(6000, 12000);
        m_uiElectrifiedNetTimer = 10000;
        m_nextMechanicPct = 75.0f;
        m_uiMechanicTimer = 15000;
        m_uiBerserkTimer = 5 * 60 * 1000;
    }

    void JustReachedHome() override
    {
        if (m_pInstance)
            m_pInstance->SetData(TYPE_MEKGINEER_STEAMRIGGER, FAIL);
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        DoScriptText(SAY_DEATH, m_creature);

        if (m_pInstance)
            m_pInstance->SetData(TYPE_MEKGINEER_STEAMRIGGER, DONE);
    }

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_KILL_1, SAY_KILL_2, SAY_KILL_3);
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

        if (m_pInstance)
            m_pInstance->SetData(TYPE_MEKGINEER_STEAMRIGGER, IN_PROGRESS);
    }

    // Wrapper to summon three Mechanics
    void SummonMechanichs()
    {
        DoScriptText(SAY_MECHANICS, m_creature);

        for (auto& aSteamriggerSpawnLoc : aSteamriggerSpawnLocs)
            m_creature->SummonCreature(NPC_STEAMRIGGER_MECHANIC,
                aSteamriggerSpawnLoc.m_fX, aSteamriggerSpawnLoc.m_fY,
                aSteamriggerSpawnLoc.m_fZ, 0,
                TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 12000);
    }

    void JustSummoned(Creature* summon) override
    {
        if (mob_steamrigger_mechanicAI* AI =
                dynamic_cast<mob_steamrigger_mechanicAI*>(summon->AI()))
            AI->WasJustSummoned(m_creature);
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (m_uiShrinkTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(
                    m_creature->getVictim(), SPELL_SUPER_SHRINK_RAY) == CAST_OK)
                m_uiShrinkTimer = urand(25000, 40000);
        }
        else
            m_uiShrinkTimer -= uiDiff;

        if (m_uiSawBladeTimer <= uiDiff)
        {
            Unit* pTarget =
                m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 1);
            if (!pTarget)
                pTarget = m_creature->getVictim();

            if (pTarget)
            {
                if (DoCastSpellIfCan(pTarget, SPELL_SAW_BLADE) == CAST_OK)
                    m_uiSawBladeTimer = m_bIsRegularMode ? urand(10000, 15000) :
                                                           urand(6000, 12000);
            }
        }
        else
            m_uiSawBladeTimer -= uiDiff;

        if (m_uiElectrifiedNetTimer <= uiDiff)
        {
            if (Unit* pTarget = m_creature->SelectAttackingTarget(
                    ATTACKING_TARGET_RANDOM, 0))
            {
                if (DoCastSpellIfCan(pTarget, SPELL_ELECTRIFIED_NET) == CAST_OK)
                    m_uiElectrifiedNetTimer = urand(10000, 25000);
            }
        }
        else
            m_uiElectrifiedNetTimer -= uiDiff;

        // On Heroic mode summon a mechanic each 20 secs
        if (!m_bIsRegularMode)
        {
            if (m_uiMechanicTimer <= uiDiff)
            {
                m_creature->SummonCreature(NPC_STEAMRIGGER_MECHANIC,
                    aSteamriggerSpawnLocs[2].m_fX,
                    aSteamriggerSpawnLocs[2].m_fY,
                    aSteamriggerSpawnLocs[2].m_fZ, 0,
                    TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 12000);
                m_uiMechanicTimer = 20000;
            }
            else
                m_uiMechanicTimer -= uiDiff;
        }

        if (m_creature->GetHealthPercent() <= m_nextMechanicPct &&
            m_nextMechanicPct != 0.0f)
        {
            SummonMechanichs();
            m_nextMechanicPct -= 25.0f;
        }

        if (m_uiBerserkTimer)
        {
            if (m_uiBerserkTimer <= uiDiff)
            {
                if (DoCastSpellIfCan(m_creature, SPELL_BERSERK) == CAST_OK)
                    m_uiBerserkTimer = 0;
            }
            else
                m_uiBerserkTimer -= uiDiff;
        }

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_mekgineer_steamrigger(Creature* pCreature)
{
    return new boss_mekgineer_steamriggerAI(pCreature);
}

void mob_steamrigger_mechanicAI::Reset()
{
    m_uiRepairTimer = 0;
    Scripted_BehavioralAI::Reset();
}

void mob_steamrigger_mechanicAI::DamageTaken(
    Unit* /*pDealer*/, uint32& uiDamage)
{
    if (uiDamage)
    {
        if (m_creature->IsNonMeleeSpellCasted(false))
            m_uiRepairTimer = urand(6000, 8000);
        m_creature->InterruptNonMeleeSpells(false);
        m_creature->movement_gens.remove_all(movement::gen::stopped);
    }
}

void mob_steamrigger_mechanicAI::WasJustSummoned(Creature* mekgineer)
{
    if (auto victim = mekgineer->getVictim())
        AttackStart(victim);
    m_uiRepairTimer = 2000;
}

void mob_steamrigger_mechanicAI::UpdateAI(const uint32 uiDiff)
{
    // Attempt repair rather frequently
    if (m_uiRepairTimer)
    {
        if (m_uiRepairTimer <= uiDiff)
        {
            if (m_pInstance)
                if (Creature* mekgineer =
                        m_pInstance->GetSingleCreatureFromStorage(
                            NPC_STEAMRIGGER))
                {
                    if (CanCastSpell(mekgineer,
                            m_bIsRegularMode ? SPELL_REPAIR_N : SPELL_REPAIR_H,
                            false) == CAST_OK)
                    {
                        DoCastSpellIfCan(mekgineer,
                            m_bIsRegularMode ? SPELL_REPAIR_N : SPELL_REPAIR_H);
                    }
                }
            m_uiRepairTimer = 12000;
        }
        else
            m_uiRepairTimer -= uiDiff;
    }

    if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
        return;

    Scripted_BehavioralAI::UpdateInCombatAI(uiDiff);

    DoMeleeAttackIfReady();
}

CreatureAI* GetAI_mob_steamrigger_mechanic(Creature* pCreature)
{
    return new mob_steamrigger_mechanicAI(pCreature);
}

void AddSC_boss_mekgineer_steamrigger()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_mekgineer_steamrigger";
    pNewScript->GetAI = &GetAI_boss_mekgineer_steamrigger;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "mob_steamrigger_mechanic";
    pNewScript->GetAI = &GetAI_mob_steamrigger_mechanic;
    pNewScript->RegisterSelf();
}
