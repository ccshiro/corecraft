/* Copyright (C) 2012 CoreCraft
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
SDName: Boss_Netherspite
SD%Complete: 100
SDComment:
SDCategory: Karazhan
EndScriptData */

#include "karazhan.h"
#include "precompiled.h"

enum
{
    BANISH_EMOTE = -1532126,
    PORTAL_EMOTE = -1532127,

    // Perseverence: Red, Serenity: Green, Domination: Blue

    // Player beams:
    SPELL_BEAM_RED = 30400,
    SPELL_BEAM_GREEN = 30401,
    SPELL_BEAM_BLUE = 30402,

    // Netherspite beams:
    SPELL_NETHERBEAM_RED = 30465,
    SPELL_NETHERBEAM_GREEN = 30464,
    SPELL_NETHERBEAM_BLUE = 30463,

    SPELL_PORTAL_RED = 30487,
    SPELL_PORTAL_GREEN = 30490,
    SPELL_PORTAL_BLUE = 30491,

    SPELL_PBUFF_RED = 30421,
    SPELL_PBUFF_GREEN = 30422,
    SPELL_PBUFF_BLUE = 30423,

    SPELL_NBUFF_RED = 30466,
    SPELL_NBUFF_GREEN = 30467,
    SPELL_NBUFF_BLUE = 30468,

    SPELL_EXHAUST_RED = 38637,
    SPELL_EXHAUST_GREEN = 38638,
    SPELL_EXHAUST_BLUE = 38639,

    NPC_PORTAL_RED = 17369,
    NPC_PORTAL_GREEN = 17367,
    NPC_PORTAL_BLUE = 17368,

    SPELL_VOID_ZONE = 37063,
    SPELL_CONSUMPTION_TICK = 28865,
    SPELL_NETHERBREATH = 38523,
    SPELL_NETHER_BURN = 30522,
    SPELL_EMPOWERMENT = 38549,
    SPELL_VIS_NETHER_RAGE = 39833,
    SPELL_NETHER_INFUSION = 38688,

    SPELL_NETHERSPITE_ROAR = 38684,

    // Not part of the fight, but useful
    SPELL_INVISIBLE_ROOT = 42716
};

enum NetherspitePhase
{
    PORTAL_PHASE = 1,
    BANISH_PHASE = 2,
};

enum PortalColour
{
    PORTAL_RED,
    PORTAL_GREEN,
    PORTAL_BLUE,
};

#define IN_FRONT_10_F M_PI_F / 18

struct PortalPos
{
    float X, Y, Z;
};

const PortalPos PortalPositions[] = {{-11140.0f, -1680.5f, 278.2f},
    {-11196.0f, -1613.5f, 278.2f}, {-11107.7f, -1602.6f, 279.9f}};

ptrdiff_t netherspite_shufflerng(ptrdiff_t i)
{
    return urand(0, i - 1);
}

struct PortalDebuff
{
    PortalDebuff(ObjectGuid o, uint32 t)
      : target_guid(std::move(o)), time_remaining(t)
    {
    }
    ObjectGuid target_guid;
    uint32 time_remaining;
};

struct MANGOS_DLL_DECL nether_portalAI : public ScriptedAI
{
    nether_portalAI(Creature* pCreature) : ScriptedAI(pCreature) { Reset(); }

    // Disable combat:
    void MoveInLineOfSight(Unit* /*pWho*/) override {}
    void AttackStart(Unit* /*pWho*/) override {}
    void AttackedBy(Unit* /*pAttacker*/) override {}

    PortalColour m_colour;
    bool m_hasActivatedPortalSpell;
    uint32 m_checkTimer;
    ObjectGuid m_netherspiteGuid;
    std::vector<PortalDebuff> m_debuffedTargets;
    uint32 m_activationTimer;
    ObjectGuid m_currentTarget;

    void Reset() override
    {
        m_hasActivatedPortalSpell = false;
        m_checkTimer = 1000;
        m_activationTimer = 10000;
        m_currentTarget = ObjectGuid();
    }

    void StopPortal()
    {
        m_creature->InterruptNonMeleeSpells(false);
        for (auto& elem : m_debuffedTargets)
        {
            if (Player* pPlayer =
                    m_creature->GetMap()->GetPlayer((elem).target_guid))
            {
                switch (m_colour)
                {
                case PORTAL_RED:
                    pPlayer->AddAuraThroughNewHolder(
                        SPELL_EXHAUST_RED, m_creature);
                    pPlayer->remove_auras(
                        SPELL_PBUFF_RED); // Just in case it lingers...
                    m_creature->remove_auras(SPELL_PORTAL_RED);
                    break;
                case PORTAL_GREEN:
                    pPlayer->AddAuraThroughNewHolder(
                        SPELL_EXHAUST_GREEN, m_creature);
                    pPlayer->remove_auras(SPELL_PBUFF_GREEN);
                    m_creature->remove_auras(SPELL_PORTAL_GREEN);
                    break;
                case PORTAL_BLUE:
                    pPlayer->AddAuraThroughNewHolder(
                        SPELL_EXHAUST_BLUE, m_creature);
                    pPlayer->remove_auras(SPELL_PBUFF_BLUE);
                    m_creature->remove_auras(SPELL_PORTAL_BLUE);
                    break;
                }
            }
        }
        m_creature->ForcedDespawn();
        m_netherspiteGuid.Set(0);
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_netherspiteGuid)
            return;

        if (!m_hasActivatedPortalSpell)
        {
            switch (m_colour)
            {
            case PORTAL_RED:
                if (DoCastSpellIfCan(m_creature, SPELL_PORTAL_RED) == CAST_OK)
                    m_hasActivatedPortalSpell = true;
                break;
            case PORTAL_GREEN:
                if (DoCastSpellIfCan(m_creature, SPELL_PORTAL_GREEN) == CAST_OK)
                    m_hasActivatedPortalSpell = true;
                break;
            case PORTAL_BLUE:
                if (DoCastSpellIfCan(m_creature, SPELL_PORTAL_BLUE) == CAST_OK)
                    m_hasActivatedPortalSpell = true;
                break;
            }
            if (!m_hasActivatedPortalSpell)
                return;
        }

        if (m_activationTimer)
        {
            if (m_activationTimer <= uiDiff)
                m_activationTimer = 0;
            else
            {
                m_activationTimer -= uiDiff;
                return;
            }
        }

        Creature* netherspite =
            m_creature->GetMap()->GetCreature(m_netherspiteGuid);
        if (!netherspite)
            return;
        m_creature->SetOrientation(m_creature->GetAngle(netherspite));

        for (
            std::vector<PortalDebuff>::iterator itr = m_debuffedTargets.begin();
            itr != m_debuffedTargets.end();)
        {
            if (itr->time_remaining <= uiDiff)
            {
                Player* plr = m_creature->GetMap()->GetPlayer(itr->target_guid);
                if (plr && plr->isAlive())
                {
                    switch (m_colour)
                    {
                    case PORTAL_RED:
                        plr->AddAuraThroughNewHolder(SPELL_EXHAUST_RED,
                            m_creature); // You cannot avoid the exhaustion
                        plr->remove_auras(
                            SPELL_PBUFF_RED); // Just in case it lingers...
                        plr->UpdateMaxHealth();
                        break;
                    case PORTAL_GREEN:
                        plr->AddAuraThroughNewHolder(
                            SPELL_EXHAUST_GREEN, m_creature);
                        plr->remove_auras(SPELL_PBUFF_GREEN);
                        break;
                    case PORTAL_BLUE:
                        plr->AddAuraThroughNewHolder(
                            SPELL_EXHAUST_BLUE, m_creature);
                        plr->remove_auras(SPELL_PBUFF_BLUE);
                        break;
                    }
                }
                itr = m_debuffedTargets.erase(itr);
            }
            else
            {
                itr->time_remaining -= uiDiff;
                ++itr;
            }
        }

        if (m_checkTimer <= uiDiff)
        {
            float distNether = m_creature->GetDistance(
                netherspite->GetX(), netherspite->GetY(), netherspite->GetZ());

            auto unitList =
                GetAllPlayersInObjectRangeCheckInCell(m_creature, distNether);

            std::vector<Player*> possibleTargets;
            for (auto temp : unitList)
            {
                if (temp->isGameMaster())
                    continue;
                if (temp->isAlive())
                {
                    if ((m_colour == PORTAL_RED &&
                            temp->has_aura(SPELL_EXHAUST_RED)) ||
                        (m_colour == PORTAL_GREEN &&
                            temp->has_aura(SPELL_EXHAUST_GREEN)) ||
                        (m_colour == PORTAL_BLUE &&
                            temp->has_aura(SPELL_EXHAUST_BLUE)) ||
                        temp->IsImmunedToDamage(SPELL_SCHOOL_MASK_ALL))
                        continue;

                    if (m_creature->isInFront(
                            temp, distNether, IN_FRONT_10_F) &&
                        m_creature->GetDistance(temp) < distNether)
                        possibleTargets.push_back(temp);
                }
            }

            uint32 beamId = m_colour == PORTAL_RED ? SPELL_NETHERBEAM_RED :
                                                     m_colour == PORTAL_GREEN ?
                                                     SPELL_NETHERBEAM_GREEN :
                                                     SPELL_NETHERBEAM_BLUE;
            if (possibleTargets.empty())
            {
                // No players to cast on, cast on netherspite
                if (m_currentTarget != netherspite->GetObjectGuid())
                    m_creature->InterruptNonMeleeSpells(true);
                if (m_currentTarget == netherspite->GetObjectGuid() ||
                    DoCastSpellIfCan(netherspite, beamId) == CAST_OK)
                {
                    uint32 buffId = m_colour == PORTAL_RED ?
                                        SPELL_NBUFF_RED :
                                        m_colour == PORTAL_GREEN ?
                                        SPELL_NBUFF_GREEN :
                                        SPELL_NBUFF_BLUE;
                    netherspite->CastSpell(netherspite, buffId, true);
                    m_currentTarget = netherspite->GetObjectGuid();
                }
            }
            else
            {
                // There exists possible players to cast on
                Player* target = NULL;
                float lastDist = 1000.0f;
                for (auto& possibleTarget : possibleTargets)
                {
                    float dist = possibleTarget->GetDistance(m_creature);
                    if (dist < lastDist)
                    {
                        target = possibleTarget;
                        lastDist = dist;
                    }
                }

                uint32 buffId = m_colour == PORTAL_RED ?
                                    SPELL_PBUFF_RED :
                                    m_colour == PORTAL_GREEN ?
                                    SPELL_PBUFF_GREEN :
                                    SPELL_PBUFF_BLUE;
                uint32 duration = m_colour == PORTAL_RED ?
                                      20000 :
                                      m_colour == PORTAL_GREEN ? 10000 : 8000;
                if (m_currentTarget != target->GetObjectGuid())
                    m_creature->InterruptNonMeleeSpells(true);
                if (m_currentTarget == target->GetObjectGuid() ||
                    DoCastSpellIfCan(target, beamId) == CAST_OK)
                {
                    target->CastSpell(target, buffId, true);
                    if (m_colour == PORTAL_RED)
                        netherspite->SetFocusTarget(target);
                    bool addNew = true;
                    std::vector<PortalDebuff>::iterator itr;
                    for (itr = m_debuffedTargets.begin();
                         itr != m_debuffedTargets.end(); ++itr)
                        if (itr->target_guid == target->GetObjectGuid())
                        {
                            addNew = false;
                            break;
                        }
                    if (addNew)
                        m_debuffedTargets.push_back(
                            PortalDebuff(target->GetObjectGuid(), duration));
                    else
                        itr->time_remaining = duration;
                    m_currentTarget = target->GetObjectGuid();
                }
            }
            m_checkTimer = 1000;
        }
        else
            m_checkTimer -= uiDiff;
    }
};

struct MANGOS_DLL_DECL nether_portal_redAI : public nether_portalAI
{
    nether_portal_redAI(Creature* pCreature) : nether_portalAI(pCreature)
    {
        Reset();
    }

    void Reset() override { m_colour = PORTAL_RED; }
};
CreatureAI* GetAI_nether_portal_red(Creature* pCreature)
{
    return new nether_portal_redAI(pCreature);
}

struct MANGOS_DLL_DECL nether_portal_greenAI : public nether_portalAI
{
    nether_portal_greenAI(Creature* pCreature) : nether_portalAI(pCreature)
    {
        Reset();
    }

    void Reset() override { m_colour = PORTAL_GREEN; }
};
CreatureAI* GetAI_nether_portal_green(Creature* pCreature)
{
    return new nether_portal_greenAI(pCreature);
}

struct MANGOS_DLL_DECL nether_portal_blueAI : public nether_portalAI
{
    nether_portal_blueAI(Creature* pCreature) : nether_portalAI(pCreature)
    {
        Reset();
    }

    void Reset() override { m_colour = PORTAL_BLUE; }
};
CreatureAI* GetAI_nether_portal_blue(Creature* pCreature)
{
    return new nether_portal_blueAI(pCreature);
}

struct MANGOS_DLL_DECL boss_netherspiteAI : public ScriptedAI
{
    boss_netherspiteAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_instance = pCreature->GetMapId() == KARAZHAN_MAP_ID ?
                         (instance_karazhan*)pCreature->GetInstanceData() :
                         NULL;
        Reset();
    }

    instance_karazhan* m_instance;
    NetherspitePhase m_phase;
    uint32 m_enrageTimer;
    uint32 m_phaseTimer;
    // Phase One
    std::vector<ObjectGuid> m_portalGuids;
    uint32 m_portalsTimer;
    uint32 m_voidZoneTimer;
    uint32 m_empowermentTimer;
    bool m_firstSpawn;
    // Phase Two
    uint32 m_netherbreathTimer;

    void Reset() override
    {
        m_phase = PORTAL_PHASE;
        m_phaseTimer = 60000;
        m_portalsTimer = 5000;
        m_voidZoneTimer = urand(5000, 15000);
        m_netherbreathTimer = 0;
        m_empowermentTimer = 15000;
        m_enrageTimer = 9 * 60 * 1000;
        m_firstSpawn = true;

        SetCombatMovement(true);
        m_creature->SetFocusTarget(nullptr);
    }

    void Aggro(Unit* /*pWho*/) override
    {
        DoCastSpellIfCan(m_creature, SPELL_NETHER_BURN, CAST_TRIGGERED);
        if (m_instance)
            m_instance->SetData(TYPE_NETHERSPITE, IN_PROGRESS);
    }

    void JustReachedHome() override
    {
        if (m_instance)
            m_instance->SetData(TYPE_NETHERSPITE, FAIL);
        DespawnPortals();
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        if (m_instance)
            m_instance->SetData(TYPE_NETHERSPITE, DONE);
        DespawnPortals();
    }

    void DespawnPortals()
    {
        for (auto& elem : m_portalGuids)
        {
            if (Creature* portal = m_creature->GetMap()->GetCreature(elem))
            {
                if (nether_portalAI* portalAI =
                        dynamic_cast<nether_portalAI*>(portal->AI()))
                    portalAI->StopPortal();
                portal->ForcedDespawn();
            }
        }
        m_portalGuids.clear();
    }

    void DoPhaseSwitchIfNeeded(const uint32 uiDiff)
    {
        if (m_phaseTimer <= uiDiff)
        {
            if (m_phase == PORTAL_PHASE)
            {
                if (DoCastSpellIfCan(m_creature, SPELL_NETHERSPITE_ROAR) ==
                    CAST_OK)
                {
                    DoCastSpellIfCan(
                        m_creature, SPELL_INVISIBLE_ROOT, CAST_TRIGGERED);
                    SetCombatMovement(false);
                    DespawnPortals();
                    m_creature->remove_auras(SPELL_EMPOWERMENT);
                    DoScriptText(BANISH_EMOTE, m_creature);
                    DoCastSpellIfCan(
                        m_creature, SPELL_VIS_NETHER_RAGE, CAST_TRIGGERED);
                    DoResetThreat();
                    m_creature->SetFocusTarget(nullptr);
                    m_phase = BANISH_PHASE;
                    m_netherbreathTimer = urand(10000, 15000);
                    m_phaseTimer = 30000;
                }
            }
            else if (m_phase == BANISH_PHASE)
            {
                if (DoCastSpellIfCan(m_creature, SPELL_NETHERSPITE_ROAR) ==
                    CAST_OK)
                {
                    m_creature->remove_auras(SPELL_INVISIBLE_ROOT);
                    SetCombatMovement(true);
                    m_creature->remove_auras(SPELL_VIS_NETHER_RAGE);
                    DoScriptText(PORTAL_EMOTE, m_creature);
                    DoResetThreat();
                    m_phase = PORTAL_PHASE;
                    m_portalsTimer = 5000;
                    m_empowermentTimer = 15000;
                    m_phaseTimer = 60000;
                    m_voidZoneTimer = urand(5000, 15000);
                }
            }
        }
        else
            m_phaseTimer -= uiDiff;
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (m_enrageTimer)
        {
            if (m_enrageTimer <= uiDiff)
            {
                if (DoCastSpellIfCan(m_creature, SPELL_NETHER_INFUSION) ==
                    CAST_OK)
                    m_enrageTimer = 0;
            }
            else
                m_enrageTimer -= uiDiff;
        }

        DoPhaseSwitchIfNeeded(uiDiff);

        if (m_phase == PORTAL_PHASE)
        {
            if (m_empowermentTimer &&
                m_enrageTimer >
                    0) // Stop casting Empowerement if enrage is active
            {
                if (m_empowermentTimer <= uiDiff)
                {
                    if (DoCastSpellIfCan(m_creature, SPELL_EMPOWERMENT) ==
                        CAST_OK)
                        m_empowermentTimer = 0;
                }
                else
                    m_empowermentTimer -= uiDiff;
            }

            if (m_voidZoneTimer <= uiDiff)
            {
                if (Unit* target = m_creature->SelectAttackingTarget(
                        ATTACKING_TARGET_RANDOM, 0, SPELL_VOID_ZONE))
                {
                    if (DoCastSpellIfCan(target, SPELL_VOID_ZONE) == CAST_OK)
                        m_voidZoneTimer = urand(15000, 20000);
                }
            }
            else
                m_voidZoneTimer -= uiDiff;

            if (m_portalsTimer)
            {
                if (m_portalsTimer <= uiDiff)
                {
                    std::vector<PortalColour> colours;
                    colours.push_back(PORTAL_RED);
                    colours.push_back(PORTAL_GREEN);
                    colours.push_back(PORTAL_BLUE);
                    // Keep RGB order first time, random otherwise
                    if (!m_firstSpawn)
                        std::random_shuffle(colours.begin(), colours.end(),
                            netherspite_shufflerng);
                    else
                        m_firstSpawn = false;

                    int i = 0;
                    for (std::vector<PortalColour>::iterator itr =
                             colours.begin();
                         itr != colours.end() && i < 3; ++itr)
                    {
                        PortalPos p = PortalPositions[i++];

                        switch (*itr)
                        {
                        case PORTAL_RED:
                            if (Creature* portal = m_creature->SummonCreature(
                                    NPC_PORTAL_RED, p.X, p.Y, p.Z, 0.0f,
                                    TEMPSUMMON_TIMED_DESPAWN, 100 * 1000))
                            {
                                if (nether_portalAI* pAI =
                                        dynamic_cast<nether_portalAI*>(
                                            portal->AI()))
                                    pAI->m_netherspiteGuid =
                                        m_creature->GetObjectGuid();
                                m_portalGuids.push_back(
                                    portal->GetObjectGuid());
                            }
                            break;
                        case PORTAL_BLUE:
                            if (Creature* portal = m_creature->SummonCreature(
                                    NPC_PORTAL_BLUE, p.X, p.Y, p.Z, 0.0f,
                                    TEMPSUMMON_TIMED_DESPAWN, 100 * 1000))
                            {
                                if (nether_portalAI* pAI =
                                        dynamic_cast<nether_portalAI*>(
                                            portal->AI()))
                                    pAI->m_netherspiteGuid =
                                        m_creature->GetObjectGuid();
                                m_portalGuids.push_back(
                                    portal->GetObjectGuid());
                            }
                            break;
                        case PORTAL_GREEN:
                            if (Creature* portal = m_creature->SummonCreature(
                                    NPC_PORTAL_GREEN, p.X, p.Y, p.Z, 0.0f,
                                    TEMPSUMMON_TIMED_DESPAWN, 100 * 1000))
                            {
                                if (nether_portalAI* pAI =
                                        dynamic_cast<nether_portalAI*>(
                                            portal->AI()))
                                    pAI->m_netherspiteGuid =
                                        m_creature->GetObjectGuid();
                                m_portalGuids.push_back(
                                    portal->GetObjectGuid());
                            }
                            break;
                        }
                    }
                    m_portalsTimer = 0;
                }
                else
                    m_portalsTimer -= uiDiff;
            }
        }
        else if (m_phase == BANISH_PHASE)
        {
            if (m_netherbreathTimer <= uiDiff)
            {
                if (Unit* target = m_creature->SelectAttackingTarget(
                        ATTACKING_TARGET_RANDOM, 0, SPELL_NETHERBREATH))
                {
                    if (DoCastSpellIfCan(target, SPELL_NETHERBREATH) == CAST_OK)
                        m_netherbreathTimer = 5000;
                }
            }
            else
                m_netherbreathTimer -= uiDiff;
        }

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_netherspite(Creature* pCreature)
{
    return new boss_netherspiteAI(pCreature);
}

void AddSC_boss_netherspite()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_netherspite";
    pNewScript->GetAI = GetAI_boss_netherspite;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "nether_portal_red";
    pNewScript->GetAI = GetAI_nether_portal_red;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "nether_portal_green";
    pNewScript->GetAI = GetAI_nether_portal_green;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "nether_portal_blue";
    pNewScript->GetAI = GetAI_nether_portal_blue;
    pNewScript->RegisterSelf();
}
