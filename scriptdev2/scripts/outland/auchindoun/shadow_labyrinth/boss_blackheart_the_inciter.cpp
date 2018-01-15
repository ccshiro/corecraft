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
SDName: Boss_Blackheart_the_Inciter
SD%Complete: 100
SDComment:
SDCategory: Auchindoun, Shadow Labyrinth
EndScriptData */

#include "precompiled.h"
#include "shadow_labyrinth.h"

enum
{
    SPELL_INCITE_CHAOS = 33676,
    SPELL_INCITE_CHAOS_CHARM = 33684,
    SPELL_CHARGE = 33709,
    SPELL_WAR_STOMP = 33707,

    SAY_INTRO1 = -1555008,
    SAY_INTRO2 = -1555009,
    SAY_INTRO3 = -1555010,
    SAY_AGGRO1 = -1555011,
    SAY_INCITE_CHAOS =
        -1555012, // Used as an aggro say in current retail, was MC say in TBC
    SAY_AGGRO2 = -1555013,
    SAY_KILL_1 = -1555014,
    SAY_KILL_2 = -1555015,
    SAY_HELP = -1555016,
    SAY_DEATH = -1555017,

    SAY2_INTRO1 = -1555018,
    SAY2_INTRO2 = -1555019,
    SAY2_INTRO3 = -1555020,
    SAY2_AGGRO1 = -1555021,
    SAY2_AGGRO2 = -1555022,
    SAY2_AGGRO3 = -1555023,
    SAY2_KILL_1 = -1555024,
    SAY2_KILL_2 = -1555025,
    SAY2_HELP = -1555026,
    SAY2_DEATH = -1555027,

    LEFT_GROUP = 0x1,
    MIDDLE_GROUP = 0x2,
    RIGHT_GROUP = 0x4,
};

#define MAX_ENTRIES 7
uint32 eventEntries[MAX_ENTRIES] = {
    18631, 18633, 18635, 18637, 18640, 18641, 18642,
};

struct MANGOS_DLL_DECL boss_blackheart_the_inciterAI : public ScriptedAI
{
    boss_blackheart_the_inciterAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_pInstance = (ScriptedInstance*)pCreature->GetInstanceData();
        Reset();
    }

    ScriptedInstance* m_pInstance;

    uint32 m_uiInciteChaosTimer;
    uint32 m_uiChaosUpTimer;
    uint32 m_uiLastLaugh;
    uint32 m_uiChargeTimer;
    uint32 m_uiWarstompTimer;

    void Reset() override
    {
        m_uiInciteChaosTimer = urand(23000, 25000);
        m_uiChaosUpTimer = 0;
        m_uiLastLaugh = 0;
        m_uiChargeTimer = urand(30000, 35000);
        m_uiWarstompTimer = urand(18000, 22000);
    }

    /* Out of Combat RP START */
    void DoGroupEmotes(uint32 groupMask, uint32 emote)
    {
        auto v = GetFriendlyCreatureListInGrid(m_creature, 150.0f);
        for (auto c : v)
        {
            if (!c->isAlive() || c->isInCombat())
                continue;
            if (std::find(eventEntries, eventEntries + MAX_ENTRIES,
                    c->GetEntry()) == eventEntries + MAX_ENTRIES)
                continue;
            if (!((groupMask & LEFT_GROUP &&
                      c->IsWithinDist2d(-264.7f, 3.3f, 15.0f)) ||
                    (groupMask & MIDDLE_GROUP &&
                        c->IsWithinDist2d(-257.6f, -40.0f, 20.0f)) ||
                    (groupMask & RIGHT_GROUP &&
                        c->IsWithinDist2d(-270.6f, -79.2f, 15.0f))))
                continue;
            c->HandleEmote(emote);
        }
    }

    void MovementInform(movement::gen uiType, uint32 uiPoint) override
    {
        if (uiType != movement::gen::waypoint)
            return;
        switch (uiPoint)
        {
        case 1:
            m_creature->HandleEmote(EMOTE_ONESHOT_ROAR);
            DoGroupEmotes(LEFT_GROUP,
                urand(0, 1) ? EMOTE_ONESHOT_BOW : EMOTE_ONESHOT_SALUTE);
            break;
        case 6:
            m_creature->HandleEmote(EMOTE_ONESHOT_ROAR);
            DoGroupEmotes(RIGHT_GROUP,
                urand(0, 1) ? EMOTE_ONESHOT_BOW : EMOTE_ONESHOT_SALUTE);
            break;
        case 11:
            m_creature->HandleEmote(EMOTE_ONESHOT_ROAR);
            DoGroupEmotes(MIDDLE_GROUP,
                urand(0, 1) ? EMOTE_ONESHOT_BOW : EMOTE_ONESHOT_SALUTE);
            break;
        case 3:
        case 8:
        case 13:
            m_creature->HandleEmote(EMOTE_ONESHOT_LAUGH);
            break;
        case 4:
        case 9:
        case 14:
            m_creature->HandleEmote(EMOTE_ONESHOT_ROAR);
            DoGroupEmotes(
                LEFT_GROUP | MIDDLE_GROUP | RIGHT_GROUP, EMOTE_ONESHOT_BEG);
            break;
        }
    }
    /* Out of Combat RP END */

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_KILL_1, SAY_KILL_2);
    }

    void RemoveInciteChaosMobs()
    {
        if (!m_pInstance)
            return;

        uint32 entry = NPC_INCITER_1;
        for (int i = 0; i < 5; ++i, ++entry)
            if (Creature* c = m_pInstance->GetSingleCreatureFromStorage(entry))
                c->ForcedDespawn();
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        DoScriptText(SAY_DEATH, m_creature);

        if (m_pInstance)
            m_pInstance->SetData(TYPE_INCITER, DONE);

        RemoveInciteChaosMobs();
    }

    void Aggro(Unit* /*pWho*/) override
    {
        switch (urand(0, 1))
        {
        case 0:
            DoScriptText(SAY_AGGRO1, m_creature);
            break;
        case 1:
            DoScriptText(SAY_AGGRO2, m_creature);
            break;
        }

        if (m_pInstance)
            m_pInstance->SetData(TYPE_INCITER, IN_PROGRESS);
    }

    void EnterEvadeMode(bool by_group = false) override
    {
        RemoveInciteChaosMobs();
        ScriptedAI::EnterEvadeMode(by_group);
    }

    void JustReachedHome() override
    {
        if (m_pInstance)
            m_pInstance->SetData(TYPE_INCITER, FAIL);
    }

    void MoveInLineOfSight(Unit* pWho) override
    {
        if (m_uiChaosUpTimer)
            return;
        ScriptedAI::MoveInLineOfSight(pWho);
    }

    void AttackStart(Unit* who) override
    {
        if (m_uiChaosUpTimer)
            return;
        ScriptedAI::AttackStart(who);
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (m_uiChaosUpTimer)
        {
            if (m_uiChaosUpTimer <= uiDiff)
            {
                m_uiChaosUpTimer = 0;
                m_uiLastLaugh = 0;

                m_uiInciteChaosTimer = urand(35000, 45000);
                m_uiChargeTimer = urand(4000, 14000);
                m_uiWarstompTimer = urand(4000, 8000);

                RemoveInciteChaosMobs();
            }
            else
            {
                m_creature->SetTargetGuid(ObjectGuid());

                if (m_uiLastLaugh - m_uiChaosUpTimer >= 3000)
                {
                    m_creature->HandleEmote(EMOTE_ONESHOT_LAUGH);
                    m_uiLastLaugh = m_uiChaosUpTimer;
                }
                m_uiChaosUpTimer -= uiDiff;
                return;
            }
        }

        // Return since we have no pTarget
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (m_creature->movement_gens.has(movement::gen::stopped))
        {
            SetCombatMovement(true);
            m_creature->addUnitState(UNIT_STAT_MELEE_ATTACKING);
            m_creature->SendMeleeAttackStart(m_creature->getVictim());
            m_creature->SetTargetGuid(m_creature->getVictim()->GetObjectGuid());
            m_creature->movement_gens.remove_all(movement::gen::stopped);
        }

        if (m_uiInciteChaosTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_INCITE_CHAOS) == CAST_OK)
            {
                DoScriptText(SAY_INCITE_CHAOS, m_creature);
                if (m_pInstance)
                {
                    std::vector<Unit*> targets;

                    const ThreatList& tl =
                        m_creature->getThreatManager().getThreatList();
                    for (const auto& elem : tl)
                    {
                        if (targets.size() >= 5)
                            break;

                        Unit* u = m_creature->GetMap()->GetUnit(
                            (elem)->getUnitGuid());
                        if (!u || u->GetTypeId() != TYPEID_PLAYER ||
                            !u->isAlive())
                            continue;
                        targets.push_back(u);
                    }
                    m_uiChaosUpTimer = 15500;
                    m_uiLastLaugh = 15500;

                    // Need to do this after we iterate the threat list, as it
                    // will change due to these actions
                    uint32 entry = NPC_INCITER_1;
                    for (auto u : targets)
                    {
                        if (Creature* c = u->SummonCreature(entry, u->GetX(),
                                u->GetY(), u->GetZ(), 0.0f,
                                TEMPSUMMON_TIMED_DESPAWN, 20000))
                        {
                            c->CastSpell(u, SPELL_INCITE_CHAOS_CHARM, true);
                        }
                        ++entry;
                    }

                    SetCombatMovement(false);
                    m_creature->SendMeleeAttackStop(m_creature->getVictim());
                    m_creature->clearUnitState(UNIT_STAT_MELEE_ATTACKING);

                    return; // We must return now as our victims have changed
                            // (we trigger the spell)
                }
            }
        }
        else
            m_uiInciteChaosTimer -= uiDiff;

        // Charge Timer
        if (m_uiChargeTimer <= uiDiff)
        {
            Unit* pTarget = m_creature->SelectAttackingTarget(
                ATTACKING_TARGET_RANDOM, 1, SPELL_CHARGE);
            if (pTarget && DoCastSpellIfCan(pTarget, SPELL_CHARGE) == CAST_OK)
            {
                m_uiChargeTimer = urand(30000, 35000);
                // Drop threat
                DoResetThreat();
                // Delay chaos incite if needed
                if (m_uiInciteChaosTimer < 2500)
                    m_uiInciteChaosTimer = 2500;
            }
        }
        else
            m_uiChargeTimer -= uiDiff;

        // Knockback Timer
        if (m_uiWarstompTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_WAR_STOMP) == CAST_OK)
            {
                m_uiWarstompTimer = urand(18000, 22000);
                if (m_uiInciteChaosTimer < 2500)
                    m_uiInciteChaosTimer = 2500;
            }
        }
        else
            m_uiWarstompTimer -= uiDiff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_blackheart_the_inciter(Creature* pCreature)
{
    return new boss_blackheart_the_inciterAI(pCreature);
}

void AddSC_boss_blackheart_the_inciter()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_blackheart_the_inciter";
    pNewScript->GetAI = &GetAI_boss_blackheart_the_inciter;
    pNewScript->RegisterSelf();
}
