/* Copyright (C) 2013-2015 Corecraft <https://www.worldofcorecraft.com/>
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
SDName: Boss_Morogrim_Tidewalker
SD%Complete: 100
SDComment:
SDCategory: Coilfang Resevoir, Serpentshrine Cavern
EndScriptData */

#include "precompiled.h"
#include "serpentshrine_cavern.h"

enum
{
    SAY_AGGRO = -1548030,
    SAY_MURLOCS_1 = -1548031,
    SAY_MURLOCS_2 = -1548032,
    SAY_WATER_GLOBULE_1 = -1548033,
    SAY_WATER_GLOBULE_2 = -1548034,
    SAY_KILL_1 = -1548035,
    SAY_KILL_2 = -1548036,
    SAY_KILL_3 = -1548037,
    SAY_DEATH = -1548038,
    EMOTE_WATERY_GRAVE = -1548039,
    EMOTE_MURLOCS = -1548040,
    EMOTE_WATER_GLOBULE = -1548041,

    WATER_SPELL_COUNT = 4,
    SPELL_TIDAL_WAVE = 37730,
    SPELL_EARTHQUAKE = 37764,

    NPC_ELEMENTAL_ADD = 21874,
    NPC_MURLOC_ADD = 21920,
    SPELL_FREEZE = 37871,
};

static const uint32 wateryGraves[WATER_SPELL_COUNT] = {
    37850, 38023, 38024, 38025};

static const uint32 waterGlobule[WATER_SPELL_COUNT] = {
    37854, 37858, 37860, 37861};

struct SpawnInfo
{
    uint32 entry;
    float x;
    float y;
    float z;
};

#define WAVES 3
const std::vector<SpawnInfo> wave_spawns[WAVES] = {
    // Wave One: Murlocs
    {
     // North
     {NPC_MURLOC_ADD, 521.9f, -715.5f, -7.1f},
     {NPC_MURLOC_ADD, 524.5f, -713.6f, -7.1f},
     {NPC_MURLOC_ADD, 526.1f, -712.3f, -7.1f},
     {NPC_MURLOC_ADD, 527.8f, -711.1f, -7.1f},
     {NPC_MURLOC_ADD, 529.9f, -709.5f, -7.1f},
     // South
     {NPC_MURLOC_ADD, 269.8f, -717.5f, -3.9f},
     {NPC_MURLOC_ADD, 267.8f, -716.3f, -3.4f},
     {NPC_MURLOC_ADD, 265.7f, -714.9f, -3.3f},
     {NPC_MURLOC_ADD, 263.7f, -713.6f, -3.2f},
     {NPC_MURLOC_ADD, 261.4f, -712.2f, -3.1f},
    },
    // Wave Two: Murlocs (duplicate of wave one)
    {
     // North
     {NPC_MURLOC_ADD, 521.9f, -715.5f, -7.1f},
     {NPC_MURLOC_ADD, 524.5f, -713.6f, -7.1f},
     {NPC_MURLOC_ADD, 526.1f, -712.3f, -7.1f},
     {NPC_MURLOC_ADD, 527.8f, -711.1f, -7.1f},
     {NPC_MURLOC_ADD, 529.9f, -709.5f, -7.1f},
     // South
     {NPC_MURLOC_ADD, 269.8f, -717.5f, -3.9f},
     {NPC_MURLOC_ADD, 267.8f, -716.3f, -3.4f},
     {NPC_MURLOC_ADD, 265.7f, -714.9f, -3.3f},
     {NPC_MURLOC_ADD, 263.7f, -713.6f, -3.2f},
     {NPC_MURLOC_ADD, 261.4f, -712.2f, -3.1f},
    },
    // Wave Three: Elementals
    {
     // North
     {NPC_ELEMENTAL_ADD, 528.7f, -720.3f, -7.1f},
     {NPC_ELEMENTAL_ADD, 526.1f, -712.9f, -7.1f},
     {NPC_ELEMENTAL_ADD, 523.1f, -718.7f, -7.1f},
     // South
     {NPC_ELEMENTAL_ADD, 269.8f, -717.5f, -3.9f},
     {NPC_ELEMENTAL_ADD, 266.3f, -726.7f, -4.2f},
     {NPC_ELEMENTAL_ADD, 272.1f, -724.1f, -4.9f},
    },
};

struct MANGOS_DLL_DECL boss_morogrim_tidewalkerAI : public ScriptedAI
{
    boss_morogrim_tidewalkerAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_instance = (ScriptedInstance*)pCreature->GetInstanceData();
        Reset();
    }

    std::vector<ObjectGuid> m_spawns;
    ScriptedInstance* m_instance;
    uint32 m_grave;
    uint32 m_globule;
    uint32 m_tidal;
    uint32 m_quake;
    uint32 m_wave;
    uint32 m_wave_timer;

    void Reset() override
    {
        m_grave = urand(20000, 30000);
        m_globule = 30000;
        m_tidal = urand(12000, 17000);
        m_quake = urand(25000, 35000);
        m_wave = 0;
        m_wave_timer = 0;
    }

    void Aggro(Unit* /*pWho*/) override
    {
        DoScriptText(SAY_AGGRO, m_creature);
        if (m_instance)
            m_instance->SetData(TYPE_MOROGRIM, IN_PROGRESS);
    }

    void JustReachedHome() override
    {
        if (m_instance)
            m_instance->SetData(TYPE_MOROGRIM, FAIL);
        DespawnSummons();
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        if (m_instance)
            m_instance->SetData(TYPE_MOROGRIM, DONE);
        DoScriptText(SAY_DEATH, m_creature);
    }

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_KILL_1, SAY_KILL_2, SAY_KILL_3);
    }

    void JustSummoned(Creature* c) override
    {
        m_spawns.push_back(c->GetObjectGuid());
    }

    void DespawnSummons()
    {
        for (auto& elem : m_spawns)
            if (Creature* c = m_creature->GetMap()->GetCreature(elem))
                c->Kill(c);
        m_spawns.clear();
    }

    void WateryGrave()
    {
        const ThreatList& tl = m_creature->getThreatManager().getThreatList();
        if (tl.size() < 2)
            return;
        std::vector<Unit*> potential;
        potential.reserve(tl.size());
        ThreatList::const_iterator itr = tl.begin();
        std::advance(itr, 1); // Skip MT
        for (; itr != tl.end(); ++itr)
        {
            Unit* u = m_creature->GetMap()->GetUnit((*itr)->getUnitGuid());
            if (!u || u->GetTypeId() != TYPEID_PLAYER)
                continue;
            if (CanCastSpell(u, wateryGraves[0], true) == CAST_OK)
                potential.push_back(u);
        }
        for (int i = 0; i < WATER_SPELL_COUNT && !potential.empty(); ++i)
        {
            int j = urand(0, potential.size() - 1);
            m_creature->CastSpell(potential[j], wateryGraves[i], true);
            potential.erase(potential.begin() + j);
        }
    }

    void UpdateAI(const uint32 diff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (m_creature->GetHealthPercent() >= 26.0f)
        {
            // Watery Grave
            if (m_grave <= diff)
            {
                if (CanCastSpell(m_creature, wateryGraves[0], false) == CAST_OK)
                {
                    WateryGrave();
                    m_grave = urand(35000, 45000);
                    DoScriptText(EMOTE_WATERY_GRAVE, m_creature);
                }
            }
            else
                m_grave -= diff;
        }
        else
        {
            // Water Globule
            if (m_globule <= diff)
            {
                if (DoCastSpellIfCan(m_creature, waterGlobule[0]) == CAST_OK)
                {
                    DoScriptText(
                        urand(0, 1) ? SAY_WATER_GLOBULE_1 : SAY_WATER_GLOBULE_2,
                        m_creature);
                    for (int i = 1; i < WATER_SPELL_COUNT; ++i)
                    {
                        m_creature->CastSpell(
                            m_creature, waterGlobule[i], true);
                        m_globule = urand(45000, 60000);
                    }
                    DoScriptText(EMOTE_WATER_GLOBULE, m_creature);
                }
            }
            else
                m_globule -= diff;
        }

        if (m_quake <= diff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_EARTHQUAKE) == CAST_OK)
            {
                DoScriptText(
                    urand(0, 1) ? SAY_MURLOCS_1 : SAY_MURLOCS_2, m_creature);
                m_quake = urand(50000, 70000);
                // Spawn Murlocs
                m_wave = 1;
                m_wave_timer = 0;
                DoScriptText(EMOTE_MURLOCS, m_creature);
            }
        }
        else
            m_quake -= diff;

        if (m_tidal <= diff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_TIDAL_WAVE) == CAST_OK)
                m_tidal = urand(25000, 35000);
        }
        else
            m_tidal -= diff;

        if (m_wave)
        {
            if (m_wave_timer <= diff)
            {
                for (auto& spawn : wave_spawns[m_wave - 1])
                {
                    m_creature->SummonCreature(spawn.entry, spawn.x, spawn.y,
                        spawn.z, 0, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 4000,
                        SUMMON_OPT_ACTIVE);
                }

                if (++m_wave > WAVES)
                    m_wave = 0;
                m_wave_timer = 4000;
            }
            else
                m_wave_timer -= diff;
        }

        DoMeleeAttackIfReady();
    }
};

/* WATER GLOBULE */
struct MANGOS_DLL_DECL npc_water_globuleAI : public ScriptedAI
{
    npc_water_globuleAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_instance = (ScriptedInstance*)pCreature->GetInstanceData();
        Reset();
    }

    void Reset() override
    {
        m_tarSwitch = 0;
        m_castDelay = 0;
    }

    void SummonedBy(WorldObject*) override
    {
        m_creature->SetInCombatWithZone();
    }

    ScriptedInstance* m_instance;
    uint32 m_tarSwitch;
    uint32 m_castDelay;

    void UpdateAI(const uint32 diff) override
    {
        if (m_tarSwitch <= diff)
        {
            Unit* tar = nullptr;
            if (m_instance)
            {
                Creature* morogrim =
                    m_instance->GetSingleCreatureFromStorage(NPC_MOROGRIM);
                if (morogrim && morogrim->isAlive() && morogrim->getVictim())
                {
                    tar = morogrim->SelectAttackingTarget(
                        ATTACKING_TARGET_RANDOM, 1);
                    if (!tar)
                        tar = morogrim->getVictim();
                }
            }
            if (tar)
            {
                m_creature->SetFocusTarget(tar);
                m_tarSwitch = 20000;
            }
        }
        else
            m_tarSwitch -= diff;

        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (m_creature->getVictim()->has_aura(SPELL_FREEZE))
        {
            m_tarSwitch = 1;
            return;
        }

        if (m_castDelay <= diff)
            m_castDelay = 0;
        else
            m_castDelay -= diff;

        if (!m_castDelay)
            if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_FREEZE) ==
                CAST_OK)
                m_castDelay = 2000;
    }
};

CreatureAI* GetAI_boss_morogrim_tidewalker(Creature* pCreature)
{
    return new boss_morogrim_tidewalkerAI(pCreature);
}

CreatureAI* GetAI_npc_water_globule(Creature* pCreature)
{
    return new npc_water_globuleAI(pCreature);
}

void AddSC_boss_morogrim_tidewalker()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_morogrim_tidewalker";
    pNewScript->GetAI = &GetAI_boss_morogrim_tidewalker;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_water_globule";
    pNewScript->GetAI = &GetAI_npc_water_globule;
    pNewScript->RegisterSelf();
}
