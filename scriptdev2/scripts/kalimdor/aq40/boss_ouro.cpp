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
#include "aq40.h"

enum
{
    SUBMERGE_TIMER = 10000,
    SUBMERGE_FORCE_TIMER = 90000,
    EMERGE_TIMER = 30000,

    SPELL_BIRTH = 26262,
    SPELL_SUMMON_SANDWORM_BASE = 26133,
    SPELL_OURO_SUBMERGE_VISUAL = 26063,
    SPELL_GROUND_RUPTURE = 26100,

    SPELL_SWEEP = 26103,
    SPELL_SAND_BLAST = 26102,
    SPELL_SUMMON_OURO_MOUNDS = 26058,

    SPELL_BERSERK = 26615,
    SPELL_BOULDER = 26616,

    NPC_DIRT_MOUND = 15712,

    PHASE_ONE = 1,
    PHASE_TWO = 2,
};

struct MANGOS_DLL_DECL boss_ouroAI : public ScriptedAI
{
    boss_ouroAI(Creature* creature) : ScriptedAI(creature)
    {
        instance = (ScriptedInstance*)creature->GetInstanceData();
        Reset();
    }

    ScriptedInstance* instance;
    ObjectGuid summoner;
    ObjectGuid base;
    std::vector<ObjectGuid> spawns;
    uint32 phase;
    uint32 submerge;
    uint32 force_submerge;
    uint32 emerge;
    uint32 ground_rupture;
    uint32 sweep;
    uint32 sand_blast;
    uint32 boulder;

    void Reset() override
    {
        phase = PHASE_ONE;
        submerge = SUBMERGE_TIMER;
        force_submerge = SUBMERGE_FORCE_TIMER;
        emerge = 0;
        ground_rupture = 0;
        sweep = 20000;
        sand_blast = 0;
        boulder = 6000;
    }

    void SummonedBy(WorldObject* obj) override
    {
        if (obj->GetTypeId() == TYPEID_UNIT)
        {
            summoner = obj->GetObjectGuid();
            m_creature->Kill(static_cast<Creature*>(obj));
        }

        m_creature->CastSpell(m_creature, SPELL_SUMMON_SANDWORM_BASE, true);
        m_creature->CastSpell(m_creature, SPELL_BIRTH, false);

        m_creature->SetInCombatWithZone();
    }

    void EnterEvadeMode(bool by_group) override
    {
        if (instance)
            instance->SetData(
                TYPE_OURO, NOT_STARTED); // Don't fail, reset encounter
        DespawnSummons();

        ScriptedAI::EnterEvadeMode(by_group);

        if (auto spawner = m_creature->GetMap()->GetCreature(summoner))
            spawner->queue_action(20000, [spawner]()
                {
                    spawner->Respawn();
                });

        if (auto base_obj = m_creature->GetMap()->GetGameObject(base))
            base_obj->Delete();

        m_creature->ForcedDespawn();
    }

    void JustDied(Unit* /*killer*/) override
    {
        if (instance)
            instance->SetData(TYPE_OURO, DONE);
        DespawnSummons();
    }

    void DespawnSummons()
    {
        for (auto& elem : spawns)
            if (Creature* c = m_creature->GetMap()->GetCreature(elem))
                c->Kill(c);
        spawns.clear();
    }

    void JustSummoned(GameObject* go) override { base = go->GetObjectGuid(); }

    void JustSummoned(Creature* c) override
    {
        spawns.push_back(c->GetObjectGuid());
        if (c->GetEntry() == NPC_DIRT_MOUND && phase == PHASE_TWO)
            c->AI()->Notify(10);
    }

    void SpellHit(Unit*, const SpellEntry* spell)
    {
        if (spell->Id == SPELL_OURO_SUBMERGE_VISUAL)
        {
            m_creature->CastSpell(m_creature, SPELL_SUMMON_OURO_MOUNDS, true);
            m_creature->SetFlag(UNIT_FIELD_FLAGS,
                UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE);
        }
    }

    bool do_submerge()
    {
        if (m_creature->IsNonMeleeSpellCasted(false))
            return false;

        Pacify(true);
        m_creature->CastSpell(m_creature, SPELL_OURO_SUBMERGE_VISUAL, false);
        emerge = EMERGE_TIMER;

        return true;
    }

    void do_emerge()
    {
        std::vector<Creature*> mounds;
        mounds.reserve(5);
        for (auto& spawn : spawns)
        {
            if (spawn.GetEntry() == NPC_DIRT_MOUND)
                if (auto c = m_creature->GetMap()->GetCreature(spawn))
                    mounds.push_back(c);
        }

        if (mounds.size() < 2)
            return;

        auto index = urand(0, mounds.size() - 1);
        mounds[index]->AI()->Notify(2);
        m_creature->NearTeleportTo(mounds[index]->GetX(), mounds[index]->GetY(),
            mounds[index]->GetZ(), mounds[index]->GetO());
        mounds.erase(mounds.begin() + index);
        for (auto mound : mounds)
            mound->AI()->Notify(1);

        m_creature->remove_auras(SPELL_OURO_SUBMERGE_VISUAL);
        m_creature->CastSpell(m_creature, SPELL_SUMMON_SANDWORM_BASE, true);
        m_creature->CastSpell(m_creature, SPELL_BIRTH, false);

        m_creature->RemoveFlag(UNIT_FIELD_FLAGS,
            UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE);

        ground_rupture = 2000;
        submerge = SUBMERGE_TIMER;
        force_submerge = SUBMERGE_FORCE_TIMER;
        sweep = 20000;
        sand_blast = 0;
    }

    void UpdateAI(uint32 diff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (emerge)
        {
            if (emerge <= diff)
            {
                do_emerge();
                emerge = 0;
            }
            else
                emerge -= diff;
        }

        if (ground_rupture)
        {
            if (ground_rupture <= diff)
            {
                m_creature->CastSpell(m_creature, SPELL_GROUND_RUPTURE, true);
                ground_rupture = 0;
                Pacify(false);
            }
            else
                ground_rupture -= diff;
        }

        // Don't update combat script while submerged
        if (IsPacified())
            return;

        if (phase == PHASE_ONE && m_creature->GetHealthPercent() < 21.0f)
        {
            m_creature->MonsterTextEmote(
                "%s goes into a berserker rage!", nullptr);
            m_creature->CastSpell(m_creature, SPELL_BERSERK, true);
            phase = PHASE_TWO;
        }

        if (sweep <= diff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_SWEEP) == CAST_OK)
            {
                sweep = 20000;
                sand_blast = urand(2000, 7000);
            }
        }
        else
            sweep -= diff;

        if (sand_blast)
        {
            if (sand_blast <= diff)
            {
                if (DoCastSpellIfCan(
                        m_creature->getVictim(), SPELL_SAND_BLAST) == CAST_OK)
                    sand_blast = 0;
            }
            else
                sand_blast -= diff;
        }

        for (auto& attacker : m_creature->getAttackers())
            if (m_creature->CanReachWithMeleeAttack(attacker))
            {
                submerge = SUBMERGE_TIMER;
                boulder = 6000;
                break;
            }

        if (phase == PHASE_ONE)
        {
            if (submerge <= diff)
            {
                if (do_submerge())
                    submerge = SUBMERGE_TIMER;
            }
            else
                submerge -= diff;

            if (force_submerge <= diff)
            {
                if (do_submerge())
                    force_submerge = SUBMERGE_FORCE_TIMER;
            }
            else
                force_submerge -= diff;
        }
        else
        {
            if (boulder <= diff)
            {
                if (auto tar = m_creature->SelectAttackingTarget(
                        ATTACKING_TARGET_RANDOM, 0, SPELL_BOULDER))
                    if (DoCastSpellIfCan(tar, SPELL_BOULDER) == CAST_OK)
                        boulder = urand(2000, 4000);
            }
            else
                boulder -= diff;
        }

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_ouro(Creature* creature)
{
    return new boss_ouroAI(creature);
}

void AddSC_boss_ouro()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_ouro";
    pNewScript->GetAI = &GetAI_boss_ouro;
    pNewScript->RegisterSelf();
}
