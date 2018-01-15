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
SDName: Dark_Portal
SD%Complete: 100
SDComment:
SDCategory: Caverns of Time, The Dark Portal
EndScriptData */

/* ContentData
npc_medivh_black_morass
npc_time_rift
EndContentData */

#include "dark_portal.h"

/*######
## npc_medivh_black_morass
######*/

enum
{
    SPELL_MANA_SHIELD = 31635,
};

struct MANGOS_DLL_DECL npc_medivh_black_morassAI : public ScriptedAI
{
    npc_medivh_black_morassAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_pInstance = (instance_dark_portal*)pCreature->GetInstanceData();
        Reset();
    }

    instance_dark_portal* m_pInstance;
    uint32 m_uiSpawnCircle;
    uint32 m_uiCrystalTimer;
    std::vector<ObjectGuid> m_crystals;

    void Reset() override
    {
        if (m_pInstance && m_pInstance->GetData(TYPE_MEDIVH) != DONE)
        {
            DoCast(m_creature, SPELL_MEDIVH_CHANNEL, true);
            DoCast(m_creature, SPELL_MANA_SHIELD, true);
            DoCast(m_creature, SPELL_RUNE_CIRCLE, true);
        }
        m_uiCrystalTimer = 0;
        for (auto& elem : m_crystals)
            if (Creature* crystal = m_creature->GetMap()->GetCreature(elem))
                crystal->ForcedDespawn();
    }

    void DamageShield()
    {
        if (!m_pInstance || m_pInstance->GetData(TYPE_MEDIVH) != IN_PROGRESS)
            return;

        // Drain one percentage if we're above 0
        if (m_pInstance->GetData(TYPE_SHIELD_PCT) > 0)
        {
            m_pInstance->SetData(
                TYPE_SHIELD_PCT, m_pInstance->GetData(TYPE_SHIELD_PCT) - 1);
            m_pInstance->DoUpdateWorldState(
                WORLD_STATE_SHIELD_PCT, m_pInstance->GetData(TYPE_SHIELD_PCT));
        }

        // Do events at given percentages
        if (m_pInstance->GetData(TYPE_SHIELD_PCT) == 75)
        {
            DoScriptText(SAY_MEDIVH_WEAK_75, m_creature);
            if (!m_crystals.empty())
                if (Creature* crystal =
                        m_creature->GetMap()->GetCreature(m_crystals[0]))
                {
                    crystal->CastSpell(
                        crystal, SPELL_CRYSTAL_ARCANE_EXPLOSION, false);
                    crystal->ForcedDespawn(3000);
                    m_crystals.erase(m_crystals.begin());
                }
        }
        else if (m_pInstance->GetData(TYPE_SHIELD_PCT) == 50)
        {
            DoScriptText(SAY_MEDIVH_WEAK_50, m_creature);
            if (!m_crystals.empty())
                if (Creature* crystal =
                        m_creature->GetMap()->GetCreature(m_crystals[0]))
                {
                    crystal->CastSpell(
                        crystal, SPELL_CRYSTAL_ARCANE_EXPLOSION, false);
                    crystal->ForcedDespawn(3000);
                    m_crystals.erase(m_crystals.begin());
                }
        }
        else if (m_pInstance->GetData(TYPE_SHIELD_PCT) == 25)
        {
            DoScriptText(SAY_MEDIVH_WEAK_25, m_creature);
            if (!m_crystals.empty())
                if (Creature* crystal =
                        m_creature->GetMap()->GetCreature(m_crystals[0]))
                {
                    crystal->CastSpell(
                        crystal, SPELL_CRYSTAL_ARCANE_EXPLOSION, false);
                    crystal->ForcedDespawn(3000);
                    m_crystals.erase(m_crystals.begin());
                }
        }
        else if (m_pInstance->GetData(TYPE_SHIELD_PCT) == 0)
        {
            m_pInstance->ResetEvent();
            m_creature->Kill(m_creature, false);
        }
    }

    void MoveInLineOfSight(Unit* pWho) override
    {
        if (!m_pInstance)
            return;

        // Start event if player gets nearby
        if (pWho->GetTypeId() == TYPEID_PLAYER &&
            m_pInstance->GetData(TYPE_MEDIVH) != IN_PROGRESS &&
            m_pInstance->GetData(TYPE_MEDIVH) != DONE &&
            m_creature->IsWithinDistInMap(pWho, 20.0f) &&
            !((Player*)pWho)->isGameMaster() && pWho->isAlive())
        {
            m_pInstance->StartEvent();
            m_uiCrystalTimer = 4000;
        }

        // Make attack medivh creatures channel rift channel on medivh when they
        // get nearby
        if (pWho->GetTypeId() == TYPEID_UNIT &&
            m_pInstance->IsAttackMedivhCreature(pWho->GetEntry()) &&
            !pWho->IsNonMeleeSpellCasted(false) &&
            !m_creature->movement_gens.has(movement::gen::chase) &&
            m_creature->IsWithinDistInMap(pWho, 20.0f) && pWho->isAlive() &&
            !pWho->isInCombat())
        {
            if (pWho->GetEntry() == NPC_AEONUS)
                pWho->CastSpell(m_creature, SPELL_CORRUPT_AEONUS, false);
            else
                pWho->CastSpell(m_creature, SPELL_CORRUPT, false);
            pWho->movement_gens.remove_if([](auto*)
                {
                    return true;
                });
            pWho->movement_gens.push(new movement::IdleMovementGenerator());
            pWho->StopMoving();
        }
    }

    void AttackStart(Unit* /*pWho*/) override {}

    void JustDied(Unit* /*pKiller*/) override
    {
        if (m_pInstance)
            m_pInstance->SetData(TYPE_MEDIVH, FAIL);

        DoScriptText(SAY_MEDIVH_FAIL, m_creature);
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (m_uiCrystalTimer)
        {
            if (m_uiCrystalTimer <= uiDiff)
            {
                std::vector<DynamicWaypoint> dw;
                dw.push_back(DynamicWaypoint(-2019.4f, 7127.3f, 22.7f));
                dw.push_back(DynamicWaypoint(-2025.7f, 7129.9f, 22.7f));
                dw.push_back(DynamicWaypoint(-2031.7f, 7127.7f, 22.7f));
                dw.push_back(DynamicWaypoint(-2034.6f, 7121.2f, 22.7f));
                dw.push_back(DynamicWaypoint(-2032.4f, 7115.6f, 22.7f));
                dw.push_back(DynamicWaypoint(-2025.8f, 7112.5f, 22.7f));
                dw.push_back(DynamicWaypoint(-2019.5f, 7115.2f, 22.7f));
                dw.push_back(DynamicWaypoint(-2016.9f, 7121.2f, 22.7f));
                if (Creature* crystal = m_creature->SummonCreature(
                        NPC_DARK_PORTAL_BLACK_CRYSTAL, -2016.9f, 7121.2f, 22.7f,
                        3.14f, TEMPSUMMON_MANUAL_DESPAWN, 0))
                {
                    crystal->movement_gens.push(
                        new movement::DynamicWaypointMovementGenerator(
                            dw, true));
                    crystal->CastSpell(
                        crystal, SPELL_BLACK_CRYSTAL_STATE, false);
                    m_crystals.push_back(crystal->GetObjectGuid());
                }
                if (m_crystals.size() < 3)
                    m_uiCrystalTimer = 7000;
                else
                    m_uiCrystalTimer = 0;
            }
            else
                m_uiCrystalTimer -= uiDiff;
        }
    }
};

CreatureAI* GetAI_npc_medivh_black_morass(Creature* pCreature)
{
    return new npc_medivh_black_morassAI(pCreature);
}

bool Medivh_DummyEffectHandler(Unit* /*pCaster*/, uint32 uiSpellId,
    SpellEffectIndex /*uiEffIndex*/, Creature* pCreatureTarget)
{
    if (uiSpellId == SPELL_CORRUPT_TRIGGERED &&
        pCreatureTarget->GetEntry() == NPC_MEDIVH)
    {
        if (pCreatureTarget->AI())
        {
            if (npc_medivh_black_morassAI* ai =
                    dynamic_cast<npc_medivh_black_morassAI*>(
                        pCreatureTarget->AI()))
                ai->DamageShield();
        }
    }
    return true;
}

/*######
## npc_time_rift
######*/

enum
{
    SPELL_RIFT_PERIODIC = 31320, // should trigger 31388

    // Boss spawn yells
    SAY_CHRONO_LORD_ENTER = -1269006,
    SAY_TEMPORUS_ENTER = -1269000,
    SAY_AEONUS_ENTER = -1269012,
};

struct MANGOS_DLL_DECL npc_time_riftAI : public ScriptedAI
{
    npc_time_riftAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_pInstance = (instance_dark_portal*)pCreature->GetInstanceData();
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();
        Reset();

        if (m_pInstance)
            m_pInstance->OnNewPortal(pCreature->GetObjectGuid());

        m_spawnTimer = 15000;
        m_uiSpawnPhase = 0;
    }

    instance_dark_portal* m_pInstance;
    ObjectGuid m_portalBoss;
    std::vector<ObjectGuid> m_summonedList; // If the portal dies all the
                                            // spawned mobs should despawn as
                                            // well
    bool m_bIsRegularMode;
    uint32 m_spawnTimer;
    uint32 m_uiSpawnPhase;

    void Reset() override {}

    // Died is only used on event reset, if a portal is cleared we simply
    // desummon it
    void JustDied(Unit* /*pWho*/) override
    {
        for (auto& elem : m_summonedList)
            if (Creature* summon = m_creature->GetMap()->GetCreature(elem))
            {
                // Skip despawn if unit is a dead boss (to not rob people of
                // loot)
                if ((summon->GetEntry() == NPC_CHRONO_LORD_DEJA ||
                        summon->GetEntry() == NPC_TEMPORUS ||
                        summon->GetEntry() == NPC_AEONUS) &&
                    !summon->isAlive())
                    continue;

                summon->ForcedDespawn();
            }
        m_creature->ForcedDespawn();
    }

    void JustSummoned(Creature* pCreature) override
    {
        m_summonedList.push_back(pCreature->GetObjectGuid());
    }

    // Removes any creatures on our list that are not alive anymore
    void ClearSpawnList()
    {
        for (auto itr = m_summonedList.begin(); itr != m_summonedList.end();)
            if (Creature* c = m_creature->GetMap()->GetCreature(*itr))
            {
                if (c->isDead())
                    itr = m_summonedList.erase(itr);
                else
                    ++itr;
            }
    }

    void PickSpawns(std::vector<uint32>& spawns)
    {
        if (!m_pInstance)
            return;

        // Boss spawns
        if (m_pInstance->IsBossWave() && m_uiSpawnPhase == 0)
        {
            // Replacements do not spawn in normal
            // Adds keep coming at Deja and Temporus, but not Aeonus
            if (m_pInstance->GetData(TYPE_RIFT_NUMBER) == 6)
            {
                if (m_pInstance->GetData(TYPE_CHRONO_LORD) != DONE ||
                    m_bIsRegularMode)
                    spawns.push_back(NPC_CHRONO_LORD_DEJA);
                else
                    spawns.push_back(NPC_CHRONO_LORD);
                m_spawnTimer = urand(10000, 15000);
            }
            else if (m_pInstance->GetData(TYPE_RIFT_NUMBER) == 12)
            {
                if (m_pInstance->GetData(TYPE_TEMPORUS) != DONE ||
                    m_bIsRegularMode)
                    spawns.push_back(NPC_TEMPORUS);
                else
                    spawns.push_back(NPC_TIMEREAVER);
                m_spawnTimer = urand(10000, 15000);
            }
            else if (m_pInstance->GetData(NPC_AEONUS) != DONE)
            {
                spawns.push_back(NPC_AEONUS);
                m_spawnTimer = 0;
            }

            ++m_uiSpawnPhase;
            return;
        }

        // Spawn orders:
        // Random rift keeper or rift lord, then:
        // before deja = Assassin, 3 whelps, Chronomancer
        // before temporus = Executioner, Chronomancer, 3 whelps, Assassin
        // before aeonus = Executioner, Vanquisher, Chronomancer, Assassin

        switch (m_uiSpawnPhase)
        {
        case 0:
            spawns.push_back(
                urand(0, 1) ?
                    (urand(0, 1) ? NPC_RIFT_KEEPER_1 : NPC_RIFT_KEEPER_2) :
                    (urand(0, 1) ? NPC_RIFT_LORD_1 : NPC_RIFT_LORD_2));
            break;
        case 1:
            if (m_pInstance->GetData(TYPE_RIFT_NUMBER) <= 6)
                spawns.push_back(urand(0, 1) ? NPC_ASSASSIN_1 : NPC_ASSASSIN_2);
            else
                spawns.push_back(
                    urand(0, 1) ? NPC_EXECUTIONER_1 : NPC_EXECUTIONER_2);
            break;
        case 2:
            if (m_pInstance->GetData(TYPE_RIFT_NUMBER) <= 6)
            {
                spawns.push_back(NPC_WHELP);
                spawns.push_back(NPC_WHELP);
                spawns.push_back(NPC_WHELP);
            }
            else if (m_pInstance->GetData(TYPE_RIFT_NUMBER) <= 12)
                spawns.push_back(
                    urand(0, 1) ? NPC_CHRONOMANCER_1 : NPC_CHRONOMANCER_2);
            else
                spawns.push_back(
                    urand(0, 1) ? NPC_VANQUISHER_1 : NPC_VANQUISHER_2);
            break;
        case 3:
            if (m_pInstance->GetData(TYPE_RIFT_NUMBER) <= 6)
            {
                spawns.push_back(
                    urand(0, 1) ? NPC_CHRONOMANCER_1 : NPC_CHRONOMANCER_2);
                m_uiSpawnPhase = 1;
                m_spawnTimer = urand(10000, 15000);
                return;
            }
            else if (m_pInstance->GetData(TYPE_RIFT_NUMBER) <= 12)
            {
                spawns.push_back(NPC_WHELP);
                spawns.push_back(NPC_WHELP);
                spawns.push_back(NPC_WHELP);
            }
            else
                spawns.push_back(
                    urand(0, 1) ? NPC_CHRONOMANCER_1 : NPC_CHRONOMANCER_2);
            break;
        case 4:
            spawns.push_back(urand(0, 1) ? NPC_ASSASSIN_1 : NPC_ASSASSIN_2);
            m_uiSpawnPhase = 1;
            m_spawnTimer = urand(10000, 15000);
            return;
            break;
        default:
            m_spawnTimer = 0;
            return;
        }

        ++m_uiSpawnPhase;
        m_spawnTimer = urand(10000, 15000);
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (m_portalBoss)
        {
            if (Creature* boss =
                    m_creature->GetMap()->GetCreature(m_portalBoss))
            {
                // Post is cleared if the rift protector is dead
                if (!boss->isAlive())
                {
                    if (m_pInstance)
                    {
                        ClearSpawnList();
                        m_pInstance->OnPortalClear(m_creature->GetObjectGuid(),
                            m_summonedList, boss->GetEntry() == NPC_AEONUS);
                    }
                    m_creature->ForcedDespawn();
                    return;
                }
                // The rift protector evades if you pull him too far, this apply
                // to bosses as well, except for Aeonus
                else if (!m_creature->IsWithinDistInMap(boss, 80) &&
                         boss->AI() && boss->GetEntry() != NPC_AEONUS)
                    boss->AI()->EnterEvadeMode();
            }
        }

        if (m_spawnTimer)
        {
            if (m_spawnTimer <= uiDiff)
            {
                bool isBoss = false;
                if (m_uiSpawnPhase == 0)
                    isBoss = true;
                std::vector<uint32> spawns;
                PickSpawns(spawns);
                for (auto& spawn : spawns)
                {
                    Creature* summon = m_creature->SummonCreature(spawn,
                        m_creature->GetX(), m_creature->GetY(),
                        m_creature->GetZ(), m_creature->GetO(),
                        TEMPSUMMON_CORPSE_TIMED_DESPAWN, 30 * 60 * 1000);
                    if (summon)
                    {
                        if (isBoss)
                        {
                            m_portalBoss = summon->GetObjectGuid();
                            // Create a link between the rift keeper of this
                            // rift
                            if (m_pInstance && summon->GetEntry() != NPC_AEONUS)
                                m_creature->CastSpell(
                                    summon, SPELL_RIFT_CHANNEL, false);
                            if (summon->GetEntry() == NPC_AEONUS)
                                summon->SetFlag(UNIT_FIELD_FLAGS,
                                    UNIT_FLAG_PASSIVE); // Aeonus just wander
                                                        // past you until you
                                                        // attack him
                        }
                        else
                            summon->SetFlag(UNIT_FIELD_FLAGS,
                                UNIT_FLAG_PASSIVE); // Mobs just wander past you
                                                    // until you attack them
                    }
                }
            }
            else
                m_spawnTimer -= uiDiff;
        }
    }
};

CreatureAI* GetAI_npc_time_rift(Creature* pCreature)
{
    return new npc_time_riftAI(pCreature);
}

#define SAAT_CHRONO_BEACON_ANSWER "I require a chrono-beacon, Sa'at."
enum
{
    ITEM_CHRONO_BEACON = 24289,
    SPELL_CONJURE_CHRONO_BEACON = 34975,
};

bool GossipHello_npc_saat(Player* pPlayer, Creature* pCreature)
{
    if (pCreature->isQuestGiver())
        pPlayer->PrepareQuestMenu(pCreature->GetObjectGuid());

    if (pCreature->IsNonMeleeSpellCasted(false))
        return false;

    if (pPlayer->HasItemCount(ITEM_CHRONO_BEACON, 1))
    {
        pPlayer->SEND_GOSSIP_MENU(10007, pCreature->GetObjectGuid());
    }
    else
    {
        pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, SAAT_CHRONO_BEACON_ANSWER,
            GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 1);
        pPlayer->SEND_GOSSIP_MENU(10000, pCreature->GetObjectGuid());
    }

    return true;
}

bool GossipSelect_npc_saat(
    Player* pPlayer, Creature* pCreature, uint32 /*uiSender*/, uint32 uiAction)
{
    if (uiAction == GOSSIP_ACTION_INFO_DEF + 1)
    {
        pPlayer->CLOSE_GOSSIP_MENU();

        if (!pPlayer->HasItemCount(ITEM_CHRONO_BEACON, 1) &&
            !pCreature->IsNonMeleeSpellCasted(false))
            pCreature->CastSpell(pPlayer, SPELL_CONJURE_CHRONO_BEACON, false);
    }

    return true;
}

enum
{
    SPELL_SAND_BREATH = 31478
};

struct MANGOS_DLL_DECL npc_time_keeperAI : public ScriptedAI
{
    npc_time_keeperAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_uiDespawnTimer = 30000;
        Reset();
    }

    uint32 m_uiDespawnTimer;
    uint32 m_uiBreathTimer;

    void Reset() override { m_uiBreathTimer = urand(6000, 8000); }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (m_uiDespawnTimer <= uiDiff)
        {
            m_creature->ForcedDespawn();
            return;
        }
        else
            m_uiDespawnTimer -= uiDiff;

        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (m_uiBreathTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_SAND_BREATH) ==
                CAST_OK)
                m_uiBreathTimer = urand(8000, 12000);
        }
        else
            m_uiBreathTimer -= uiDiff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_npc_time_keeper(Creature* pCreature)
{
    return new npc_time_keeperAI(pCreature);
}

void AddSC_dark_portal()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "npc_medivh_black_morass";
    pNewScript->GetAI = &GetAI_npc_medivh_black_morass;
    pNewScript->pEffectDummyNPC = &Medivh_DummyEffectHandler;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_time_rift";
    pNewScript->GetAI = &GetAI_npc_time_rift;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_saat";
    pNewScript->pGossipHello = &GossipHello_npc_saat;
    pNewScript->pGossipSelect = &GossipSelect_npc_saat;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_time_keeper";
    pNewScript->GetAI = &GetAI_npc_time_keeper;
    pNewScript->RegisterSelf();
}
