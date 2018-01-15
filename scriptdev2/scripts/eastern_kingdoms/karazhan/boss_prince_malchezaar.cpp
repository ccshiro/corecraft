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
SDName: Boss_Prince_Malchezzar
SD%Complete: 100
SDComment:
SDCategory: Karazhan
EndScriptData */

#include "karazhan.h"
#include "precompiled.h"

// 32 Coordinates for Infernal spawns
struct InfernalPoint
{
    float x, y;
};

#define INFERNAL_Z 275.5

static InfernalPoint InfernalPoints[] = {
    {-10922.8f, -1985.2f}, {-10916.2f, -1996.2f}, {-10932.2f, -2008.1f},
    {-10948.8f, -2022.1f}, {-10958.7f, -1997.7f}, {-10971.5f, -1997.5f},
    {-10990.8f, -1995.1f}, {-10989.8f, -1976.5f}, {-10971.6f, -1973.0f},
    {-10955.5f, -1974.0f}, {-10939.6f, -1969.8f}, {-10958.0f, -1952.2f},
    {-10941.7f, -1954.8f}, {-10943.1f, -1988.5f}, {-10948.8f, -2005.1f},
    {-10984.0f, -2019.3f}, {-10932.8f, -1979.6f}, {-10935.7f, -1996.0f},

    // OURS
    {-10967.4f, -2010.1f}, {-10967.6f, -2023.9f}, {-10981.9f, -2006.9f},
    {-10966.9f, -1983.3f}, {-11003.3f, -1985.0f}, {-10977.7f, -1961.1f},
    {-10921.8f, -2008.8f}, {-10940.1f, -2016.0f}, {-10992.7f, -2007.0f},
    {-10979.9f, -1985.2f}, {-10957.2f, -2017.5f}, {-10957.9f, -1963.2f},
    {-10929.9f, -1969.4f}, {-10928.8f, -2016.8f},
};

enum
{
    TOTAL_INFERNAL_POINTS = 32,

    SAY_AGGRO = -1532091,
    SAY_PHASE_2 = -1532092,
    SAY_PHASE_3 = -1532096,
    SAY_KILL_1 = -1532097,
    SAY_KILL_2 = -1532098,
    SAY_KILL_3 = -1532099,
    SAY_SUMMON_1 = -1532100,
    SAY_SUMMON_2 = -1532101,
    SAY_DEATH = -1532102,

    SAY_UNUSED_1 = -1532093, // Unclear what these are used for
    SAY_UNUSED_2 = -1532094,
    SAY_UNUSED_3 = -1532095,

    // Enfeeble is supposed to reduce hp to 1 and then heal player back to full
    // when it ends
    // Along with reducing healing and regen while enfeebled to 0%
    // This spell effect will only reduce healing
    SPELL_ENFEEBLE = 30843,
    SPELL_ENFEEBLE_EFFECT = 41624,

    SPELL_SHADOWNOVA = 30852,
    SPELL_SW_PAIN = 30854,
    SPELL_SUNDER_ARMOR = 30901,
    SPELL_CLEAVE = 30131,
    SPELL_THRASH_AURA = 12787,
    SPELL_EQUIP_AXES = 30857,
    SPELL_AMPLIFY_DAMAGE = 39095,
    SPELL_SUMMON_AXES = 30891, // We made this spell into a dummy effect only;
                               // makes scripting easier
    NPC_NETHERSPITE_INFERNAL = 17646,
    NPC_MALCHEZAARS_AXE = 17650,
    SPELL_HELLFIRE = 30859, // Infenals' hellfire spell

    INFERNAL_MODEL_INVISIBLE = 11686, // Infernal Effects
    SPELL_INFERNAL_RELAY = 30834,

    EQUIPMENT_RAW_ID_AXES = 6090,

    SPELL_PRINCE_MALCHEZAAR_PHASE_TWO_ATTACK_SPEED_AURA = 150059,
};

//---------Infernal code first
struct MANGOS_DLL_DECL netherspite_infernalAI : public ScriptedAI
{
    netherspite_infernalAI(Creature* pCreature)
      : ScriptedAI(pCreature), m_hellfireTimer(0), m_cleanupTimer(0),
        pPoint(NULL)
    {
        Reset();
    }

    uint32 m_hellfireTimer;
    uint32 m_cleanupTimer;
    ObjectGuid m_malchezaarGuid;
    InfernalPoint* pPoint;

    // Disable combat:
    void MoveInLineOfSight(Unit* /*pWho*/) override {}
    void AttackStart(Unit* /*pWho*/) override {}
    void AttackedBy(Unit* /*pAttacker*/) override {}

    void Reset() override {}

    void UpdateAI(const uint32 uiDiff) override
    {
        m_creature->SetTargetGuid(ObjectGuid());

        if (m_hellfireTimer)
        {
            if (m_hellfireTimer <= uiDiff)
            {
                if (DoCastSpellIfCan(m_creature, SPELL_HELLFIRE) == CAST_OK)
                    m_hellfireTimer = 0;
            }
            else
                m_hellfireTimer -= uiDiff;
        }

        if (m_cleanupTimer)
        {
            if (m_cleanupTimer <= uiDiff)
            {
                Cleanup();
                m_cleanupTimer = 0;
            }
            else
                m_cleanupTimer -= uiDiff;
        }
    }

    void KilledUnit(Unit* who) override
    {
        Creature* pMalchezaar =
            m_creature->GetMap()->GetCreature(m_malchezaarGuid);
        if (pMalchezaar && pMalchezaar->AI())
            pMalchezaar->AI()->KilledUnit(who);
    }

    void SpellHit(Unit* /*pWho*/, const SpellEntry* pSpell) override
    {
        if (pSpell->Id == SPELL_INFERNAL_RELAY)
        {
            m_creature->SetDisplayId(
                m_creature->GetUInt32Value(UNIT_FIELD_NATIVEDISPLAYID));
            m_creature->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
            m_hellfireTimer = 4000;
            m_cleanupTimer = 180000;
        }
    }

    void DamageTaken(Unit* pDealer, uint32& uiDamage) override
    {
        if (pDealer->GetObjectGuid() != m_malchezaarGuid)
            uiDamage = 0;
    }

    void Cleanup(); // below ...
};

struct MANGOS_DLL_DECL boss_malchezaarAI : public ScriptedAI
{
    boss_malchezaarAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_instance = pCreature->GetMapId() == KARAZHAN_MAP_ID ?
                         (instance_karazhan*)pCreature->GetInstanceData() :
                         NULL;
        Reset();
    }

    instance_karazhan* m_instance;
    uint32 m_enfeebleTimer;
    uint32 m_enfeebleResetTimer;
    uint32 m_shadowNovaTimer;
    uint32 m_SWPainTimer;
    uint32 m_sunderArmorTimer;
    uint32 m_amplifyDamageTimer;
    uint32 m_cleaveTimer;
    uint32 m_infernalTimer;
    uint32 m_axesTargetSwitchTimer;
    uint32 m_infernalRelayTimer;
    ObjectGuid m_infernalRelayGuid;
    GUIDVector m_infernalGuids;
    ObjectGuid m_axeGuid;
    ObjectGuid m_enfeebleTargetGuid[5];
    uint32 m_enfeebleHealth[5];
    uint32 m_phase;
    std::vector<InfernalPoint*> m_positions;

    void Reset() override
    {
        AxesCleanup();
        ClearWeapons();
        InfernalCleanup();
        m_positions.clear();

        for (int i = 0; i < 5; ++i)
        {
            m_enfeebleTargetGuid[i] = ObjectGuid();
            m_enfeebleHealth[i] = 0;
        }

        for (auto& InfernalPoint : InfernalPoints)
            m_positions.push_back(&InfernalPoint);

        m_enfeebleTimer = 30000;
        m_SWPainTimer = urand(5000, 15000);
        m_amplifyDamageTimer = 10000;
        m_infernalTimer = 20000;
        m_cleaveTimer = urand(10000, 15000);
        m_phase = 1;
        m_enfeebleResetTimer = 0;
        m_shadowNovaTimer = 0;
        m_axesTargetSwitchTimer = 0;
        m_sunderArmorTimer = 0;
        m_infernalRelayTimer = 0;
        m_infernalRelayGuid = ObjectGuid();

        m_creature->SetAggroDistance(60.0f);
    }

    void JustSummoned(Creature* pCreature) override
    {
        if (pCreature->GetEntry() == NPC_MALCHEZAARS_AXE)
            m_axeGuid = pCreature->GetObjectGuid();
    }

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_KILL_1, SAY_KILL_2, SAY_KILL_3);
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        DoScriptText(SAY_DEATH, m_creature);

        AxesCleanup();
        ClearWeapons();
        InfernalCleanup();
        m_positions.clear();

        if (m_instance)
            m_instance->SetData(TYPE_MALCHEZAAR, DONE);
    }

    void Aggro(Unit* /*who*/) override
    {
        DoScriptText(SAY_AGGRO, m_creature);
        if (m_instance)
            m_instance->SetData(TYPE_MALCHEZAAR, IN_PROGRESS);
    }

    void JustReachedHome() override
    {
        if (m_instance)
            m_instance->SetData(TYPE_MALCHEZAAR, FAIL);
    }

    void InfernalCleanup()
    {
        // Infernal Cleanup
        for (GUIDVector::const_iterator itr = m_infernalGuids.begin();
             itr != m_infernalGuids.end(); ++itr)
        {
            Creature* pInfernal = m_creature->GetMap()->GetCreature(*itr);
            if (pInfernal && pInfernal->isAlive())
                pInfernal->ForcedDespawn();
        }
        m_infernalGuids.clear();
    }

    void AxesCleanup()
    {
        Creature* pAxe = m_creature->GetMap()->GetCreature(m_axeGuid);
        if (pAxe && pAxe->isAlive())
            pAxe->ForcedDespawn();
    }

    void ClearWeapons()
    {
        m_creature->SetCurrentEquipmentId(
            m_creature->GetCreatureInfo()->equipmentId);
        m_creature->LoadEquipment(
            m_creature->GetCreatureInfo()->equipmentId, true);
        m_creature->remove_auras(
            SPELL_PRINCE_MALCHEZAAR_PHASE_TWO_ATTACK_SPEED_AURA);
    }

    void DualWieldAxes()
    {
        m_creature->SetCurrentEquipmentId(EQUIPMENT_RAW_ID_AXES);
        m_creature->LoadEquipment(EQUIPMENT_RAW_ID_AXES);
        m_creature->CastSpell(m_creature, SPELL_THRASH_AURA, true);
        m_creature->CastSpell(m_creature,
            SPELL_PRINCE_MALCHEZAAR_PHASE_TWO_ATTACK_SPEED_AURA, true);
    }

    void EnfeebleHealthEffect()
    {
        const SpellEntry* info = GetSpellStore()->LookupEntry(SPELL_ENFEEBLE);
        if (!info)
            return;

        ThreatList const& tList =
            m_creature->getThreatManager().getThreatList();
        std::vector<Unit*> targets;

        if (tList.empty())
            return;

        // begin + 1 , so we don't target the one with the highest threat
        ThreatList::const_iterator itr = tList.begin();
        std::advance(itr, 1);
        for (; itr != tList.end(); ++itr)
        {
            Unit* target = m_creature->GetMap()->GetUnit((*itr)->getUnitGuid());
            // only on alive players
            if (target && target->isAlive() &&
                target->GetTypeId() == TYPEID_PLAYER &&
                !target->IsImmuneToSpell(info))
                targets.push_back(target);
        }

        // cut until we only have 5 players
        while (targets.size() > 5)
            targets.erase(targets.begin() + urand(0, targets.size() - 1));

        int i = 0;
        for (std::vector<Unit*>::iterator itr = targets.begin();
             itr != targets.end(); ++itr, ++i)
        {
            Unit* target = *itr;
            m_enfeebleTargetGuid[i] = target->GetObjectGuid();
            m_enfeebleHealth[i] = target->GetHealth();

            DoCastSpellIfCan(target, SPELL_ENFEEBLE, CAST_TRIGGERED);
            target->SetHealth(1);
        }
    }

    void EnfeebleResetHealth()
    {
        for (int i = 0; i < 5; ++i)
        {
            Player* pTarget =
                m_creature->GetMap()->GetPlayer(m_enfeebleTargetGuid[i]);

            if (pTarget && pTarget->isAlive())
                pTarget->SetHealth(m_enfeebleHealth[i]);

            m_enfeebleTargetGuid[i].Clear();
            m_enfeebleHealth[i] = 0;
        }
    }

    void SummonInfernal()
    {
        InfernalPoint* point = NULL;
        float posX, posY, posZ;
        if ((m_creature->GetMapId() != 532) || m_positions.empty())
        {
            auto pos = m_creature->GetPointXYZ(
                G3D::Vector3(
                    m_creature->GetX(), m_creature->GetY(), m_creature->GetZ()),
                2 * M_PI_F * rand_norm_f(), 60.0f * rand_norm_f());
            posX = pos.x;
            posY = pos.y;
            posZ = pos.z;
        }
        else
        {
            // Exclude all points further than 70 yards from malchezaar
            std::vector<InfernalPoint*> points;
            points.reserve(m_positions.size());
            for (auto& elem : m_positions)
                if (m_creature->GetDistance2d((elem)->x, (elem)->y) < 70.0f)
                    points.push_back(elem);

            // Find an infernal not close to others. Start with a min distance
            // of 40 yards,
            // lower it to 30 and 20 and then eventually 10; or, until we find a
            // valid point
            uint32 minDist = 40;
            std::vector<InfernalPoint*> available;
            available.reserve(points.size());
            while (available.empty() && minDist >= 10)
            {
                for (auto& point : points)
                {
                    bool pushBack = true;
                    for (auto& elem : m_infernalGuids)
                        if (Creature* infernal =
                                m_creature->GetMap()->GetCreature(elem))
                            if (infernal->GetDistance2d(
                                    (point)->x, (point)->y) < minDist)
                            {
                                pushBack = false;
                                break;
                            }
                    if (pushBack)
                        available.push_back(point);
                }
                minDist -= 10;
            }

            if (available.empty())
                point = m_positions[urand(0, m_positions.size() - 1)];
            else
                point = available[urand(0, available.size() - 1)];

            if (!point)
                return;

            // Remove point from available points (will be added back in once
            // the infernal disappears)
            std::vector<InfernalPoint*>::iterator itr =
                std::find(m_positions.begin(), m_positions.end(), point);
            if (itr != m_positions.end())
                m_positions.erase(itr);

            posX = point->x;
            posY = point->y;
            posZ = INFERNAL_Z;
        }

        if (Creature* infernal =
                m_creature->SummonCreature(NPC_NETHERSPITE_INFERNAL, posX, posY,
                    posZ, frand(0, 6), TEMPSUMMON_MANUAL_DESPAWN, 0))
        {
            if (netherspite_infernalAI* infernalAI =
                    dynamic_cast<netherspite_infernalAI*>(infernal->AI()))
            {
                if (point)
                    infernalAI->pPoint = point;
                infernalAI->m_malchezaarGuid = m_creature->GetObjectGuid();
            }
            infernal->SetDisplayId(INFERNAL_MODEL_INVISIBLE);
            m_infernalGuids.push_back(infernal->GetObjectGuid());
            m_infernalRelayGuid = infernal->GetObjectGuid();
            m_infernalRelayTimer = 13500;
        }

        DoScriptText(urand(0, 1) ? SAY_SUMMON_1 : SAY_SUMMON_2, m_creature);
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (m_enfeebleResetTimer)
        {
            if (m_enfeebleResetTimer)
            {
                if (m_enfeebleResetTimer <= uiDiff)
                {
                    EnfeebleResetHealth();
                    m_enfeebleResetTimer = 0;
                }
                else
                    m_enfeebleResetTimer -= uiDiff;
            }
        }

        if (m_creature->hasUnitState(UNIT_STAT_STUNNED)) // While shifting to
                                                         // m_uiPhase 2
                                                         // m_malchezaarGuid
                                                         // stuns himself
            return;

        if (m_phase == 1)
        {
            if (m_creature->GetHealthPercent() < 61.0f)
            {
                if (DoCastSpellIfCan(m_creature, SPELL_EQUIP_AXES) == CAST_OK)
                {
                    m_phase = 2;
                    DoScriptText(SAY_PHASE_2, m_creature);
                    DualWieldAxes();

                    m_SWPainTimer = 0;
                    m_sunderArmorTimer = urand(3000, 5000);
                }
            }
        }
        else if (m_phase == 2)
        {
            if (m_creature->GetHealthPercent() < 31.0f)
            {
                if (DoCastSpellIfCan(m_creature, SPELL_SUMMON_AXES) == CAST_OK)
                {
                    auto pos = m_creature->GetPoint(0.0f, 5.0f);
                    m_creature->SummonCreature(NPC_MALCHEZAARS_AXE, pos.x,
                        pos.y, pos.z, frand(0, 2 * M_PI_F),
                        TEMPSUMMON_MANUAL_DESPAWN, 0);
                    m_phase = 3;
                    ClearWeapons();
                    m_creature->remove_auras(SPELL_THRASH_AURA);
                    DoScriptText(SAY_PHASE_3, m_creature);

                    m_axesTargetSwitchTimer = 1; // "Switch" target right away
                    m_shadowNovaTimer = urand(3000, 5000);
                    return; // Skip rest of phase 2 logic
                }
            }

            if (m_sunderArmorTimer <= uiDiff)
            {
                if (DoCastSpellIfCan(
                        m_creature->getVictim(), SPELL_SUNDER_ARMOR) == CAST_OK)
                    m_sunderArmorTimer = 10000;
            }
            else
                m_sunderArmorTimer -= uiDiff;

            if (m_cleaveTimer <= uiDiff)
            {
                if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_CLEAVE) ==
                    CAST_OK)
                    m_cleaveTimer = urand(10000, 15000);
            }
            else
                m_cleaveTimer -= uiDiff;
        }
        else
        {
            if (m_axesTargetSwitchTimer <= uiDiff)
            {
                m_axesTargetSwitchTimer = urand(5000, 15000);

                if (Unit* target = m_creature->SelectAttackingTarget(
                        ATTACKING_TARGET_RANDOM, 0))
                {
                    if (Creature* axe =
                            m_creature->GetMap()->GetCreature(m_axeGuid))
                        axe->SetFocusTarget(target);
                }
            }
            else
                m_axesTargetSwitchTimer -= uiDiff;

            if (m_amplifyDamageTimer <= uiDiff)
            {
                if (Unit* target = m_creature->SelectAttackingTarget(
                        ATTACKING_TARGET_RANDOM, 1, SPELL_AMPLIFY_DAMAGE))
                    if (DoCastSpellIfCan(target, SPELL_AMPLIFY_DAMAGE) ==
                        CAST_OK)
                        m_amplifyDamageTimer = 10000;
            }
            else
                m_amplifyDamageTimer -= uiDiff;
        }

        // Time for global and double timers
        if (m_infernalRelayTimer && m_instance)
        {
            if (m_infernalRelayTimer <= uiDiff)
            {
                if (Creature* relay = m_instance->GetSingleCreatureFromStorage(
                        NPC_INFERNAL_RELAY))
                {
                    if (Creature* infernal = m_creature->GetMap()->GetCreature(
                            m_infernalRelayGuid))
                    {
                        relay->CastSpell(infernal, SPELL_INFERNAL_RELAY, false);
                        m_infernalRelayTimer = 0;
                    }
                }
            }
            else
                m_infernalRelayTimer -= uiDiff;
        }

        if (m_infernalTimer <= uiDiff)
        {
            SummonInfernal();
            m_infernalTimer = m_creature->GetHealthPercent() > 15.0f ?
                                  urand(45000, 50000) :
                                  20000;
        }
        else
            m_infernalTimer -= uiDiff;

        if (m_shadowNovaTimer)
        {
            if (m_shadowNovaTimer <= uiDiff)
            {
                if (DoCastSpellIfCan(m_creature, SPELL_SHADOWNOVA) == CAST_OK)
                {
                    if (m_phase ==
                        3) // In phase 3 we just keep casting shadow nova
                        m_shadowNovaTimer = 20000;
                    else
                        m_shadowNovaTimer = 0;
                }
            }
            else
                m_shadowNovaTimer -= uiDiff;
        }

        if (m_phase != 2)
        {
            if (m_SWPainTimer <= uiDiff)
            {
                if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_SW_PAIN) ==
                    CAST_OK)
                    m_SWPainTimer = urand(25000, 35000);
            }
            else
                m_SWPainTimer -= uiDiff;
        }

        if (m_phase != 3)
        {
            if (m_enfeebleTimer <= uiDiff)
            {
                EnfeebleHealthEffect();
                m_enfeebleTimer = 30000;
                m_shadowNovaTimer = 6000;
                m_enfeebleResetTimer = 8300;
            }
            else
                m_enfeebleTimer -= uiDiff;
        }

        DoMeleeAttackIfReady();
    }

    void Cleanup(Creature* infernal, InfernalPoint* point)
    {
        for (GUIDVector::iterator itr = m_infernalGuids.begin();
             itr != m_infernalGuids.end(); ++itr)
            if (*itr == infernal->GetObjectGuid())
            {
                m_infernalGuids.erase(itr);
                break;
            }

        if (point)
            m_positions.push_back(point);
    }
};

void netherspite_infernalAI::Cleanup()
{
    Creature* pMalchezaar = m_creature->GetMap()->GetCreature(m_malchezaarGuid);

    if (pMalchezaar && pMalchezaar->isAlive())
    {
        if (boss_malchezaarAI* pMalAI =
                dynamic_cast<boss_malchezaarAI*>(pMalchezaar->AI()))
            pMalAI->Cleanup(m_creature, pPoint);
    }

    m_creature->ForcedDespawn();
}

CreatureAI* GetAI_netherspite_infernal(Creature* pCreature)
{
    return new netherspite_infernalAI(pCreature);
}

CreatureAI* GetAI_boss_malchezaar(Creature* pCreature)
{
    return new boss_malchezaarAI(pCreature);
}

void AddSC_boss_prince_malchezaar()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_malchezaar";
    pNewScript->GetAI = &GetAI_boss_malchezaar;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "netherspite_infernal";
    pNewScript->GetAI = &GetAI_netherspite_infernal;
    pNewScript->RegisterSelf();
}
