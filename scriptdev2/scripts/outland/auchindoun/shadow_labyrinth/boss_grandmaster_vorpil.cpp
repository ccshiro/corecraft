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
SDName: Boss_Grandmaster_Vorpil
SD%Complete: 100
SDComment:
SDCategory: Auchindoun, Shadow Labyrinth
EndScriptData */

#include "precompiled.h"
#include "shadow_labyrinth.h"

enum
{
    SAY_INTRO = -1555028,
    SAY_AGGRO_1 = -1555029,
    SAY_AGGRO_2 = -1555030,
    SAY_AGGRO_3 = -1555031,
    SAY_HELP = -1555032,
    SAY_KILL_1 = -1555033,
    SAY_KILL_2 = -1555034,
    SAY_DEATH = -1555035,

    SPELL_DRAW_SHADOWS = 33563,  // should trigger spell 33558 which is missing;
                                 // so we need to hack the teleport
    SPELL_VOID_PORTAL_A = 33566, // spell only summon one unit, but we use it
                                 // for the visual effect and summon the 4 other
                                 // portals manual way (only one spell exist)
    SPELL_SHADOW_BOLT_VOLLEY = 32963,
    SPELL_RAIN_OF_FIRE = 33617,
    SPELL_RAIN_OF_FIRE_H = 39363,
    SPELL_BANISH_H = 38791,
    SPELL_SUMMON_VOIDWALKER_A = 33582, // the void travelers are summond at
                                       // portal locations according to DB
                                       // coords
    SPELL_SUMMON_VOIDWALKER_B = 33583,
    SPELL_SUMMON_VOIDWALKER_C = 33584,
    SPELL_SUMMON_VOIDWALKER_D = 33585,
    SPELL_SUMMON_VOIDWALKER_E = 33586,

    SPELL_VOID_PORTAL_VISUAL = 33569,

    SPELL_EMPOWERING_SHADOWS = 33783,
    SPELL_EMPOWERING_SHADOWS_H = 39364,
    SPELL_SHADOW_NOVA = 33846,

    NPC_VOID_PORTAL = 19224,
    NPC_VOID_TRAVELER = 19226,

    MAX_PORTALS = 5
};

// Summon locations for the void portals
const float aVorpilLocation[MAX_PORTALS][3] = {
    {-208.6f, -263.5f, 17.1f}, // Not used for portal summoning;
                               // SPELL_VOID_PORTAL_A summons this one
    {-262.3f, -297.6f, 17.1f}, {-291.8f, -269.3f, 12.7f},
    {-306.4f, -255.9f, 12.7f}, {-284.4f, -240.0f, 12.7f}};

static const float aVorpilTeleportLoc[4] = {-253.06f, -264.02f, 17.08f, 3.14f};

static const uint32 aTravelerSummonSpells[MAX_PORTALS] = {
    SPELL_SUMMON_VOIDWALKER_A, SPELL_SUMMON_VOIDWALKER_B,
    SPELL_SUMMON_VOIDWALKER_C, SPELL_SUMMON_VOIDWALKER_D,
    SPELL_SUMMON_VOIDWALKER_E};

struct MANGOS_DLL_DECL boss_grandmaster_vorpilAI : public ScriptedAI
{
    boss_grandmaster_vorpilAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_pInstance = (ScriptedInstance*)pCreature->GetInstanceData();
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();
        m_bHasDoneIntro = false;
        Reset();
    }

    ScriptedInstance* m_pInstance;
    bool m_bIsRegularMode;

    uint32 m_uiShadowBoltVolleyTimer;
    uint32 m_uiDrawShadowsTimer;
    uint32 m_uiRainOfFireTimer;
    uint32 m_uiTeleportTimer;
    uint32 m_uiVoidTravelerTimer;
    uint32 m_uiBanishTimer;
    std::vector<ObjectGuid> m_spawns;
    float m_uiSummonDecayTimer;

    // RP Timers
    bool m_bHasDoneIntro;
    uint32 m_uiEmoteTimer;
    uint32 m_uiEmoteIndex;

    void Reset() override
    {
        m_uiShadowBoltVolleyTimer = 15000;
        m_uiDrawShadowsTimer = urand(35000, 40000);
        m_uiRainOfFireTimer = 0;
        m_uiTeleportTimer = 0;
        m_uiVoidTravelerTimer = 5000;
        m_uiBanishTimer = urand(10000, 15000);
        m_uiSummonDecayTimer = 8.0f;
        m_uiEmoteTimer = 3000;
        m_uiEmoteIndex = 1;
    }

    void MoveInLineOfSight(Unit* pWho) override
    {
        if (!m_bHasDoneIntro && pWho->GetTypeId() == TYPEID_PLAYER &&
            pWho->IsWithinDistInMap(m_creature, 70.0f) &&
            pWho->IsWithinWmoLOSInMap(m_creature))
        {
            DoScriptText(SAY_INTRO, m_creature);
            m_bHasDoneIntro = true;
        }

        ScriptedAI::MoveInLineOfSight(pWho);
    }

    void DespawnSummons()
    {
        for (auto& elem : m_spawns)
            if (Creature* sum = m_creature->GetMap()->GetCreature(elem))
                sum->ForcedDespawn();
        m_spawns.clear();
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

        DoCastSpellIfCan(m_creature, SPELL_VOID_PORTAL_A);

        // summon the other 4 portals
        for (uint8 i = 1; i < MAX_PORTALS; ++i)
            m_creature->SummonCreature(NPC_VOID_PORTAL, aVorpilLocation[i][0],
                aVorpilLocation[i][1], aVorpilLocation[i][2], 0.0f,
                TEMPSUMMON_CORPSE_DESPAWN, 0);

        if (m_pInstance)
            m_pInstance->SetData(TYPE_VORPIL, IN_PROGRESS);
    }

    void JustReachedHome() override
    {
        if (m_pInstance)
            m_pInstance->SetData(TYPE_VORPIL, FAIL);
        DespawnSummons();
    }

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_KILL_1, SAY_KILL_2);
    }

    void JustSummoned(Creature* pSummoned) override
    {
        switch (pSummoned->GetEntry())
        {
        case NPC_VOID_TRAVELER:
            pSummoned->movement_gens.push(new movement::ChaseMovementGenerator(
                m_creature->GetObjectGuid()));
            break;
        case NPC_VOID_PORTAL:
            pSummoned->CastSpell(pSummoned, SPELL_VOID_PORTAL_VISUAL, true);
            break;

        default:
            return;
        }

        m_spawns.push_back(pSummoned->GetObjectGuid());
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        DoScriptText(SAY_DEATH, m_creature);

        if (m_pInstance)
            m_pInstance->SetData(TYPE_VORPIL, DONE);
        DespawnSummons();
    }

    // Wrapper to teleport all players to the platform - Workaround for missing
    // spell
    void DoTeleportToPlatform()
    {
        m_creature->NearTeleportTo(aVorpilTeleportLoc[0], aVorpilTeleportLoc[1],
            aVorpilTeleportLoc[2], aVorpilTeleportLoc[3]);

        GUIDVector vGuids;
        m_creature->FillGuidsListFromThreatList(vGuids);
        for (GUIDVector::const_iterator itr = vGuids.begin();
             itr != vGuids.end(); ++itr)
        {
            Unit* pTarget = m_creature->GetMap()->GetUnit(*itr);

            if (pTarget && pTarget->GetTypeId() == TYPEID_PLAYER)
                DoTeleportPlayer(pTarget, aVorpilTeleportLoc[0],
                    aVorpilTeleportLoc[1], aVorpilTeleportLoc[2],
                    aVorpilTeleportLoc[3]);
        }
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        // Return since we have no target
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
        {
            if (m_uiEmoteTimer <= uiDiff)
            {
                switch (m_uiEmoteIndex)
                {
                case 1:
                    m_creature->HandleEmote(EMOTE_ONESHOT_ROAR);
                    break;
                case 2:
                    m_creature->HandleEmote(EMOTE_ONESHOT_EXCLAMATION);
                    break;
                case 3:
                    m_creature->HandleEmote(EMOTE_ONESHOT_NO);
                    m_uiEmoteIndex = 0;
                    break;
                }
                m_uiEmoteTimer = 3000;
                ++m_uiEmoteIndex;
            }
            else
                m_uiEmoteTimer -= uiDiff;
            return;
        }

        if (m_uiRainOfFireTimer)
        {
            if (m_uiRainOfFireTimer <= uiDiff)
            {
                if (DoCastSpellIfCan(m_creature,
                        m_bIsRegularMode ? SPELL_RAIN_OF_FIRE :
                                           SPELL_RAIN_OF_FIRE_H) == CAST_OK)
                    m_uiRainOfFireTimer = 0;
            }
            else
                m_uiRainOfFireTimer -= uiDiff;
        }

        if (m_uiTeleportTimer)
        {
            if (m_uiTeleportTimer <= uiDiff)
            {
                DoTeleportToPlatform();
                m_uiTeleportTimer = 0;
                m_uiRainOfFireTimer = 1;
            }
            else
                m_uiTeleportTimer -= uiDiff;
        }

        if (m_uiShadowBoltVolleyTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_SHADOW_BOLT_VOLLEY) ==
                CAST_OK)
                m_uiShadowBoltVolleyTimer = urand(20000, 25000);
        }
        else
            m_uiShadowBoltVolleyTimer -= uiDiff;

        if (m_uiDrawShadowsTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_DRAW_SHADOWS) == CAST_OK)
            {
                m_uiDrawShadowsTimer = urand(35000, 40000);
                m_uiTeleportTimer = 1000;
            }
        }
        else
            m_uiDrawShadowsTimer -= uiDiff;

        if (m_uiVoidTravelerTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature,
                    aTravelerSummonSpells[urand(0, MAX_PORTALS)]) == CAST_OK)
            {
                if (m_uiSummonDecayTimer > 0)
                    m_uiSummonDecayTimer -= 0.5f;
                m_uiVoidTravelerTimer =
                    urand(1000, 6000) + m_uiSummonDecayTimer * 1000;
            }
        }
        else
            m_uiVoidTravelerTimer -= uiDiff;

        if (!m_bIsRegularMode)
        {
            if (m_uiBanishTimer <= uiDiff)
            {
                if (Unit* pTarget = m_creature->SelectAttackingTarget(
                        ATTACKING_TARGET_RANDOM, 1))
                {
                    if (DoCastSpellIfCan(pTarget, SPELL_BANISH_H) == CAST_OK)
                        m_uiBanishTimer = urand(15000, 20000);
                }
            }
            else
                m_uiBanishTimer -= uiDiff;
        }

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_grandmaster_vorpil(Creature* pCreature)
{
    return new boss_grandmaster_vorpilAI(pCreature);
}

struct MANGOS_DLL_DECL npc_void_travelerAI : public ScriptedAI
{
    npc_void_travelerAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();
        Reset();
    }

    ObjectGuid m_summoner;
    uint32 m_uiDeathTimer;
    bool m_bIsRegularMode;
    bool m_bHasExploded;

    void Reset() override
    {
        m_uiDeathTimer = 0;
        m_bHasExploded = false;
    }

    void AttackStart(Unit* /*pWho*/) override {}

    void SummonedBy(WorldObject* obj) override
    {
        m_summoner = obj->GetObjectGuid();
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (m_uiDeathTimer)
        {
            if (m_uiDeathTimer <= uiDiff)
            {
                m_creature->Kill(m_creature, false);
                m_uiDeathTimer = 0;
            }
            else
                m_uiDeathTimer -= uiDiff;
        }

        auto summoner = m_creature->GetMap()->GetCreature(m_summoner);
        if (!m_bHasExploded && summoner &&
            summoner->IsWithinDistInMap(m_creature, 3.0f))
        {
            if (DoCastSpellIfCan(m_creature, SPELL_SHADOW_NOVA) == CAST_OK)
            {
                summoner->CastSpell(summoner,
                    m_bIsRegularMode ? SPELL_EMPOWERING_SHADOWS :
                                       SPELL_EMPOWERING_SHADOWS_H,
                    true);
                m_bHasExploded = true;
                m_uiDeathTimer = 1000;
            }
        }
    }
};

CreatureAI* GetAI_npc_void_traveler(Creature* pCreature)
{
    return new npc_void_travelerAI(pCreature);
}

void AddSC_boss_grandmaster_vorpil()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_grandmaster_vorpil";
    pNewScript->GetAI = &GetAI_boss_grandmaster_vorpil;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_void_traveler";
    pNewScript->GetAI = &GetAI_npc_void_traveler;
    pNewScript->RegisterSelf();
}
