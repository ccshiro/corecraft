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
SDName: Wizard of Oz
SD%Complete: 100
SDComment:
SDCategory: Karazhan
EndScriptData */

#include "karazhan.h"
#include "precompiled.h"

enum
{
    SAY_DOROTHEE_DEATH = -1532025,
    SAY_DOROTHEE_SUMMON = -1532026,
    // SAY_DOROTHEE_TITO_DEATH     = -1532027, // Handled in instance's
    // OnCreatureDeath
    SAY_DOROTHEE_AGGRO = -1532028,

    SAY_ROAR_AGGRO = -1532029,
    SAY_ROAR_DEATH = -1532030,
    SAY_ROAR_KILL = -1532031,

    SAY_STRAWMAN_AGGRO = -1532032,
    SAY_STRAWMAN_DEATH = -1532033,
    SAY_STRAWMAN_KILL = -1532034,

    SAY_TINHEAD_AGGRO = -1532035,
    SAY_TINHEAD_DEATH = -1532036,
    SAY_TINHEAD_KILL = -1532037,
    EMOTE_RUST = -1532038,

    SAY_CRONE_AGGRO_1 = -1532039,
    SAY_CRONE_AGGRO_2 = -1532040,
    SAY_CRONE_DEATH = -1532041,
    SAY_CRONE_KILL = -1532042,

    /**** Spells ****/
    // Dorothee
    SPELL_WATER_BOLT = 31012,
    SPELL_SCREAM = 31013,
    SPELL_SUMMON_TITO = 31014,

    // Tito
    SPELL_YIPPING = 31015,

    // Strawman
    SPELL_BRAIN_BASH = 31046,
    SPELL_BRAIN_WIPE = 31069,
    SPELL_BURNING_STRAW = 31073,

    // Tinhead
    SPELL_CLEAVE = 31043,
    SPELL_RUST = 31086,

    // Roar
    SPELL_MANGLE = 31041,
    SPELL_SHRED = 31042,

    // Crone
    SPELL_CHAIN_LIGHTNING = 32337,
    SPELL_FIREY_BROOM_PROC = 32339,

    // Cyclone
    SPELL_CYCLONE = 32334,
    NPC_CYCLONE = 18412,

    // Not part of the fight, but proves useful
    SPELL_STUN_SELF = 48342
};

struct MANGOS_DLL_DECL boss_dorotheeAI : public Scripted_BehavioralAI
{
    boss_dorotheeAI(Creature* pCreature) : Scripted_BehavioralAI(pCreature)
    {
        m_instance = pCreature->GetMapId() == KARAZHAN_MAP_ID ?
                         (instance_karazhan*)pCreature->GetInstanceData() :
                         NULL;
        m_aggroTimer = 10000;
        m_entryYellTimer = 1000;
        Reset();
    }

    instance_karazhan* m_instance;
    uint32 m_aggroTimer;
    uint32 m_entryYellTimer;
    uint32 m_fearTimer;
    uint32 m_titoTimer;

    void Reset() override
    {
        Scripted_BehavioralAI::Reset();
        m_fearTimer = 30000;
        m_titoTimer = urand(40000, 45000);
    }

    void JustReachedHome() override
    {
        if (m_instance &&
            m_instance->GetData(TYPE_OPERA) !=
                FAIL) // May have been failed by other add already
            m_instance->SetData(TYPE_OPERA, FAIL);
        m_creature->ForcedDespawn();
    }

    void JustDied(Unit* /*killer*/) override
    {
        DoScriptText(SAY_DOROTHEE_DEATH, m_creature);
        if (m_instance)
            m_instance->SetData(DATA_OPERA_OZ_COUNT,
                m_instance->GetData(DATA_OPERA_OZ_COUNT) - 1);
    }

    void UpdateAI(const uint32 diff) override
    {
        if (m_entryYellTimer)
        {
            if (m_entryYellTimer <= diff)
            {
                DoScriptText(SAY_DOROTHEE_AGGRO, m_creature);
                m_entryYellTimer = 0;
            }
            else
                m_entryYellTimer -= diff;
        }

        if (m_aggroTimer)
        {
            if (!m_creature->GetGroup() && m_instance)
                if (CreatureGroup* grp =
                        m_creature->GetMap()->GetCreatureGroupMgr().GetGroup(
                            m_instance->m_uiOzGrpId))
                    grp->AddMember(m_creature, false);
            if (m_aggroTimer <= diff)
            {
                m_creature->RemoveFlag(UNIT_FIELD_FLAGS,
                    UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE);
                m_creature->SetInCombatWithZone();
                m_aggroTimer = 0;
            }
            else
                m_aggroTimer -= diff;
        }

        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (m_titoTimer)
        {
            if (m_titoTimer <= diff)
            {
                if (DoCastSpellIfCan(m_creature, SPELL_SUMMON_TITO) == CAST_OK)
                    m_titoTimer = 0;
            }
            else
                m_titoTimer -= diff;
        }

        if (m_fearTimer <= diff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_SCREAM) == CAST_OK)
                m_fearTimer = 20000;
        }
        else
            m_fearTimer -= diff;

        Scripted_BehavioralAI::UpdateInCombatAI(diff);

        DoMeleeAttackIfReady();
    }
};

struct MANGOS_DLL_DECL boss_strawmanAI : public Scripted_BehavioralAI
{
    boss_strawmanAI(Creature* pCreature) : Scripted_BehavioralAI(pCreature)
    {
        m_instance = pCreature->GetMapId() == KARAZHAN_MAP_ID ?
                         (instance_karazhan*)pCreature->GetInstanceData() :
                         NULL;
        m_aggroTimer = 24000;
        DoCastSpellIfCan(m_creature, SPELL_STUN_SELF, CAST_TRIGGERED);
        Reset();
    }

    instance_karazhan* m_instance;
    uint32 m_aggroTimer;

    void Reset() override { Scripted_BehavioralAI::Reset(); }

    void AttackStart(Unit* pWho) override
    {
        if (!m_creature->has_aura(SPELL_STUN_SELF))
            Scripted_BehavioralAI::AttackStart(pWho);
    }

    void JustReachedHome() override
    {
        if (m_instance &&
            m_instance->GetData(TYPE_OPERA) !=
                FAIL) // May have been failed by other add already
            m_instance->SetData(TYPE_OPERA, FAIL);
        m_creature->ForcedDespawn();
    }

    void JustDied(Unit* /*killer*/) override
    {
        DoScriptText(SAY_STRAWMAN_DEATH, m_creature);

        if (m_instance)
            m_instance->SetData(DATA_OPERA_OZ_COUNT,
                m_instance->GetData(DATA_OPERA_OZ_COUNT) - 1);
    }

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_STRAWMAN_KILL);
    }

    void UpdateAI(const uint32 diff) override
    {
        if (m_aggroTimer)
        {
            if (!m_creature->GetGroup() && m_instance)
                if (CreatureGroup* grp =
                        m_creature->GetMap()->GetCreatureGroupMgr().GetGroup(
                            m_instance->m_uiOzGrpId))
                    grp->AddMember(m_creature, false);
            if (m_aggroTimer <= diff)
            {
                DoScriptText(SAY_STRAWMAN_AGGRO, m_creature);
                m_creature->remove_auras(SPELL_STUN_SELF);
                m_creature->RemoveFlag(UNIT_FIELD_FLAGS,
                    UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE);
                m_aggroTimer = 0;
            }
            else
                m_aggroTimer -= diff;
        }

        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        Scripted_BehavioralAI::UpdateInCombatAI(diff);

        DoMeleeAttackIfReady();
    }
};

struct MANGOS_DLL_DECL boss_tinheadAI : public Scripted_BehavioralAI
{
    boss_tinheadAI(Creature* pCreature) : Scripted_BehavioralAI(pCreature)
    {
        m_instance = pCreature->GetMapId() == KARAZHAN_MAP_ID ?
                         (instance_karazhan*)pCreature->GetInstanceData() :
                         NULL;
        m_aggroTimer = 33000;
        DoCastSpellIfCan(m_creature, SPELL_STUN_SELF, CAST_TRIGGERED);
        Reset();
    }

    instance_karazhan* m_instance;
    uint32 m_aggroTimer;
    uint32 m_rustTimer;
    bool m_emote;

    void Reset() override
    {
        Scripted_BehavioralAI::Reset();
        m_rustTimer = 7000;
        m_emote = false;
    }

    void AttackStart(Unit* pWho) override
    {
        if (!m_creature->has_aura(SPELL_STUN_SELF))
            Scripted_BehavioralAI::AttackStart(pWho);
    }

    void JustReachedHome() override
    {
        if (m_instance &&
            m_instance->GetData(TYPE_OPERA) !=
                FAIL) // May have been failed by other add already
            m_instance->SetData(TYPE_OPERA, FAIL);
        m_creature->ForcedDespawn();
    }

    void JustDied(Unit* /*killer*/) override
    {
        DoScriptText(SAY_TINHEAD_DEATH, m_creature);

        if (m_instance)
            m_instance->SetData(DATA_OPERA_OZ_COUNT,
                m_instance->GetData(DATA_OPERA_OZ_COUNT) - 1);
    }

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_TINHEAD_KILL);
    }

    void UpdateAI(const uint32 diff) override
    {
        if (m_aggroTimer)
        {
            if (!m_creature->GetGroup() && m_instance)
                if (CreatureGroup* grp =
                        m_creature->GetMap()->GetCreatureGroupMgr().GetGroup(
                            m_instance->m_uiOzGrpId))
                    grp->AddMember(m_creature, false);
            if (m_aggroTimer <= diff)
            {
                DoScriptText(SAY_TINHEAD_AGGRO, m_creature);
                m_creature->remove_auras(SPELL_STUN_SELF);
                m_creature->RemoveFlag(UNIT_FIELD_FLAGS,
                    UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE);
                m_aggroTimer = 0;
            }
            else
                m_aggroTimer -= diff;
        }

        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        Scripted_BehavioralAI::UpdateInCombatAI(diff);

        if (m_rustTimer <= diff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_RUST) == CAST_OK)
            {
                if (!m_emote)
                {
                    DoScriptText(EMOTE_RUST, m_creature);
                    m_emote = true;
                }
                m_rustTimer = 5000;
            }
        }
        else
            m_rustTimer -= diff;

        DoMeleeAttackIfReady();
    }
};

struct MANGOS_DLL_DECL boss_roarAI : public Scripted_BehavioralAI
{
    boss_roarAI(Creature* pCreature) : Scripted_BehavioralAI(pCreature)
    {
        m_instance = pCreature->GetMapId() == KARAZHAN_MAP_ID ?
                         (instance_karazhan*)pCreature->GetInstanceData() :
                         NULL;
        m_aggroTimer = 14500;
        DoCastSpellIfCan(m_creature, SPELL_STUN_SELF, CAST_TRIGGERED);
        Reset();
    }

    instance_karazhan* m_instance;
    uint32 m_aggroTimer;

    void Reset() override { Scripted_BehavioralAI::Reset(); }

    void AttackStart(Unit* pWho) override
    {
        if (!m_creature->has_aura(SPELL_STUN_SELF))
            Scripted_BehavioralAI::AttackStart(pWho);
    }

    void JustReachedHome() override
    {
        if (m_instance &&
            m_instance->GetData(TYPE_OPERA) !=
                FAIL) // May have been failed by other add already
            m_instance->SetData(TYPE_OPERA, FAIL);
        m_creature->ForcedDespawn();
    }

    void JustDied(Unit* /*killer*/) override
    {
        DoScriptText(SAY_ROAR_DEATH, m_creature);

        if (m_instance)
            m_instance->SetData(DATA_OPERA_OZ_COUNT,
                m_instance->GetData(DATA_OPERA_OZ_COUNT) - 1);
    }

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_ROAR_KILL);
    }

    void UpdateAI(const uint32 diff) override
    {
        if (m_aggroTimer)
        {
            if (!m_creature->GetGroup() && m_instance)
                if (CreatureGroup* grp =
                        m_creature->GetMap()->GetCreatureGroupMgr().GetGroup(
                            m_instance->m_uiOzGrpId))
                    grp->AddMember(m_creature, false);
            if (m_aggroTimer <= diff)
            {
                DoScriptText(SAY_ROAR_AGGRO, m_creature);
                m_creature->remove_auras(SPELL_STUN_SELF);
                m_creature->RemoveFlag(UNIT_FIELD_FLAGS,
                    UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE);
                m_aggroTimer = 0;
            }
            else
                m_aggroTimer -= diff;
        }

        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        Scripted_BehavioralAI::UpdateInCombatAI(diff);

        DoMeleeAttackIfReady();
    }
};

/***
 CYCLONE
***/
#define CYCLONE_PATH_SIZE 13
const float CycloneMovement[CYCLONE_PATH_SIZE][4] = {
    /*{   // Cyclone One
        { -10911.9, -1774.74, 90.477, 5.83943 },
        { -10899.7, -1780.28, 90.477, 0.300019 },
        { -10893.6, -1778.38, 90.477, 0.908702 },
        { -10886.3, -1769.08, 90.477, 0.947972 },
        { -10873.7, -1750.61, 90.477, 2.99394 },
        { -10892.5, -1747.81, 90.477, 4.59222 },
        { -10894.6, -1762.72, 90.477, 4.59222 },
        { -10905.6, -1750, 90.477, 2.59731 },
        { -10911.7, -1759.4, 90.4771, 5.73104 },
        { -10895, -1764.91, 90.4767, 6.08838 },
        { -10877.9, -1777.52, 90.4768, 3.27667 },
        { -10893.7, -1779.67, 90.4768, 2.3656 },
        { -10906.1, -1764.02, 90.4768, 4.04243 },
    },*/
    // Cyclone path
    {-10911.1, -1772.37, 90.4775, 5.81744},
    {-10900.5, -1777.74, 90.4775, 0.248961},
    {-10895.3, -1775.87, 90.4775, 0.725698},
    {-10882.7, -1771.52, 90.4775, 1.13018},
    {-10874.4, -1754.05, 90.4775, 3.01906},
    {-10889.3, -1750.03, 90.4775, 4.31889}, {-10891, -1763.3, 90.4775, 4.63698},
    {-10907.6, -1754.29, 90.4775, 2.39859},
    {-10913.3, -1762.92, 90.4775, 5.96822},
    {-10896.6, -1769.63, 90.4775, 5.94073},
    {-10878.4, -1771.7, 90.4775, 3.51386},
    {-10891.7, -1774.13, 90.4775, 2.6185},
    {-10909.6, -1759.6, 90.4775, 4.52309},
    /*{   // Cyclone three
        { -10907.6, -1774.91, 90.4776, 6.05068 },
        { -10896.5, -1777.98, 90.4776, 0.63143 },
        { -10892.4, -1773.77, 90.4776, 0.52933 },
        { -10882.3, -1766.84, 90.4776, 0.890613 },
        { -10876.1, -1751.53, 90.4776, 2.98763 },
        { -10891.8, -1751.45, 90.4776, 4.59769 },
        { -10893.7, -1765.76, 90.4776, 4.54272 },
        { -10911.4, -1749.67, 90.4776, 1.7035 },
        { -10907.3, -1763.29, 90.4776, 5.92423 },
        { -10892.4, -1769.41, 90.4776, 5.7907 },
        { -10880.6, -1776.17, 90.4776, 3.39525 },
        { -10895.6, -1773.3, 90.4776, 2.4724 },
        { -10907, -1765.83, 90.4776, 2.78264 },
    }*/
};

#define CYCLONE_COUNT 3
float CycloneSpawnPos[CYCLONE_COUNT][4] = {{-10911.9f, -1774.7f, 90.5f, 5.8f},
    {-10911.1f, -1772.4f, 90.5f, 5.8f}, {-10907.6f, -1774.9f, 90.5f, 6.1f}};

struct MANGOS_DLL_DECL boss_croneAI : public ScriptedAI
{
    boss_croneAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_instance = pCreature->GetMapId() == KARAZHAN_MAP_ID ?
                         (instance_karazhan*)pCreature->GetInstanceData() :
                         NULL;
        m_aggroTimer = 6000;
        m_cycloneGrpId = 0;
        Reset();
    }

    int32 m_cycloneGrpId;
    instance_karazhan* m_instance;
    uint32 m_aggroTimer;
    uint32 m_chainLightningTimer;
    bool m_cyclones;

    void Reset() override
    {
        m_chainLightningTimer = urand(5000, 10000);
        m_cyclones = false;
    }

    void DespawnCyclones()
    {
        if (CreatureGroup* grp =
                m_creature->GetMap()->GetCreatureGroupMgr().GetGroup(
                    m_cycloneGrpId))
        {
            for (auto& elem : grp->GetMembers())
                if ((elem)->isAlive())
                    (elem)->ForcedDespawn();
            m_creature->GetMap()->GetCreatureGroupMgr().DeleteGroup(
                m_cycloneGrpId);
        }
        m_cycloneGrpId = 0;
    }

    void JustReachedHome() override
    {
        DespawnCyclones();
        if (m_instance)
            m_instance->SetData(TYPE_OPERA, FAIL);
        m_creature->ForcedDespawn();
    }

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_CRONE_KILL);
    }

    void Aggro(Unit* /*who*/) override
    {
        DoScriptText(
            urand(0, 1) ? SAY_CRONE_AGGRO_1 : SAY_CRONE_AGGRO_2, m_creature);
    }

    void JustDied(Unit* /*killer*/) override
    {
        DoScriptText(SAY_CRONE_DEATH, m_creature);
        DespawnCyclones();

        if (m_instance)
            m_instance->SetData(TYPE_OPERA, DONE);
    }

    void UpdateAI(const uint32 diff) override
    {
        if (!m_cyclones)
        {
            m_cycloneGrpId =
                m_creature->GetMap()->GetCreatureGroupMgr().CreateNewGroup(
                    "Crone's Cyclones", true);
            CreatureGroup* grp =
                m_creature->GetMap()->GetCreatureGroupMgr().GetGroup(
                    m_cycloneGrpId);
            std::vector<Creature*> cyclones;
            if (grp)
            {
                grp->AddFlag(CREATURE_GROUP_FLAG_GROUP_MOVEMENT);
                for (auto& pos : CycloneSpawnPos)
                {
                    if (Creature* cyclone = m_creature->SummonCreature(
                            NPC_CYCLONE, pos[0], pos[1], pos[2], pos[3],
                            TEMPSUMMON_MANUAL_DESPAWN, 0))
                    {
                        grp->AddMember(cyclone, false);
                        m_creature->GetMap()
                            ->GetCreatureGroupMgr()
                            .GetMovementMgr()
                            .SetNewFormation(m_cycloneGrpId, cyclone);
                        cyclones.push_back(cyclone);
                    }
                }

                for (auto& elem : CycloneMovement)
                    m_creature->GetMap()
                        ->GetCreatureGroupMgr()
                        .GetMovementMgr()
                        .AddWaypoint(
                            m_cycloneGrpId, DynamicWaypoint(elem[0], elem[1],
                                                elem[2], elem[3], 0, false));

                m_creature->GetMap()
                    ->GetCreatureGroupMgr()
                    .GetMovementMgr()
                    .StartMovement(m_cycloneGrpId, cyclones);
            }
            m_cyclones = true;
        }

        if (m_aggroTimer)
        {
            if (m_aggroTimer <= diff)
            {
                m_creature->RemoveFlag(UNIT_FIELD_FLAGS,
                    UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE);
                m_aggroTimer = 0;
            }
            else
                m_aggroTimer -= diff;
        }

        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (m_chainLightningTimer <= diff)
        {
            if (DoCastSpellIfCan(
                    m_creature->getVictim(), SPELL_CHAIN_LIGHTNING) == CAST_OK)
                m_chainLightningTimer = urand(10000, 20000);
        }
        else
            m_chainLightningTimer -= diff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_dorothee(Creature* pCreature)
{
    return new boss_dorotheeAI(pCreature);
}

CreatureAI* GetAI_boss_strawman(Creature* pCreature)
{
    return new boss_strawmanAI(pCreature);
}

CreatureAI* GetAI_boss_tinhead(Creature* pCreature)
{
    return new boss_tinheadAI(pCreature);
}

CreatureAI* GetAI_boss_roar(Creature* pCreature)
{
    return new boss_roarAI(pCreature);
}

CreatureAI* GetAI_boss_crone(Creature* pCreature)
{
    return new boss_croneAI(pCreature);
}

void Opera_WizardOfOz()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->GetAI = &GetAI_boss_dorothee;
    pNewScript->Name = "boss_dorothee";
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->GetAI = &GetAI_boss_strawman;
    pNewScript->Name = "boss_strawman";
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->GetAI = &GetAI_boss_tinhead;
    pNewScript->Name = "boss_tinhead";
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->GetAI = &GetAI_boss_roar;
    pNewScript->Name = "boss_roar";
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->GetAI = &GetAI_boss_crone;
    pNewScript->Name = "boss_crone";
    pNewScript->RegisterSelf();
}
