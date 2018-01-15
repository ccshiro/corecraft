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
SDName: boss_midnight, boss_attumen_fake, boss_attumen
SD%Complete: 100
SDComment:
SDCategory: Karazhan
EndScriptData */

#include "karazhan.h"
#include "precompiled.h"

enum
{
    EMOTE_MIDNIGHT_SUMMON = -1531999,
    SAY_MIDNIGHT_KILL = -1532000,
    SAY_MOUNT = -1532004,

    SPELL_SUMMON_ATTUMEN = 29714,
    SPELL_SUMMON_ATTUMEN_MOUNTED = 29799,
    SPELL_KNOCKDOWN = 29711,

    // Not actually related to this fight, but provides a good way to stun
    // attumen for phase 2 -> 3 transition & midnight for "kill":
    SPELL_SELF_STUN = 48342,
};

/***
 MIDNIGHT
***/
struct MANGOS_DLL_DECL boss_midnightAI : public ScriptedAI
{
    boss_midnightAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_instance = pCreature->GetMapId() == KARAZHAN_MAP_ID ?
                         (instance_karazhan*)pCreature->GetInstanceData() :
                         NULL;
        Reset();
    }

    instance_karazhan* m_instance;
    uint32 m_phase;
    uint32 m_knockdownTimer;

    void Reset() override
    {
        m_phase = 1;
        m_knockdownTimer = urand(7000, 12000);

        m_creature->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
    }

    void JustReachedHome() override
    {
        if (m_instance)
            m_instance->SetData(TYPE_ATTUMEN, FAIL);
        m_creature->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
        m_creature->SetVisibility(VISIBILITY_ON);
    }

    void Aggro(Unit* /*tar*/) override
    {
        if (m_instance)
            m_instance->SetData(TYPE_ATTUMEN, IN_PROGRESS);
    }

    void KilledUnit(Unit* victim) override
    {
        if (m_instance)
            if (Creature* attumen = m_instance->GetSingleCreatureFromStorage(
                    NPC_ATTUMEN_THE_HUNTSMAN_FAKE))
                DoKillSay(attumen, victim, SAY_MIDNIGHT_KILL);
    }

    void JustSummoned(Creature* creature) override
    {
        if (m_creature->getVictim() && creature->AI())
            creature->AI()->AttackStart(m_creature->getVictim());
    }

    void Mount()
    {
        if (!m_instance || m_phase == 3)
            return;
        Creature* attumen = m_instance->GetSingleCreatureFromStorage(
            NPC_ATTUMEN_THE_HUNTSMAN_FAKE);
        if (!attumen)
            return;

        DoScriptText(SAY_MOUNT, attumen);
        attumen->SetFacingTo(attumen->GetAngle(m_creature));
        m_phase = 3;

        attumen->CastSpell(attumen, SPELL_SELF_STUN, true);

        // Midnight runs to attumen
        auto pos = attumen->GetPoint(m_creature);
        m_creature->movement_gens.push(new movement::PointMovementGenerator(
            1000, pos.x, pos.y, pos.z, true, true));
    }

    void MovementInform(movement::gen moveType, uint32 pointId) override
    {
        if (moveType == movement::gen::point && pointId == 1000)
        {
            if (Creature* attumen = m_instance->GetSingleCreatureFromStorage(
                    NPC_ATTUMEN_THE_HUNTSMAN_FAKE))
                attumen->CastSpell(attumen, SPELL_SUMMON_ATTUMEN_MOUNTED, true);
        }
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (m_creature->has_aura(SPELL_SELF_STUN) ||
            !m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        switch (m_phase)
        {
        case 1:
            if (m_creature->GetHealthPercent() < 96.0f)
            {
                DoScriptText(EMOTE_MIDNIGHT_SUMMON, m_creature);
                m_creature->CastSpell(m_creature, SPELL_SUMMON_ATTUMEN, true);
                m_phase = 2;
            }
            break;
        case 2:
            if (m_creature->GetHealthPercent() < 26.0f)
                Mount();
            break;
        case 3:
            return; // Skip combat logic
        }

        if (m_knockdownTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_KNOCKDOWN) ==
                CAST_OK)
                m_knockdownTimer = urand(15000, 25000);
        }
        else
            m_knockdownTimer -= uiDiff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_midnight(Creature* pCreature)
{
    return new boss_midnightAI(pCreature);
}

enum
{
    SAY_APPEAR_1 = -1532001,
    SAY_APPEAR_2 = -1532002,
    SAY_APPEAR_3 = -1532003,
    SAY_KILL_1 = -1532005,
    SAY_KILL_2 = -1532006,
    SAY_DISARMED = -1532007,
    SAY_DEATH = -1532008,
    SAY_RANDOM_1 = -1532009,
    SAY_RANDOM_2 = -1532010,

    SPELL_SHADOW_CLEAVE = 29832,
    SPELL_INTANGIBLE_PRESENCE = 29833,
    SPELL_CHARGE = 29847, // Only when mounted

    SPELL_FIXATE = 34719, // Triggered by SPELL_CHARGE
};

void AttumenDisarm(Creature* attumen, bool apply)
{
    if (apply)
    {
        attumen->SetStatFloatValue(
            UNIT_FIELD_MINDAMAGE, attumen->GetCreatureInfo()->mindmg * 0.65f);
        attumen->SetStatFloatValue(
            UNIT_FIELD_MAXDAMAGE, attumen->GetCreatureInfo()->maxdmg * 0.65f);
    }
    else
    {
        attumen->SetStatFloatValue(
            UNIT_FIELD_MINDAMAGE, attumen->GetCreatureInfo()->mindmg);
        attumen->SetStatFloatValue(
            UNIT_FIELD_MAXDAMAGE, attumen->GetCreatureInfo()->maxdmg);
    }
}

/***
 Attumen (fake)
***/
struct MANGOS_DLL_DECL boss_attumen_fakeAI : public ScriptedAI
{
    boss_attumen_fakeAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_instance = (instance_karazhan*)pCreature->GetInstanceData();
        Reset();
    }

    instance_karazhan* m_instance;
    uint32 m_cleaveTimer;
    uint32 m_presenceTimer;
    bool m_disarmed;

    void Reset() override
    {
        m_cleaveTimer = urand(10000, 15000);
        m_presenceTimer = urand(20000, 30000);
        m_disarmed = false;
        AttumenDisarm(m_creature, false);
    }

    void JustSummoned(Creature* creature) override
    {
        if (m_creature->getVictim() && creature->AI())
            creature->AI()->AttackStart(m_creature->getVictim());
    }

    void JustReachedHome() override
    {
        if (m_instance)
            m_instance->SetData(TYPE_ATTUMEN, FAIL);
    }

    void Aggro(Unit* /*who*/) override
    {
        switch (urand(1, 3))
        {
        case 1:
            DoScriptText(SAY_APPEAR_1, m_creature);
            break;
        case 2:
            DoScriptText(SAY_APPEAR_2, m_creature);
            break;
        case 3:
            DoScriptText(SAY_APPEAR_3, m_creature);
            break;
        }
    }

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_KILL_1, SAY_KILL_2);
    }

    void SpellHit(Unit* /*pSource*/, const SpellEntry* pSpell) override
    {
        if (pSpell->Mechanic == MECHANIC_DISARM && !m_disarmed)
        {
            DoScriptText(SAY_DISARMED, m_creature);
            AttumenDisarm(m_creature, true);
            m_disarmed = true;
        }
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (!m_instance)
            return;

        if (m_disarmed && !m_creature->HasAuraType(SPELL_AURA_MOD_DISARM))
            AttumenDisarm(m_creature, false);

        if (m_creature->GetHealthPercent() <= 25.0f)
            if (Creature* midnight =
                    m_instance->GetSingleCreatureFromStorage(NPC_MIDNIGHT))
                if (boss_midnightAI* AI =
                        dynamic_cast<boss_midnightAI*>(midnight->AI()))
                    AI->Mount();

        if (m_cleaveTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(
                    m_creature->getVictim(), SPELL_SHADOW_CLEAVE) == CAST_OK)
                m_cleaveTimer = urand(10000, 15000);
        }
        else
            m_cleaveTimer -= uiDiff;

        if (m_presenceTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_INTANGIBLE_PRESENCE) ==
                CAST_OK)
                m_presenceTimer = urand(20000, 30000);
        }
        else
            m_presenceTimer -= uiDiff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_attumen_fakeAI(Creature* pCreature)
{
    return new boss_attumen_fakeAI(pCreature);
}

/***
 Attumen (with midnight; the real one that drops loot)
***/
struct MANGOS_DLL_DECL boss_attumenAI : public ScriptedAI
{
    boss_attumenAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_instance = (instance_karazhan*)pCreature->GetInstanceData();
        Reset();
    }

    instance_karazhan* m_instance;
    uint32 m_knockdownTimer;
    uint32 m_cleaveTimer;
    uint32 m_presenceTimer;
    uint32 m_chargeTimer;
    bool m_disarmed;

    void Reset() override
    {
        m_knockdownTimer = 5000;
        m_cleaveTimer = urand(10000, 20000);
        m_presenceTimer = urand(15000, 20000);
        m_chargeTimer = urand(10000, 20000);
        m_disarmed = false;
        AttumenDisarm(m_creature, false);
    }

    void Aggro(Unit* /*who*/) override
    {
        // Set the health percent of either midnight or fake attumen to the new
        // attumen; whichever is the highest
        if (m_instance)
        {
            Creature* attumen = m_instance->GetSingleCreatureFromStorage(
                NPC_ATTUMEN_THE_HUNTSMAN_FAKE);
            Creature* midnight =
                m_instance->GetSingleCreatureFromStorage(NPC_MIDNIGHT);
            if (attumen && midnight)
            {
                if (attumen->GetHealthPercent() > midnight->GetHealthPercent())
                    m_creature->SetHealthPercent(attumen->GetHealthPercent());
                else
                    m_creature->SetHealthPercent(midnight->GetHealthPercent());

                midnight->SetVisibility(VISIBILITY_OFF);
                midnight->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
                midnight->CastSpell(midnight, SPELL_SELF_STUN,
                    true); // We do NOT want to kill midnight (since trash is
                           // linked to him)
                attumen->ForcedDespawn();
            }
        }
    }

    void JustReachedHome() override
    {
        if (m_instance)
            m_instance->SetData(TYPE_ATTUMEN, FAIL);
    }

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_KILL_1, SAY_KILL_2);
    }

    void SpellHit(Unit* /*pSource*/, const SpellEntry* pSpell) override
    {
        if (pSpell->Mechanic == MECHANIC_DISARM && !m_disarmed)
        {
            DoScriptText(SAY_DISARMED, m_creature);
            AttumenDisarm(m_creature, true);
            m_disarmed = true;
        }
    }

    void JustDied(Unit* /*pVictim*/) override
    {
        DoScriptText(SAY_DEATH, m_creature);
        if (m_instance)
            m_instance->SetData(TYPE_ATTUMEN, DONE);
    }

    void OnWhiteHit(Unit* target, WeaponAttackType, uint32, uint32) override
    {
        target->remove_auras(SPELL_FIXATE);
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (m_disarmed && !m_creature->HasAuraType(SPELL_AURA_MOD_DISARM))
            AttumenDisarm(m_creature, false);

        if (m_knockdownTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_KNOCKDOWN) ==
                CAST_OK)
                m_knockdownTimer = urand(20000, 40000);
        }
        else
            m_knockdownTimer -= uiDiff;

        if (m_cleaveTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(
                    m_creature->getVictim(), SPELL_SHADOW_CLEAVE) == CAST_OK)
                m_cleaveTimer = urand(10000, 20000);
        }
        else
            m_cleaveTimer -= uiDiff;

        if (m_presenceTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_INTANGIBLE_PRESENCE) ==
                CAST_OK)
                m_presenceTimer = urand(30000, 40000);
        }
        else
            m_presenceTimer -= uiDiff;

        if (m_chargeTimer <= uiDiff)
        {
            if (Unit* target = m_creature->SelectAttackingTarget(
                    ATTACKING_TARGET_RANDOM, 0, SPELL_CHARGE))
                if (DoCastSpellIfCan(target, SPELL_CHARGE) == CAST_OK)
                    m_chargeTimer = urand(10000, 20000);
        }
        else
            m_chargeTimer -= uiDiff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_attumen(Creature* pCreature)
{
    return new boss_attumenAI(pCreature);
}

void AddSC_boss_attumen()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_attumen";
    pNewScript->GetAI = &GetAI_boss_attumen;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "boss_attumen_fake";
    pNewScript->GetAI = &GetAI_boss_attumen_fakeAI;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "boss_midnight";
    pNewScript->GetAI = &GetAI_boss_midnight;
    pNewScript->RegisterSelf();
}
