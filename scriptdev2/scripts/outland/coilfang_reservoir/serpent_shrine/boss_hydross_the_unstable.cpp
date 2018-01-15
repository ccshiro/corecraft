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
SDName: boss_hydross_the_unstable
SD%Complete: 100
SDComment:
SDCategory: Coilfang Resevoir, Serpentshrine Cavern
EndScriptData */

#include "precompiled.h"
#include "serpentshrine_cavern.h"

enum
{
    SAY_AGGRO = -1547000,
    SAY_CLEAN_FORM = -1547001,
    SAY_CLEAN_KILL_1 = -1547002,
    SAY_CLEAN_KILL_2 = -1547003,
    SAY_CLEAN_DEATH = -1547004,
    SAY_CORRUPT_FORM = -1547005,
    SAY_CORRUPT_KILL_1 = -1547006,
    SAY_CORRUPT_KILL_2 = -1547007,
    SAY_CORRUPT_DEATH = -1547008,
    EMOTE_ENRAGE = -1547009,

    NPC_HYDROSS_BEAM_HELPER = 21933,
    SPELL_HYDROSS_BEAM = 38015,

    SPELL_IMMUNITY_NATURE = 7941,
    SPELL_IMMUNITY_FROST = 7940,

    SPELL_NATURE_FORM = 37961,
    SPELL_WATER_TOMB = 38235,
    SPELL_VILE_SLUDGE = 38246,
    SPELL_ELEMENTAL_SPAWN_IN = 25035,
    NPC_PURE_SPAWN = 22035,
    NPC_TAINTED_SPAWN = 22036,
    SPELL_ENRAGE = 39114,

    SPELL_SUMMON_RP_ELEMENTAL = 36459,
    NPC_RP_ELEMENTAL = 21253,

    SPELL_STUN_SELF = 48342, // Remove by SmartAI

    // Radius when water form comes on is less. But there's no delay.
    NATURE_FORM_RADIUS = 20,
    WATER_FORM_RADIUS = 15
};

static const float hydrossCenterPoint[3] = {
    -239.4f, -363.5f, -0.8f}; // X and Y to base nature & water radii on
static const float spawnAngles[4] = {M_PI_F / 4.0f, M_PI_F - M_PI_F / 4.0f,
    M_PI_F + M_PI_F / 4.0f, 2 * M_PI_F - M_PI_F / 4.0f};

#define MAX_MARK 6
static const uint32 markOfCorruption[MAX_MARK] = {
    38219, 38220, 38221, 38222, 38230, 40583};
static const uint32 markOfHydross[MAX_MARK] = {
    38215, 38216, 38217, 38218, 38231, 40584};

struct MANGOS_DLL_DECL boss_hydross_the_unstableAI : public ScriptedAI
{
    boss_hydross_the_unstableAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_instance = (ScriptedInstance*)pCreature->GetInstanceData();
        Reset();
        m_beam = 1000;
    }

    ScriptedInstance* m_instance;
    uint32 m_rpSummon;
    uint32 m_beam;
    uint32 m_spawn;
    std::vector<ObjectGuid> m_spawns;
    bool m_corrupt;
    uint32 m_mark;
    uint32 m_markStack;
    uint32 m_waterTomb;
    uint32 m_sludge;
    uint32 m_enrage;

    void Reset() override
    {
        m_rpSummon = 12000;
        m_spawn = 0;
        m_corrupt = false;
        m_markStack = 0;
        m_mark = 15000;
        m_waterTomb = urand(5000, 15000);
        m_sludge = urand(2000, 8000);
        m_enrage = 10 * MINUTE * IN_MILLISECONDS;

        m_creature->SetMeleeDamageSchool(SPELL_SCHOOL_FROST);
        m_creature->CastSpell(m_creature, SPELL_IMMUNITY_FROST, true);

        m_creature->SetAggroDistance(50.0f);
    }

    void Aggro(Unit* /*pWho*/) override
    {
        DoScriptText(SAY_AGGRO, m_creature);

        if (m_instance)
            m_instance->SetData(TYPE_HYDROSS, IN_PROGRESS);
    }

    void JustReachedHome() override
    {
        DespawnSummons();

        if (m_instance)
            m_instance->SetData(TYPE_HYDROSS, FAIL);
        m_beam = 1000;
    }

    void JustDied(Unit* /*pVictim*/) override
    {
        DespawnSummons();

        if (!m_corrupt)
            DoScriptText(SAY_CLEAN_DEATH, m_creature);
        else
            DoScriptText(SAY_CORRUPT_DEATH, m_creature);

        if (m_instance)
            m_instance->SetData(TYPE_HYDROSS, DONE);
    }

    void KilledUnit(Unit* victim) override
    {
        if (!m_corrupt)
            DoKillSay(m_creature, victim, SAY_CLEAN_KILL_1, SAY_CLEAN_KILL_2);
        else
            DoKillSay(
                m_creature, victim, SAY_CORRUPT_KILL_1, SAY_CORRUPT_KILL_2);
    }

    void ToggleHydrossBeam(bool on)
    {
        auto cs = GetCreatureListWithEntryInGrid(
            m_creature, NPC_HYDROSS_BEAM_HELPER, 120.0f);
        for (auto& c : cs)
            if (on)
                (c)->CastSpell(c, SPELL_HYDROSS_BEAM, false);
            else
                (c)->InterruptNonMeleeSpells(false);
    }

    void SummonSpawn(uint32 entry)
    {
        for (auto& spawnAngle : spawnAngles)
        {
            auto pos = m_creature->GetPoint(spawnAngle, 8.0f);
            m_creature->SummonCreature(entry, pos.x, pos.y, pos.z,
                m_creature->GetO(), TEMPSUMMON_DEAD_DESPAWN, 0);
        }
    }

    void JustSummoned(Creature* c) override
    {
        switch (c->GetEntry())
        {
        case NPC_RP_ELEMENTAL:
            c->SetActiveObjectState(true);
            return;
        case NPC_PURE_SPAWN:
        case NPC_TAINTED_SPAWN:
            break;
        default:
            return;
        }
        c->CastSpell(c, SPELL_ELEMENTAL_SPAWN_IN, true);
        c->SetInCombatWithZone();
        c->CastSpell(c, SPELL_STUN_SELF, true);
        m_spawns.push_back(c->GetObjectGuid());
    }

    void DespawnSummons()
    {
        for (auto& elem : m_spawns)
            if (Creature* c = m_creature->GetMap()->GetCreature(elem))
                c->Kill(c);
        m_spawns.clear();
    }

    void DoSpawn()
    {
        if (m_corrupt)
            SummonSpawn(NPC_TAINTED_SPAWN);
        else
            SummonSpawn(NPC_PURE_SPAWN);
        m_spawn = 0;
    }

    void UpdateAI(const uint32 diff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
        {
            if (m_beam)
            {
                if (m_beam <= diff)
                {
                    ToggleHydrossBeam(true);
                    m_beam = 0;
                }
                else
                    m_beam -= diff;
            }

            if (m_rpSummon <= diff)
            {
                DoCastSpellIfCan(
                    m_creature, SPELL_SUMMON_RP_ELEMENTAL, CAST_TRIGGERED);
                m_rpSummon = 12000;
            }
            else
                m_rpSummon -= diff;
            return;
        }

        // Enrage
        if (m_enrage)
        {
            if (m_enrage <= diff)
            {
                if (DoCastSpellIfCan(m_creature, SPELL_ENRAGE) == CAST_OK)
                {
                    DoScriptText(EMOTE_ENRAGE, m_creature);
                    m_enrage = 0;
                }
            }
            else
                m_enrage -= diff;
        }

        // Form changing
        float distance = m_creature->GetDistance(hydrossCenterPoint[0],
            hydrossCenterPoint[1], hydrossCenterPoint[2]);
        if (m_corrupt && distance <= WATER_FORM_RADIUS)
        {
            if (m_spawn)
                DoSpawn(); // Anti-cheat. In case you can manage to pull him
                           // back and forth before the 1 sec spawn timer
                           // expires

            DoResetThreat();
            DoScriptText(SAY_CLEAN_FORM, m_creature);
            ToggleHydrossBeam(true);
            m_creature->remove_auras(SPELL_NATURE_FORM);
            m_creature->remove_auras(SPELL_IMMUNITY_NATURE);
            m_creature->SetMeleeDamageSchool(SPELL_SCHOOL_FROST);
            m_creature->CastSpell(m_creature, SPELL_IMMUNITY_FROST, true);
            m_spawn = 1500;
            m_mark = 15000;
            m_markStack = 0;
            m_waterTomb = 1000; // Almost instantly after switch
            m_corrupt = false;
        }
        else if (!m_corrupt && distance >= NATURE_FORM_RADIUS)
        {
            if (m_spawn)
                DoSpawn(); // Anti-cheat. In case you can manage to pull him
                           // back and forth before the 1 sec spawn timer
                           // expires

            DoResetThreat();
            DoScriptText(SAY_CORRUPT_FORM, m_creature);
            ToggleHydrossBeam(false);
            m_creature->CastSpell(m_creature, SPELL_NATURE_FORM, true);
            m_creature->SetMeleeDamageSchool(SPELL_SCHOOL_NATURE);
            m_creature->remove_auras(SPELL_IMMUNITY_FROST);
            m_creature->CastSpell(m_creature, SPELL_IMMUNITY_NATURE, true);
            m_spawn = 1000;
            m_mark = 15000;
            m_markStack = 0;
            m_sludge = 1000; // Almost instantly after switch
            m_corrupt = true;
        }

        if (m_spawn)
        {
            if (m_spawn <= diff)
            {
                DoSpawn();
                m_spawn = 0;
            }
            else
                m_spawn -= diff;
        }

        // Mark of Hydross/Corruption
        if (m_mark <= diff)
        {
            uint32 spell = 0;
            if (m_corrupt)
                spell = markOfCorruption[m_markStack];
            else
                spell = markOfHydross[m_markStack];

            if (DoCastSpellIfCan(m_creature, spell) == CAST_OK)
            {
                m_mark = 15000;
                if (m_markStack + 1 < MAX_MARK)
                    ++m_markStack;
            }
        }
        else
            m_mark -= diff;

        // Cast form dependant spell
        if (!m_corrupt)
        {
            if (m_waterTomb <= diff)
            {
                if (Unit* tar = m_creature->SelectAttackingTarget(
                        ATTACKING_TARGET_RANDOM, 1))
                    if (DoCastSpellIfCan(tar, SPELL_WATER_TOMB) == CAST_OK)
                        m_waterTomb = urand(5000, 15000);
            }
            else
                m_waterTomb -= diff;
        }
        else
        {
            if (m_sludge <= diff)
            {
                if (Unit* tar = m_creature->SelectAttackingTarget(
                        ATTACKING_TARGET_RANDOM, 1))
                    if (DoCastSpellIfCan(tar, SPELL_VILE_SLUDGE) == CAST_OK)
                        m_sludge = urand(2000, 8000);
            }
            else
                m_sludge -= diff;
        }

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_hydross_the_unstable(Creature* pCreature)
{
    return new boss_hydross_the_unstableAI(pCreature);
}

void AddSC_boss_hydross_the_unstable()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_hydross_the_unstable";
    pNewScript->GetAI = &GetAI_boss_hydross_the_unstable;
    pNewScript->RegisterSelf();
}
