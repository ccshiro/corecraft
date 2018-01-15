/* Copyright (C) 2015 Corecraft <https://www.worldofcorecraft.com/>
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

#include "precompiled.h"
#include "zulgurub.h"

enum
{
    SAY_DING = -1100000,
    SAY_WATCHING = -1100001,
    SAY_PUNISH = -1100002,
    SAY_ENGAGE = -1100003,
    SAY_RAPTOR_FALL = -1100004,
    SAY_JINDO_GRATS = -1100005,

    SPELL_LEVEL_UP = 24312,
    SPELL_THREATENING_GAZE = 24314,
    SPELL_CHARGE_EXECUTE = 24315,
    SPELL_CHARGE = 24408,
    SPELL_ENRAGE = 24318,
    SPELL_FEAR = 16508,

    NPC_OHGAN = 14988,
};

struct Pos
{
    float x, y, z, o;
};

// Positions taken from TrinityCore, they've mined the correct locations while
// the encounter still existed it seems (checked it against vanilla videos).
const Pos chained_spirits[] = {
    {-12167.17f, -1979.330f, 133.0992f, 2.268928f},
    {-12262.74f, -1953.394f, 133.5496f, 0.593412f},
    {-12176.89f, -1983.068f, 133.7841f, 2.129302f},
    {-12226.45f, -1977.933f, 132.7982f, 1.466077f},
    {-12204.74f, -1890.431f, 135.7569f, 4.415683f},
    {-12216.70f, -1891.806f, 136.3496f, 4.677482f},
    {-12236.19f, -1892.034f, 134.1041f, 5.044002f},
    {-12248.24f, -1893.424f, 134.1182f, 5.270895f},
    {-12257.36f, -1897.663f, 133.1484f, 5.462881f},
    {-12265.84f, -1903.077f, 133.1649f, 5.654867f},
    {-12158.69f, -1972.707f, 133.8751f, 2.408554f},
    {-12178.82f, -1891.974f, 134.1786f, 3.944444f},
    {-12193.36f, -1890.039f, 135.1441f, 4.188790f},
    {-12275.59f, -1932.845f, 134.9017f, 0.174533f},
    {-12273.51f, -1941.539f, 136.1262f, 0.314159f},
    {-12247.02f, -1963.497f, 133.9476f, 0.872665f},
    {-12238.68f, -1969.574f, 133.6273f, 1.134464f},
    {-12192.78f, -1982.116f, 132.6966f, 1.919862f},
    {-12210.81f, -1979.316f, 133.8700f, 1.797689f},
    {-12283.51f, -1924.839f, 133.5170f, 0.069813f},
};

struct MANGOS_DLL_DECL boss_mandokirAI : public Scripted_BehavioralAI
{
    boss_mandokirAI(Creature* creature) : Scripted_BehavioralAI(creature)
    {
        instance = (ScriptedInstance*)creature->GetInstanceData();
        Reset();

        m_creature->movement_gens.push(
            new movement::StoppedMovementGenerator());
        m_creature->SetFlag(
            UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE);
    }

    ScriptedInstance* instance;
    std::vector<ObjectGuid> spawns;
    uint32 kills;
    uint32 kills_to_ding;
    uint32 watch;
    ObjectGuid watching;
    float watching_threat;

    void Reset() override
    {
        Pacify(true);
        kills = 0;
        kills_to_ding = 2;
        watch = 5000;
        watching = ObjectGuid();
        watching_threat = 0;

        Scripted_BehavioralAI::Reset();
    }

    void DespawnSummons()
    {
        for (auto& elem : spawns)
            if (Creature* c = m_creature->GetMap()->GetCreature(elem))
                c->Kill(c);
        spawns.clear();
    }

    void JustSummoned(Creature* c) override
    {
        spawns.push_back(c->GetObjectGuid());
    }

    void Aggro(Unit* /*who*/) override
    {
        if (instance)
            instance->SetData(TYPE_MANDOKIR, IN_PROGRESS);
    }

    void KilledUnit(Unit* unit) override
    {
        if (unit->GetTypeId() != TYPEID_PLAYER)
            return;

        if (++kills >= kills_to_ding)
        {
            kills = 0;
            kills_to_ding += 1;
            m_creature->CastSpell(m_creature, SPELL_LEVEL_UP, true);
            DoScriptText(SAY_DING, m_creature);
            if (instance && instance->GetData(TYPE_JINDO) != DONE)
            {
                auto c = m_creature;
                c->queue_action(4000, [c]()
                    {
                        DoOrSimulateScriptTextForMap(
                            SAY_JINDO_GRATS, NPC_JINDO, c->GetMap());
                    });
            }
        }
    }

    void JustReachedHome() override
    {
        if (instance)
            instance->SetData(TYPE_MANDOKIR, FAIL);
        DespawnSummons();
        ScriptedAI::JustReachedHome();

        m_creature->SetFlag(
            UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE);
        m_creature->movement_gens.push(
            new movement::StoppedMovementGenerator());
    }

    void JustDied(Unit* /*killer*/) override
    {
        if (instance)
            instance->SetData(TYPE_MANDOKIR, DONE);
        DespawnSummons();
    }

    void Notify(uint32 id, Unit*) override
    {
        if (id == 1)
        {
            // Engage Mandokir
            m_creature->movement_gens.push(new movement::PointMovementGenerator(
                1, -12197.6, -1949.6, 130.3, true, true));
            m_creature->movement_gens.remove_all(movement::gen::stopped);
        }
        else if (id == 2)
        {
            DoScriptText(SAY_RAPTOR_FALL, m_creature);
            m_creature->CastSpell(m_creature, SPELL_ENRAGE, true);
        }
    }

    void SpellHitTarget(Unit* target, const SpellEntry* spell) override
    {
        if (spell->Id == SPELL_THREATENING_GAZE)
        {
            watching = target->GetObjectGuid();
            watching_threat = m_creature->getThreatManager().getThreat(target);
        }
        else if (spell->Id == SPELL_CHARGE)
        {
            DoCastSpellIfCan(target, SPELL_FEAR);
        }
    }

    void MovementInform(movement::gen type, uint32 id) override
    {
        if (type == movement::gen::point && id == 1)
        {
            DoScriptText(SAY_ENGAGE, m_creature);
            m_creature->Unmount();
            m_creature->SummonCreature(NPC_OHGAN, m_creature->GetX(),
                m_creature->GetY(), m_creature->GetZ(), m_creature->GetO(),
                TEMPSUMMON_CORPSE_TIMED_DESPAWN, 4000);
            Pacify(false);
            m_creature->RemoveFlag(
                UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE);

            // Summon ghosts
            for (auto pos : chained_spirits)
                m_creature->SummonCreature(NPC_CHAINED_SPIRIT, pos.x, pos.y,
                    pos.z, pos.o, TEMPSUMMON_MANUAL_DESPAWN, 0);
        }
    }

    void UpdateAI(uint32 diff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (IsPacified())
            return;

        if (watch <= diff)
        {
            if (CanCastSpell(m_creature, SPELL_THREATENING_GAZE, false) ==
                CAST_OK)
            {
                if (auto target = m_creature->SelectAttackingTarget(
                        ATTACKING_TARGET_RANDOM, 0, SPELL_THREATENING_GAZE))
                {
                    if (DoCastSpellIfCan(target, SPELL_THREATENING_GAZE) ==
                        CAST_OK)
                    {
                        DoScriptText(SAY_WATCHING, m_creature, target);
                        watch = 8000;
                    }
                }
            }
        }
        else
            watch -= diff;

        if (auto watched = m_creature->GetMap()->GetPlayer(watching))
        {
            if (watching_threat !=
                m_creature->getThreatManager().getThreat(watched))
            {
                DoScriptText(SAY_PUNISH, m_creature, watched);
                m_creature->InterruptNonMeleeSpells(false);
                m_creature->CastSpell(watched, SPELL_CHARGE_EXECUTE, true);
                watching = ObjectGuid();
            }
        }

        Scripted_BehavioralAI::UpdateInCombatAI(diff);
        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_mandokir(Creature* creature)
{
    return new boss_mandokirAI(creature);
}

void AddSC_boss_mandokir()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_mandokir";
    pNewScript->GetAI = &GetAI_boss_mandokir;
    pNewScript->RegisterSelf();
}
