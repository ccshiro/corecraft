/* Copyright (C) 2013 Corecraft <https://www.worldofcorecraft.com/>
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
SDName: boss_thaladred_the_darkener, boss_lord_sanguinar,
    boss_grand_astromancer_capernian, boss_master_engineer_telonicus,
npc_netherstrand_longbow
SD%Complete: 100
SDComment:
SDCategory: Tempest Keep, The Eye
EndScriptData */

/* ContentData
EndContentData */

#include "precompiled.h"
#include "the_eye.h"

enum
{
    SPELL_STUN_SELF = 48342
};

void AdvisorDeathToggle(bool on, Creature* c)
{
    if (on)
    {
        c->CastSpell(c, SPELL_STUN_SELF, true);
        c->SetStandState(UNIT_STAND_STATE_DEAD);
        c->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
        if (instance_the_eye* inst =
                dynamic_cast<instance_the_eye*>(c->GetInstanceData()))
            inst->OnAdvisorDeath(c);
    }
    else
    {
        c->remove_auras(SPELL_STUN_SELF);
        c->SetStandState(UNIT_STAND_STATE_STAND);
        c->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
    }
}

enum
{
    SAY_THALADRED_AGGRO = -1550035,
    SAY_THALADRED_GAZE = -1550037,
    SAY_THALADRED_DEATH = -1550036,

    // SPELL_PSYCHIC_BLOW = 36966,
    // SPELL_SILENCE = 30225,
    // SPELL_REND = 36965,
    SPELL_TELEPORT_TO_PLAYER = 36967,
};

struct boss_thaladred_the_darkenerAI : public Scripted_BehavioralAI
{
    boss_thaladred_the_darkenerAI(Creature* pCreature)
      : Scripted_BehavioralAI(pCreature)
    {
        m_instance = (ScriptedInstance*)pCreature->GetInstanceData();
        Reset();
    }

    ScriptedInstance* m_instance;
    uint32 m_gaze;
    uint32 m_teleport;

    bool m_dead;
    void DamageTaken(Unit* /*pDealer*/, uint32& uiDamage) override
    {
        if (m_instance && uiDamage >= m_creature->GetHealth() &&
            m_instance->GetData(DATA_KAELTHAS_PHASE) < KAEL_PHASE_ALL_ADVISORS)
        {
            uiDamage = m_creature->GetHealth() - 1;

            if (!m_dead)
            {
                AdvisorDeathToggle(true, m_creature);
                m_dead = true;
            }
        }
    }
    void SpellHit(Unit* /*pSource*/, const SpellEntry* pSpell) override
    {
        if (pSpell->Id == SPELL_RES_ADVISORS && m_dead)
        {
            DoResetThreat();
            AdvisorDeathToggle(false, m_creature);
            m_dead = false;
        }
    }

    void Reset() override
    {
        m_creature->SetFocusTarget(nullptr);
        m_creature->SetStandState(UNIT_STAND_STATE_STAND);
        m_creature->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
        m_gaze = 0;
        m_teleport = 0;
        m_dead = false;

        Scripted_BehavioralAI::Reset();
    }

    void Aggro(Unit* /*pWho*/) override
    {
        DoScriptText(SAY_THALADRED_AGGRO, m_creature);
    }

    void JustReachedHome() override
    {
        if (m_instance)
            m_instance->SetData(TYPE_KAELTHAS, FAIL);
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        DoScriptText(SAY_THALADRED_DEATH, m_creature);
    }

    void UpdateAI(const uint32 diff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim() ||
            m_dead)
            return;

        if (m_gaze <= diff)
        {
            if (Unit* tar = m_creature->SelectAttackingTarget(
                    ATTACKING_TARGET_RANDOM, 0))
            {
                m_creature->SetFocusTarget(tar);
                DoScriptText(SAY_THALADRED_GAZE, m_creature, tar);
                m_gaze = urand(10000, 15000);
                if (!m_creature->CanReachWithMeleeAttack(tar))
                    m_teleport = 1000;
            }
        }
        else
            m_gaze -= diff;

        if (m_teleport)
        {
            if (m_teleport <= diff)
            {
                if (DoCastSpellIfCan(m_creature->getVictim(),
                        SPELL_TELEPORT_TO_PLAYER) == CAST_OK)
                    m_teleport = 0;
            }
            else
                m_teleport -= diff;
        }

        Scripted_BehavioralAI::UpdateInCombatAI(diff);

        DoMeleeAttackIfReady();
    }
};

enum
{
    SAY_SANGUINAR_AGGRO = -1550038,
    SAY_SANGUINAR_DEATH = -1550039,

    SPELL_BELLOWING_ROAR = 44863,
    // Has thrash in template_addon (3417)
};

struct boss_lord_sanguinarAI : public ScriptedAI
{
    boss_lord_sanguinarAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_instance = (ScriptedInstance*)pCreature->GetInstanceData();
        Reset();
    }

    ScriptedInstance* m_instance;
    uint32 m_roar;

    bool m_dead;
    void DamageTaken(Unit* /*pDealer*/, uint32& uiDamage) override
    {
        if (m_instance && uiDamage >= m_creature->GetHealth() &&
            m_instance->GetData(DATA_KAELTHAS_PHASE) < KAEL_PHASE_ALL_ADVISORS)
        {
            uiDamage = m_creature->GetHealth() - 1;

            if (!m_dead)
            {
                AdvisorDeathToggle(true, m_creature);
                m_dead = true;
            }
        }
    }
    void SpellHit(Unit* /*pSource*/, const SpellEntry* pSpell) override
    {
        if (pSpell->Id == SPELL_RES_ADVISORS && m_dead)
        {
            DoResetThreat();
            AdvisorDeathToggle(false, m_creature);
            m_dead = false;
        }
    }

    void Reset() override
    {
        m_creature->SetStandState(UNIT_STAND_STATE_STAND);
        m_creature->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
        m_roar = 0;
        m_dead = false;
    }

    void Aggro(Unit* /*pWho*/) override
    {
        DoScriptText(SAY_SANGUINAR_AGGRO, m_creature);
    }

    void JustReachedHome() override
    {
        if (m_instance)
            m_instance->SetData(TYPE_KAELTHAS, FAIL);
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        DoScriptText(SAY_SANGUINAR_DEATH, m_creature);
    }

    void UpdateAI(const uint32 diff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (m_roar <= diff)
        {
            if (m_creature->CanReachWithMeleeAttack(m_creature->getVictim()))
                if (DoCastSpellIfCan(m_creature, SPELL_BELLOWING_ROAR) ==
                    CAST_OK)
                    m_roar = urand(30000, 35000);
        }
        else
            m_roar -= diff;

        DoMeleeAttackIfReady();
    }
};

enum
{
    SAY_CAPERNIAN_AGGRO = -1550040,
    SAY_CAPERNIAN_DEATH = -1550041,

    // SPELL_FIREBALL = 36971,
    // SPELL_SHADOW_BOLT = 36972,
    // SPELL_CONFLAGRATION = 37018,
    // SPELL_VOID_ZONE = 37014,
    // SPELL_ARCANE_BURST = 36970,
};

struct boss_grand_astromancer_capernianAI : public Scripted_BehavioralAI
{
    boss_grand_astromancer_capernianAI(Creature* pCreature)
      : Scripted_BehavioralAI(pCreature)
    {
        m_instance = (ScriptedInstance*)pCreature->GetInstanceData();
        Reset();
    }

    ScriptedInstance* m_instance;

    bool m_dead;
    void DamageTaken(Unit* /*pDealer*/, uint32& uiDamage) override
    {
        if (m_instance && uiDamage >= m_creature->GetHealth() &&
            m_instance->GetData(DATA_KAELTHAS_PHASE) < KAEL_PHASE_ALL_ADVISORS)
        {
            uiDamage = m_creature->GetHealth() - 1;

            if (!m_dead)
            {
                AdvisorDeathToggle(true, m_creature);
                m_dead = true;
            }
        }
    }
    void SpellHit(Unit* /*pSource*/, const SpellEntry* pSpell) override
    {
        if (pSpell->Id == SPELL_RES_ADVISORS && m_dead)
        {
            DoResetThreat();
            AdvisorDeathToggle(false, m_creature);
            m_dead = false;
        }
    }

    void Reset() override
    {
        Scripted_BehavioralAI::Reset();

        m_creature->SetStandState(UNIT_STAND_STATE_STAND);
        m_creature->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
        m_dead = false;
    }

    void Aggro(Unit* /*pWho*/) override
    {
        DoScriptText(SAY_CAPERNIAN_AGGRO, m_creature);
    }

    void JustReachedHome() override
    {
        if (m_instance)
            m_instance->SetData(TYPE_KAELTHAS, FAIL);
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        DoScriptText(SAY_CAPERNIAN_DEATH, m_creature);
    }

    void UpdateAI(const uint32 diff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        Scripted_BehavioralAI::UpdateInCombatAI(diff);

        DoMeleeAttackIfReady();
    }
};

enum
{
    SAY_TELONICUS_AGGRO = -1550042,
    SAY_TELONICUS_DEATH = -1550043,

    // SPELL_REMOTE_TOY = 37027,
    // SPELL_CHAOTIC_TEMPERAMENT = 37030,
    // SPELL_BOMB = 37036,
};

struct boss_master_engineer_telonicusAI : public Scripted_BehavioralAI
{
    boss_master_engineer_telonicusAI(Creature* pCreature)
      : Scripted_BehavioralAI(pCreature)
    {
        m_instance = (ScriptedInstance*)pCreature->GetInstanceData();
        Reset();
    }

    ScriptedInstance* m_instance;

    bool m_dead;
    void DamageTaken(Unit* /*pDealer*/, uint32& uiDamage) override
    {
        if (m_instance && uiDamage >= m_creature->GetHealth() &&
            m_instance->GetData(DATA_KAELTHAS_PHASE) < KAEL_PHASE_ALL_ADVISORS)
        {
            uiDamage = m_creature->GetHealth() - 1;

            if (!m_dead)
            {
                AdvisorDeathToggle(true, m_creature);
                m_dead = true;
            }
        }
    }
    void SpellHit(Unit* /*pSource*/, const SpellEntry* pSpell) override
    {
        if (pSpell->Id == SPELL_RES_ADVISORS && m_dead)
        {
            DoResetThreat();
            AdvisorDeathToggle(false, m_creature);
            m_dead = false;
        }
    }

    void Reset() override
    {
        Scripted_BehavioralAI::Reset();

        m_creature->SetStandState(UNIT_STAND_STATE_STAND);
        m_creature->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
        m_dead = false;
    }

    void Aggro(Unit* /*pWho*/) override
    {
        DoScriptText(SAY_TELONICUS_AGGRO, m_creature);
    }

    void JustReachedHome() override
    {
        if (m_instance)
            m_instance->SetData(TYPE_KAELTHAS, FAIL);
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        DoScriptText(SAY_TELONICUS_DEATH, m_creature);
    }

    void UpdateAI(const uint32 diff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        Scripted_BehavioralAI::UpdateInCombatAI(diff);

        DoMeleeAttackIfReady();
    }
};

enum
{
    SPELL_BLINK = 36994
};

struct npc_netherstrand_longbowAI : public Scripted_BehavioralAI
{
    npc_netherstrand_longbowAI(Creature* pCreature)
      : Scripted_BehavioralAI(pCreature)
    {
        Reset();
    }

    time_t m_lastBlink;

    void Reset() override
    {
        m_lastBlink = 0;
        Scripted_BehavioralAI::Reset();
    }

    void OnTakenWhiteHit(Unit* /*pAttacker*/, WeaponAttackType /*attType*/,
        uint32 /*damage*/, uint32 /*hitInfo*/) override
    {
        time_t now = time(NULL);
        if (m_lastBlink + 10 < now)
            if (DoCastSpellIfCan(m_creature, SPELL_BLINK) == CAST_OK)
                m_lastBlink = now;
    }

    void UpdateAI(const uint32 diff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        Scripted_BehavioralAI::UpdateInCombatAI(diff);

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_thaladred_the_darkener(Creature* pCreature)
{
    return new boss_thaladred_the_darkenerAI(pCreature);
}

CreatureAI* GetAI_boss_lord_sanguinar(Creature* pCreature)
{
    return new boss_lord_sanguinarAI(pCreature);
}

CreatureAI* GetAI_boss_grand_astromancer_capernian(Creature* pCreature)
{
    return new boss_grand_astromancer_capernianAI(pCreature);
}

CreatureAI* GetAI_boss_master_engineer_telonicus(Creature* pCreature)
{
    return new boss_master_engineer_telonicusAI(pCreature);
}

CreatureAI* GetAI_npc_netherstrand_longbow(Creature* pCreature)
{
    return new npc_netherstrand_longbowAI(pCreature);
}

void AddSC_boss_advisors()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_thaladred_the_darkener";
    pNewScript->GetAI = &GetAI_boss_thaladred_the_darkener;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "boss_lord_sanguinar";
    pNewScript->GetAI = &GetAI_boss_lord_sanguinar;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "boss_grand_astromancer_capernian";
    pNewScript->GetAI = &GetAI_boss_grand_astromancer_capernian;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "boss_master_engineer_telonicus";
    pNewScript->GetAI = &GetAI_boss_master_engineer_telonicus;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_netherstrand_longbow";
    pNewScript->GetAI = &GetAI_npc_netherstrand_longbow;
    pNewScript->RegisterSelf();
}
