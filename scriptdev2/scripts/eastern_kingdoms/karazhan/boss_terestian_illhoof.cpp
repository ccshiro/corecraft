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
SDName: Boss_Terestian_Illhoof
SD%Complete: 100
SDComment:
SDCategory: Karazhan
EndScriptData */

#include "karazhan.h"
#include "precompiled.h"
#include "maps/checks.h"
#include "maps/visitors.h"

enum
{
    SAY_KILL_1 = -1532065,
    SAY_KILL_2 = -1532066,
    SAY_DEATH = -1532067,
    SAY_AGGRO = -1532068,
    SAY_SACRIFICE_1 = -1532069,
    SAY_SACRIFICE_2 = -1532070,
    SAY_SUMMON_1 = -1532071,
    SAY_SUMMON_2 = -1532072,
    SAY_IMP_SHRIEK = -1538000,

    SPELL_SUMMON_DEMONCHAINS =
        30120, // Summons demonic chains that maintain the ritual of sacrifice.
    SPELL_DEMON_CHAINS = 30206, // Instant - Visual Effect
    SPELL_SHADOW_BOLT = 30055,  // Hurls a bolt of dark magic at an enemy,
                                // inflicting Shadow damage.
    SPELL_SACRIFICE = 30115,    // Teleports and adds the debuff
    SPELL_BERSERK = 32965, // Increases attack speed by 75%. Periodically casts
                           // Shadow Bolt Volley.

    SPELL_SUMMON_IMP = 30066, // Summons Kil'rek

    SPELL_SUMMON_FIENDISH_IMP = 30184,
    SPELL_FIENDISH_PORTAL =
        30171, // Opens portal and summons Fiendish Portal, 2 sec cast
    SPELL_FIENDISH_PORTAL_1 =
        30179, // Opens portal and summons Fiendish Portal, instant cast

    SPELL_FIREBOLT = 30050, // Imps fireball spell

    SPELL_BROKEN_PACT = 30065, // All damage taken increased by 25%.
    SPELL_AMPLIFY_FLAMES =
        30053, // Increases the Fire damage taken by an enemy by 500 for 25 sec.

    NPC_DEMON_CHAINS = 17248,
    NPC_FIENDISH_IMP = 17267,
    NPC_PORTAL = 17265,
    NPC_KILREK = 17229
};

struct MANGOS_DLL_DECL mob_demon_chainAI : public ScriptedAI
{
    mob_demon_chainAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        seen_sacrifice = false;
    }

    ObjectGuid m_sacrificeGuid;
    bool seen_sacrifice;

    void Reset() override {}

    void AttackStart(Unit* /*pWho*/) override {}
    void MoveInLineOfSight(Unit* /*pWho*/) override {}

    void JustDied(Unit* /*pKiller*/) override
    {
        if (m_sacrificeGuid)
        {
            if (Player* pSacrifice =
                    m_creature->GetMap()->GetPlayer(m_sacrificeGuid))
                pSacrifice->remove_auras(SPELL_SACRIFICE);
        }
    }

    void UpdateAI(const uint32) override
    {
        bool despawn = true;
        if (auto target = m_creature->GetMap()->GetPlayer(m_sacrificeGuid))
        {
            auto has = target->has_aura(SPELL_SACRIFICE);
            if (has || !seen_sacrifice)
                despawn = false;
            if (has && !seen_sacrifice)
                seen_sacrifice = true;
        }

        if (despawn)
            m_creature->ForcedDespawn();
    }
};

struct MANGOS_DLL_DECL boss_terestianAI : public ScriptedAI
{
    boss_terestianAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_instance = pCreature->GetMapId() == KARAZHAN_MAP_ID ?
                         (instance_karazhan*)pCreature->GetInstanceData() :
                         NULL;
        Reset();
    }

    instance_karazhan* m_instance;
    ObjectGuid m_portalGuid[2];
    uint32 m_currentPortal;
    uint32 m_impSummoningTimer;
    uint32 m_summonKilrekTimer;
    uint32 m_sacrificeTimer;
    uint32 m_shadowboltTimer;
    uint32 m_portalTimer;
    uint32 m_berserkTimer;

    void DespawnStuff()
    {
        for (auto& elem : m_portalGuid)
        {
            if (elem)
            {
                if (Creature* portal = m_creature->GetMap()->GetCreature(elem))
                    portal->ForcedDespawn();
                elem.Clear();
            }
        }
        if (Creature* kilrek = m_creature->GetPet())
            kilrek->ForcedDespawn();
    }

    void Reset() override
    {
        m_summonKilrekTimer = 5000;
        m_sacrificeTimer = urand(20000, 25000);
        m_shadowboltTimer = urand(5000, 15000);
        m_portalTimer = 10000;
        m_berserkTimer = 10 * 60 * 1000;
        m_currentPortal = 1;
        m_impSummoningTimer = 0;

        DespawnStuff();
    }

    void Aggro(Unit* /*pWho*/) override
    {
        DoScriptText(SAY_AGGRO, m_creature);

        if (m_instance)
            m_instance->SetData(TYPE_TERESTIAN, IN_PROGRESS);
    }

    void JustReachedHome() override
    {
        if (m_instance)
            m_instance->SetData(TYPE_TERESTIAN, FAIL);
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        DoScriptText(SAY_DEATH, m_creature);

        DespawnStuff();

        if (m_instance)
            m_instance->SetData(TYPE_TERESTIAN, DONE);
    }

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_KILL_1, SAY_KILL_2);
    }

    void JustSummoned(Creature* pSummoned) override
    {
        switch (pSummoned->GetEntry())
        {
        case NPC_PORTAL:
            if (m_portalGuid[0])
                m_portalGuid[1] = pSummoned->GetObjectGuid();
            else
            {
                m_portalGuid[0] = pSummoned->GetObjectGuid();
                DoCastSpellIfCan(
                    m_creature, SPELL_FIENDISH_PORTAL_1, CAST_TRIGGERED);
            }
            break;
        case NPC_FIENDISH_IMP:
        {
            pSummoned->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PASSIVE);
            pSummoned->movement_gens.remove_all(movement::gen::idle);
            auto pos = m_creature->GetPoint(
                frand(0.0f, 2 * M_PI_F), frand(1.0f, 5.0f));
            pSummoned->movement_gens.push(
                new movement::PointMovementGenerator(
                    0, pos.x, pos.y, pos.z, true, true),
                movement::EVENT_ENTER_COMBAT);
            break;
        }
        case NPC_KILREK:
            if (m_creature->getVictim() && pSummoned->AI())
                pSummoned->AI()->AttackStart(m_creature->getVictim());
            break;
        }
    }

    void SummonedMovementInform(Creature* c, movement::gen gen, uint32) override
    {
        if (c->GetEntry() == NPC_FIENDISH_IMP && gen == movement::gen::point)
        {
            c->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PASSIVE);
            if (auto player = GetClosestPlayer(c, 100.0f))
                c->AI()->AttackStart(player);
        }
    }

    void SummonedCreatureJustDied(Creature* pSummoned) override
    {
        switch (pSummoned->GetEntry())
        {
        case NPC_KILREK:
            DoScriptText(SAY_IMP_SHRIEK, pSummoned);
            DoCastSpellIfCan(m_creature, SPELL_BROKEN_PACT, CAST_TRIGGERED);
            m_summonKilrekTimer = 31000;
            break;
        }
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (m_summonKilrekTimer)
        {
            if (m_summonKilrekTimer <= uiDiff)
            {
                if (DoCastSpellIfCan(m_creature, SPELL_SUMMON_IMP) == CAST_OK)
                {
                    m_creature->remove_auras(
                        SPELL_BROKEN_PACT); // Remove Broken Pact
                    m_summonKilrekTimer = 0;
                }
            }
            else
                m_summonKilrekTimer -= uiDiff;
        }

        if (m_portalTimer)
        {
            if (m_portalTimer <= uiDiff)
            {
                if (DoCastSpellIfCan(m_creature, SPELL_FIENDISH_PORTAL,
                        CAST_INTERRUPT_PREVIOUS) == CAST_OK)
                {
                    DoScriptText(
                        urand(0, 1) ? SAY_SUMMON_1 : SAY_SUMMON_2, m_creature);
                    m_portalTimer = 0;
                    m_impSummoningTimer = 3000;
                }
            }
            else
                m_portalTimer -= uiDiff;
        }

        if (m_impSummoningTimer)
        {
            if (m_impSummoningTimer <= uiDiff)
            {
                m_impSummoningTimer = urand(1000, 8000);
                auto set =
                    maps::visitors::yield_set<TemporarySummon>{}(m_creature,
                        100.0f, maps::checks::entry_guid{
                                    NPC_FIENDISH_IMP, 0, nullptr, true});
                Creature* portal = m_creature->GetMap()->GetCreature(
                    m_portalGuid[m_currentPortal - 1]);
                if (set.size() < 20 && portal)
                {
                    m_creature->SummonCreature(NPC_FIENDISH_IMP, portal->GetX(),
                        portal->GetY(), portal->GetZ(),
                        portal->GetAngle(m_creature),
                        TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,
                        20 * IN_MILLISECONDS);
                }
                // 33% chance to switch portal and do a summon right away
                if (urand(1, 3) == 1)
                {
                    m_currentPortal = m_currentPortal == 1 ? 2 : 1;
                    m_impSummoningTimer = urand(500, 1500);
                }
            }
            else
                m_impSummoningTimer -= uiDiff;
        }

        if (m_sacrificeTimer <= uiDiff)
        {
            if (Unit* pTarget = m_creature->SelectAttackingTarget(
                    ATTACKING_TARGET_RANDOM, 0, SPELL_SACRIFICE))
            {
                if (DoCastSpellIfCan(
                        pTarget, SPELL_SACRIFICE, CAST_TRIGGERED) == CAST_OK)
                {
                    DoScriptText(
                        urand(0, 1) ? SAY_SACRIFICE_1 : SAY_SACRIFICE_2,
                        m_creature);

                    if (Creature* pChains = m_creature->SummonCreature(
                            NPC_DEMON_CHAINS, -11233.8f, -1698.3f, 179.237f, 0,
                            TEMPSUMMON_TIMED_OR_CORPSE_DESPAWN, 35000))
                    {
                        if (mob_demon_chainAI* pDemonAI =
                                dynamic_cast<mob_demon_chainAI*>(pChains->AI()))
                            pDemonAI->m_sacrificeGuid =
                                pTarget->GetObjectGuid();

                        pChains->CastSpell(pChains, SPELL_DEMON_CHAINS, true);
                    }
                    m_sacrificeTimer = urand(40000, 50000);
                }
            }
        }
        else
            m_sacrificeTimer -= uiDiff;

        if (m_shadowboltTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_SHADOW_BOLT) ==
                CAST_OK)
                m_shadowboltTimer = urand(5000, 15000);
        }
        else
            m_shadowboltTimer -= uiDiff;

        if (m_berserkTimer)
        {
            if (m_berserkTimer <= uiDiff)
            {
                if (DoCastSpellIfCan(m_creature, SPELL_BERSERK,
                        CAST_INTERRUPT_PREVIOUS) == CAST_OK)
                    m_berserkTimer = 0;
            }
            else
                m_berserkTimer -= uiDiff;
        }

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_mob_demon_chain(Creature* pCreature)
{
    return new mob_demon_chainAI(pCreature);
}

CreatureAI* GetAI_boss_terestian_illhoof(Creature* pCreature)
{
    return new boss_terestianAI(pCreature);
}

void AddSC_boss_terestian_illhoof()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_terestian_illhoof";
    pNewScript->GetAI = &GetAI_boss_terestian_illhoof;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "mob_demon_chain";
    pNewScript->GetAI = &GetAI_mob_demon_chain;
    pNewScript->RegisterSelf();
}
