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
#include "molten_core.h"

enum
{
    // Yells
    SAY_INSECTS = -1409012,
    SAY_SUBMERGE = -1409013,
    SAY_BY_FIRE_BE_PURGED = -1409015,
    SAY_FLAMES_OF_SULFURON = -1409016,
    SAY_KILL = -1409017,

    // Ragnaros has the following auras (in addon):
    // 18373 - Self Root (spell not actually related to this event)
    // 21387 - Melt Weapon
    // 20563 - Elemental Fire

    // Phase 1 spells
    SPELL_MAGMA_BLAST = 20565,
    SPELL_WRATH_OF_RAGNAROS = 20566,
    SPELL_MIGHT_OF_RAGNAROS = 21154,
    // Phase 2 spells
    SPELL_SUBMERGED = 21107, // preceeded by EMOTE_ONESHOT_SUBMERGE
    SPELL_EMERGE = 20568,    // also used as part of intro RP
    // Phase 1 & 2 spells
    SPELL_LAVA_BURST_RANDOMIZER = 21908,

    SUBMERGE_TIMER = 180000,
    EMERGE_TIMER = 90000,

    NPC_SON_OF_FLAME = 12143,
};

static const std::array<G3D::Vector3, 8> sons_positions = {
    G3D::Vector3{837.3f, -790.7f, -230.3f},
    G3D::Vector3{874.4f, -885.3f, -231.1f},
    G3D::Vector3{875.0f, -856.2f, -230.3f},
    G3D::Vector3{827.4f, -874.8f, -230.8f},
    G3D::Vector3{850.5f, -799.4f, -231.1f},
    G3D::Vector3{851.1f, -903.7f, -230.4f},
    G3D::Vector3{810.9f, -817.3f, -230.4f},
    G3D::Vector3{866.2f, -811.6f, -231.1f},
};

struct MANGOS_DLL_DECL boss_ragnarosAI : public ScriptedAI
{
    boss_ragnarosAI(Creature* creature) : ScriptedAI(creature)
    {
        instance = (ScriptedInstance*)creature->GetInstanceData();
        Reset();
        insects_yell = 0;
        release = 0;
    }

    ScriptedInstance* instance;
    std::vector<ObjectGuid> spawns;
    // RP timers
    uint32 insects_yell;
    uint32 release;
    // In combat timers
    uint32 lava_burst;
    uint32 magma_blast;
    uint32 wrath;
    uint32 might;
    uint32 submerge;
    uint32 emerge;
    uint32 remove_pacify;
    bool submerge_yelled;

    void Reset() override
    {
        lava_burst = urand(10000, 15000);
        magma_blast = 15000;
        wrath = 25000;
        might = urand(10000, 13000);
        submerge = SUBMERGE_TIMER;
        emerge = 0;
        remove_pacify = 0;
        submerge_yelled = false;
    }

    void SummonedBy(WorldObject*) override
    {
        m_creature->SetFlag(
            UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE);
        m_creature->CastSpell(m_creature, SPELL_EMERGE, false);
    }

    void SpellHitTarget(Unit*, const SpellEntry* spell)
    {
        if (spell->Id == 19773)
        {
            insects_yell = 9000;
            release = 13000;
        }
    }

    void Aggro(Unit* /*who*/) override
    {
        if (instance)
            instance->SetData(TYPE_RAGNAROS, IN_PROGRESS);
    }

    void KilledUnit(Unit* unit) override
    {
        DoKillSay(m_creature, unit, SAY_KILL);
    }

    void EnterEvadeMode(bool by_group) override
    {
        if (instance)
            instance->SetData(TYPE_RAGNAROS, FAIL);
        DespawnSummons();
        m_creature->RemoveFlag(UNIT_FIELD_FLAGS,
            UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE);
        ScriptedAI::EnterEvadeMode(by_group);
    }

    void JustDied(Unit* /*killer*/) override
    {
        if (instance)
            instance->SetData(TYPE_RAGNAROS, DONE);
        DespawnSummons();
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

    void update_release(uint32 diff)
    {
        if (insects_yell)
        {
            if (insects_yell <= diff)
            {
                DoScriptText(SAY_INSECTS, m_creature);
                insects_yell = 0;
            }
            else
                insects_yell -= diff;
        }
        if (release)
        {
            if (release <= diff)
            {
                m_creature->RemoveFlag(UNIT_FIELD_FLAGS,
                    UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE);
                m_creature->SetInCombatWithZone();
                release = 0;
            }
            else
                release -= diff;
        }
    }

    void UpdateAI(uint32 diff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
        {
            update_release(diff);
            return;
        }

        if (lava_burst <= diff)
        {
            auto count = urand(2, 3);
            auto c = m_creature;
            for (uint32 i = 0; i < count; ++i)
                c->queue_action(i * 3000, [c]()
                    {
                        c->CastSpell(c, SPELL_LAVA_BURST_RANDOMIZER, true);
                    });
            lava_burst = urand(15000, 25000);
        }
        else
            lava_burst -= diff;

        if (emerge)
        {
            if (emerge <= diff)
            {
                m_creature->CastSpell(m_creature, SPELL_EMERGE, false);
                m_creature->remove_auras(SPELL_SUBMERGED);
                emerge = 0;
                submerge = SUBMERGE_TIMER;
                submerge_yelled = false;
                remove_pacify = 3000;
                magma_blast = 4000;
            }
            else
            {
                emerge -= diff;
                auto adds = GetCreatureListWithEntryInGrid(
                    m_creature, NPC_SON_OF_FLAME, 100.0f);
                bool adds_dead = true;
                for (auto& add : adds)
                    if (add->isAlive())
                    {
                        adds_dead = false;
                        break;
                    }
                if (adds_dead)
                    emerge = 1;
            }
        }

        if (submerge)
        {
            if (submerge <= diff)
            {
                if (CanCastSpell(m_creature, SPELL_SUBMERGED, false) == CAST_OK)
                {
                    if (submerge_yelled)
                    {
                        m_creature->CastSpell(
                            m_creature, SPELL_SUBMERGED, false);
                        emerge = EMERGE_TIMER;
                        submerge = 0;
                        return;
                    }
                    else
                    {
                        if (IsPacified())
                        {
                            submerge = 2000;
                            m_creature->HandleEmote(EMOTE_ONESHOT_SUBMERGE);
                            DoScriptText(SAY_SUBMERGE, m_creature);
                            submerge_yelled = true;
                            for (size_t i = 0; i < sons_positions.size(); ++i)
                                m_creature->SummonCreature(NPC_SON_OF_FLAME,
                                    sons_positions[i].x, sons_positions[i].y,
                                    sons_positions[i].z,
                                    m_creature->GetAngle(sons_positions[i].x,
                                        sons_positions[i].y) +
                                        M_PI_F,
                                    TEMPSUMMON_MANUAL_DESPAWN, 0);
                        }
                        else
                        {
                            m_creature->SetFacingToObject(
                                m_creature->getVictim());
                            m_creature->SetFlag(
                                UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE |
                                                      UNIT_FLAG_NOT_SELECTABLE);
                            Pacify(true);
                            submerge = 1000;
                        }
                    }
                }
            }
            else
                submerge -= diff;
        }

        if (remove_pacify)
        {
            if (remove_pacify <= diff)
            {
                m_creature->RemoveFlag(UNIT_FIELD_FLAGS,
                    UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE);
                Pacify(false);
                remove_pacify = 0;
            }
            else
                remove_pacify -= diff;
        }

        // Don't update combat script while submerged
        if (submerge_yelled || IsPacified())
            return;

        if (wrath <= diff)
        {
            if (m_creature->CanReachWithMeleeAttack(m_creature->getVictim()) &&
                DoCastSpellIfCan(m_creature, SPELL_WRATH_OF_RAGNAROS) ==
                    CAST_OK)
            {
                DoScriptText(SAY_FLAMES_OF_SULFURON, m_creature);
                wrath = 25000;
            }
        }
        else
            wrath -= diff;

        if (might <= diff)
        {
            if (auto target =
                    m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM,
                        0, SPELL_MIGHT_OF_RAGNAROS, SELECT_FLAG_POWER_MANA))
            {
                if (DoCastSpellIfCan(target, SPELL_MIGHT_OF_RAGNAROS) ==
                    CAST_OK)
                {
                    if (urand(0, 1))
                        DoScriptText(SAY_BY_FIRE_BE_PURGED, m_creature);
                    might = urand(10000, 13000);
                }
            }
        }
        else
            might -= diff;

        // On retail he stops buffeting when someone is in his melee
        // range and has auto attack toggle on (even if said person is
        // not hitting Ragnaros).
        for (auto& attacker : m_creature->getAttackers())
            if (m_creature->CanReachWithMeleeAttack(attacker))
            {
                magma_blast = 3000;
                break;
            }
        if (magma_blast <= diff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_MAGMA_BLAST) ==
                CAST_OK)
                magma_blast = urand(2000, 4000);
        }
        else
            magma_blast -= diff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_ragnaros(Creature* creature)
{
    return new boss_ragnarosAI(creature);
}

void AddSC_boss_ragnaros()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_ragnaros";
    pNewScript->GetAI = &GetAI_boss_ragnaros;
    pNewScript->RegisterSelf();
}
