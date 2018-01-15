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
SDName: Boss_Mechano_Lord_Capacitus
SD%Complete: 100
SDComment:
SDCategory: Tempest Keep, The Mechanar
EndScriptData */

#include "precompiled.h"

enum
{
    SAY_AGGRO = -1000901,
    SAY_KILL_1 = -1000902,
    SAY_KILL_2 = -1000903,
    SAY_DAMAGE_SHIELD = -1000904,
    SAY_MAGIC_SHIELD = -1000905,
    SAY_DEATH = -1000906,

    SPELL_HEAD_CRACK = 35161, // Used each 19 to 25 seconds
    SPELL_MAGIC_SHIELD =
        35158, // Use random shield each (30 secs? needs testing)
    SPELL_DAMAGE_SHIELD = 35159,
    SPELL_POLARITY_SHIFT = 39096, // Heroic only, used together with a yell?
                                  // (check which). Timer said 30 intial, then
                                  // 45-60, check them
    SPELL_POSITIVE_CHARGE = 39088,
    SPELL_NEGATIVE_CHARGE = 39091,
    SPELL_POSITIVE_CHARGE_DMG = 39089,
    SPELL_NEGATIVE_CHARGE_DMG = 39092,

    SPELL_BERSERK = 26662, // Enrage timer after 3 minutes (heroic only)

    NPC_NETHER_CHARGE =
        20405, // First time: after 10 secs, then each 2 to 4 seconds
    SPELL_NETHER_CHARGE_TIMER = 37670,
    SPELL_NETHER_PULSE = 35151,
    SPELL_NETHER_DETONATION = 35152,
};

struct MANGOS_DLL_DECL boss_mechano_lord_capacitusAI : public ScriptedAI
{
    boss_mechano_lord_capacitusAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();
        m_pInstance = (ScriptedInstance*)pCreature->GetMap()->GetInstanceData();
        Reset();
    }

    ScriptedInstance* m_pInstance;

    bool m_bIsRegularMode;
    uint32 m_uiHeadCrackTimer;
    uint32 m_uiProtectiveShieldTimer;
    uint32 m_uiPolarityShiftTimer;
    uint32 m_uiEnrageTimer;
    uint32 m_uiSpawnNetherChargeTimer;
    uint32 m_uiPolarityShiftCastTimer;
    uint32 m_uiUpdatePolarDamageBuffTimer;

    void Reset() override
    {
        m_uiHeadCrackTimer = 6000;
        m_uiPolarityShiftTimer = 12000;
        m_uiProtectiveShieldTimer = 20000;
        m_uiEnrageTimer = 3 * 60 * 1000;
        m_uiSpawnNetherChargeTimer = 9000;
        m_uiPolarityShiftCastTimer = 0;
        m_uiUpdatePolarDamageBuffTimer = 0;

        std::list<Player*> players = m_pInstance->GetAllPlayersInMap();
        if (players.size() > 0)
        {
            for (auto pPlayer : players)
            {
                pPlayer->remove_auras(SPELL_POSITIVE_CHARGE);
                pPlayer->remove_auras(SPELL_POSITIVE_CHARGE_DMG);
                pPlayer->remove_auras(SPELL_NEGATIVE_CHARGE);
                pPlayer->remove_auras(SPELL_NEGATIVE_CHARGE_DMG);
            }
        }
    }

    void Aggro(Unit* /*who*/) override { DoScriptText(SAY_AGGRO, m_creature); }

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_KILL_1, SAY_KILL_2);
    }

    void JustDied(Unit* /*Killer*/) override
    {
        DoScriptText(SAY_DEATH, m_creature);
        std::list<Player*> players = m_pInstance->GetAllPlayersInMap();
        if (players.size() > 0)
        {
            for (auto pPlayer : players)
            {
                pPlayer->remove_auras(SPELL_POSITIVE_CHARGE);
                pPlayer->remove_auras(SPELL_POSITIVE_CHARGE_DMG);
                pPlayer->remove_auras(SPELL_NEGATIVE_CHARGE);
                pPlayer->remove_auras(SPELL_NEGATIVE_CHARGE_DMG);
            }
        }
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        // Server Side scripting for Positive and Negative charge, applies
        // +damage buff
        if (!m_bIsRegularMode)
        {
            if (m_uiUpdatePolarDamageBuffTimer)
            {
                if (m_uiUpdatePolarDamageBuffTimer <= uiDiff)
                {
                    std::list<Player*> players =
                        m_pInstance->GetAllPlayersInMap(true, false);
                    if (players.size() > 0)
                    {
                        for (std::list<Player*>::iterator itr = players.begin();
                             itr != players.end(); ++itr)
                        {
                            Player* pPlayer = *itr;
                            uint32 auraId;
                            if (pPlayer->has_aura(SPELL_POSITIVE_CHARGE))
                                auraId = SPELL_POSITIVE_CHARGE;
                            else if (pPlayer->has_aura(SPELL_NEGATIVE_CHARGE))
                                auraId = SPELL_NEGATIVE_CHARGE;
                            else
                                continue; // Skip this player

                            uint32 damageStacks = 0;
                            for (auto pInnerPlayer : players)
                            {
                                if (pInnerPlayer == pPlayer)
                                    continue;

                                if (pInnerPlayer->has_aura(auraId) &&
                                    pPlayer->GetDistance2d(pInnerPlayer) <= 10)
                                    damageStacks += 1;
                            }

                            if (damageStacks > 0)
                            {
                                uint32 stackid =
                                    auraId == SPELL_POSITIVE_CHARGE ?
                                        SPELL_POSITIVE_CHARGE_DMG :
                                        SPELL_NEGATIVE_CHARGE_DMG;
                                AuraHolder* prevHolder =
                                    pPlayer->get_aura(stackid);
                                if (prevHolder)
                                {
                                    if (prevHolder->GetStackAmount() >
                                        damageStacks)
                                        pPlayer->remove_auras(stackid);
                                    else
                                        damageStacks -=
                                            prevHolder->GetStackAmount();
                                }

                                if (damageStacks >
                                    0) // Still needed to be applied?
                                {
                                    for (uint8 i = 0; i < damageStacks; ++i)
                                    {
                                        pPlayer->AddAuraThroughNewHolder(
                                            stackid, m_creature);
                                    }
                                }
                            }
                            else
                            {
                                // Make sure we don't have a stack when we
                                // shouldn't
                                pPlayer->remove_auras(
                                    SPELL_POSITIVE_CHARGE_DMG);
                                pPlayer->remove_auras(
                                    SPELL_NEGATIVE_CHARGE_DMG);
                            }
                        }
                    }

                    m_uiUpdatePolarDamageBuffTimer = 2500;
                }
                else
                    m_uiUpdatePolarDamageBuffTimer -= uiDiff;
            }
        }

        // Enrage (heroic only)
        if (!m_bIsRegularMode)
        {
            if (m_uiEnrageTimer)
            {
                if (m_uiEnrageTimer <= uiDiff)
                {
                    if (DoCastSpellIfCan(m_creature, SPELL_BERSERK) == CAST_OK)
                        m_uiEnrageTimer = 0;
                }
                else
                    m_uiEnrageTimer -= uiDiff;
            }
        }

        // Head Crack
        if (m_uiHeadCrackTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_HEAD_CRACK) ==
                CAST_OK)
                m_uiHeadCrackTimer = urand(19000, 25000);
        }
        else
            m_uiHeadCrackTimer -= uiDiff;

        // Protective Shield (normal mode only)
        if (m_bIsRegularMode)
        {
            if (m_uiProtectiveShieldTimer <= uiDiff)
            {
                uint32 shieldType = urand(0, 1);
                if (shieldType == 0)
                {
                    if (DoCastSpellIfCan(m_creature, SPELL_MAGIC_SHIELD) ==
                        CAST_OK)
                    {
                        DoScriptText(SAY_MAGIC_SHIELD, m_creature);
                        m_uiProtectiveShieldTimer = 30000;
                    }
                }
                else
                {
                    if (DoCastSpellIfCan(m_creature, SPELL_DAMAGE_SHIELD) ==
                        CAST_OK)
                    {
                        DoScriptText(SAY_DAMAGE_SHIELD, m_creature);
                        m_uiProtectiveShieldTimer = 30000;
                    }
                }
            }
            else
                m_uiProtectiveShieldTimer -= uiDiff;
        }

        // Polarity Shift (heroic only)
        if (!m_bIsRegularMode)
        {
            if (m_uiPolarityShiftTimer <= uiDiff)
            {
                m_uiUpdatePolarDamageBuffTimer =
                    999999; // Stop updating until buffs applied
                if (DoCastSpellIfCan(m_creature, SPELL_POLARITY_SHIFT) ==
                    CAST_OK)
                {
                    m_uiPolarityShiftCastTimer = 3100;
                    m_uiPolarityShiftTimer = urand(30000, 45000);
                }
            }
            else
                m_uiPolarityShiftTimer -= uiDiff;
        }

        // Polarity shift buff
        if (m_uiPolarityShiftCastTimer)
        {
            if (m_uiPolarityShiftCastTimer <= uiDiff)
            {
                // Remove previous charge, and add new charge
                std::list<Player*> players =
                    m_pInstance->GetAllPlayersInMap(true, false);
                if (players.size() > 0)
                {
                    // Need a RandomAccessIterator ready containter for shuffle
                    std::vector<Player*> vecPlayers =
                        std::vector<Player*>(players.begin(), players.end());
                    std::random_shuffle(vecPlayers.begin(), vecPlayers.end());
                    uint32 last_charge = urand(1, 100); // First needs to be
                                                        // random; we can either
                                                        // get 3-2 in favour of
                                                        // positive or negative
                    for (auto pPlayer : vecPlayers)
                    {
                        pPlayer->remove_auras(SPELL_POSITIVE_CHARGE);
                        pPlayer->remove_auras(SPELL_POSITIVE_CHARGE_DMG);
                        pPlayer->remove_auras(SPELL_NEGATIVE_CHARGE);
                        pPlayer->remove_auras(SPELL_NEGATIVE_CHARGE_DMG);

                        if (last_charge <= 50)
                        {
                            pPlayer->AddAuraThroughNewHolder(
                                SPELL_POSITIVE_CHARGE, m_creature);
                            last_charge = 100;
                        }
                        else
                        {
                            pPlayer->AddAuraThroughNewHolder(
                                SPELL_NEGATIVE_CHARGE, m_creature);
                            last_charge = 1;
                        }
                    }

                    m_uiUpdatePolarDamageBuffTimer = 500;
                }

                m_uiPolarityShiftCastTimer = 0;
            }
            else
                m_uiPolarityShiftCastTimer -= uiDiff;
        }

        // Nether Charge
        if (m_uiSpawnNetherChargeTimer <= uiDiff)
        {
            m_creature->SummonCreature(NPC_NETHER_CHARGE, m_creature->GetX(),
                m_creature->GetY(), m_creature->GetZ(), 0.0f,
                TEMPSUMMON_DEAD_DESPAWN, 0);
            m_uiSpawnNetherChargeTimer = urand(2000, 4000);
        }
        else
            m_uiSpawnNetherChargeTimer -= uiDiff;

        DoMeleeAttackIfReady();
    }
};
CreatureAI* GetAI_boss_mechano_lord_capacitus(Creature* pCreature)
{
    return new boss_mechano_lord_capacitusAI(pCreature);
}

struct MANGOS_DLL_DECL boss_add_nether_chargeAI : public ScriptedAI
{
    boss_add_nether_chargeAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();
        Reset();
    }

    bool m_bIsRegularMode;
    uint32 m_uiStopMovementTimer;
    uint32 m_uiDespawnTimer;
    uint32 m_uiDetonationTimer;

    uint8 m_uiPulseNr;

    void Reset() override
    {
        m_creature->movement_gens.push(
            new movement::RandomMovementGenerator(50.0f));
        m_uiStopMovementTimer = 14000;
        m_uiPulseNr = 0;
        m_uiDespawnTimer = 0;
        m_uiDetonationTimer = 0;
    }

    // Disable all forms of combat
    void MoveInLineOfSight(Unit* /*pWho*/) override {}
    void Aggro(Unit* /*pWho*/) override {}
    void AttackStart(Unit* /*pWho*/) override {}
    void EnterCombat(Unit* /*pEnemy*/) override {}
    void AttackedBy(Unit* /*pAttacker*/) override {}

    void UpdateAI(const uint32 uiDiff) override
    {
        if (m_uiStopMovementTimer)
        {
            if (m_uiStopMovementTimer <= uiDiff)
            {
                m_creature->movement_gens.push(
                    new movement::StoppedMovementGenerator());
                m_uiStopMovementTimer = 0;
                m_uiDetonationTimer = 500;
            }
            else
                m_uiStopMovementTimer -= uiDiff;
        }

        if (m_uiDespawnTimer)
        {
            if (m_uiDespawnTimer <= uiDiff)
            {
                m_creature->ForcedDespawn();
                m_uiDespawnTimer = 0;
            }
            else
                m_uiDespawnTimer -= uiDiff;
        }

        if (m_uiDetonationTimer)
        {
            if (m_uiDetonationTimer <= uiDiff)
            {
                if (m_uiPulseNr < 4)
                {
                    if (DoCastSpellIfCan(m_creature, SPELL_NETHER_PULSE,
                            CAST_TRIGGERED) == CAST_OK)
                    {
                        m_uiDetonationTimer = 500;
                        ++m_uiPulseNr;
                    }
                    return;
                }

                if (DoCastSpellIfCan(m_creature, SPELL_NETHER_DETONATION) ==
                    CAST_OK)
                {
                    m_uiDetonationTimer = 0;
                    m_uiDespawnTimer = 1500;
                }
            }
            else
                m_uiDetonationTimer -= uiDiff;
        }
    }
};
CreatureAI* GetAI_boss_add_nether_charge(Creature* pCreature)
{
    return new boss_add_nether_chargeAI(pCreature);
}

void AddSC_boss_mechano_lord_capacitus()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_mechano_lord_capacitus";
    pNewScript->GetAI = &GetAI_boss_mechano_lord_capacitus;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "boss_add_nether_charge";
    pNewScript->GetAI = &GetAI_boss_add_nether_charge;
    pNewScript->RegisterSelf();
}
