/* Copyright (C) 2013 Corecraft <https://www.worldofcorecraft.com>
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
SDName: Boss_Magtheridon
SD%Complete: 100
SDComment:
SDCategory: Hellfire Citadel, Magtheridon's lair
EndScriptData */

#include "magtheridons_lair.h"
#include "precompiled.h"

enum
{
    SAY_OOC_1 = -1544000,
    SAY_OOC_2 = -1544001,
    SAY_OOC_3 = -1544002,
    SAY_OOC_4 = -1544003,
    SAY_OOC_5 = -1544004,
    SAY_OOC_6 = -1544005,
    SAY_UNUSED = -1544007,
    SAY_SHADOW_CAGE = -1544008,
    SAY_COLLAPSE_PRISON = -1544009,
    SAY_KILL = -1544010,
    SAY_DEATH = -1544011,

    SPELL_BLAST_NOVA = 30616,
    SPELL_QUAKE = 30576,
    SPELL_CLEAVE = 30619,
    SPELL_BLAZE_FIREBALL = 30541,
    // Following spells used by the Blaze creature's Smart AI script:
    SPELL_BLAZE = 30542,
    SPELL_CONFLAGRATION = 30757,
    // Collapse spells:
    SPELL_CAMERA_SHAKE = 36455,
    SPELL_DEBRIS_BIG_ONE = 36449,
    // Spells Casted by our NPC_DEBRIS
    NPC_DEBRIS = 100034,
    SPELL_DEBRIS_PRE = 30632,
    SPELL_DEBRIS_DMG = 30631,

    SPELL_ENRAGE = 27680,

    // Useful for the collapse part
    SPELL_ROOT = 42716
};

struct MANGOS_DLL_DECL boss_magtheridonAI : public ScriptedAI
{
    boss_magtheridonAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_instance = (ScriptedInstance*)pCreature->GetInstanceData();
        Reset();

        m_creature->movement_gens.push(new movement::StoppedMovementGenerator(),
            movement::EVENT_LEAVE_COMBAT);
    }

    ScriptedInstance* m_instance;
    uint32 m_oocSay;
    uint32 m_nova;
    uint32 m_debris;
    uint32 m_quake;
    uint32 m_cleave;
    uint32 m_conflagration;
    bool m_collapsedRoom;
    uint32 m_collapseTimer;
    uint32 m_collapsePhase;
    uint32 m_enrage;

    void Reset() override
    {
        m_oocSay = 30000;
        m_nova = urand(50000, 60000);
        m_debris = urand(10000, 20000);
        m_quake = urand(30000, 40000);
        m_cleave = urand(8000, 12000);
        m_conflagration = 20000;
        m_collapsedRoom = false;
        m_collapseTimer = 0;
        m_collapsePhase = 0;
        m_enrage = 20 * MINUTE * IN_MILLISECONDS;

        m_creature->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE |
                                                  UNIT_FLAG_PASSIVE |
                                                  UNIT_FLAG_NOT_SELECTABLE);
        Pacify(true);
    }

    void JustReachedHome() override
    {
        if (m_instance)
            m_instance->SetData(TYPE_MAGTHERIDON, FAIL);

        if (!m_creature->movement_gens.has(movement::gen::stopped))
            m_creature->movement_gens.push(
                new movement::StoppedMovementGenerator(),
                movement::EVENT_LEAVE_COMBAT);
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        DoScriptText(SAY_DEATH, m_creature);

        if (m_instance)
            m_instance->SetData(TYPE_MAGTHERIDON, DONE);
    }

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_KILL);
    }

    void SpellHit(Unit* /*pCaster*/, const SpellEntry* pSpell) override
    {
        if (pSpell->Id == SPELL_SHADOW_CAGE)
            DoScriptText(SAY_SHADOW_CAGE, m_creature);
    }

    Creature* GetBlazeTarget(uint32 cooldown)
    {
        auto inst = dynamic_cast<instance_magtheridons_lair*>(m_instance);
        if (!inst)
            return nullptr;

        std::vector<size_t> indices;
        indices.reserve(inst->m_blazes.size());

        // Gather index of all available targets
        for (size_t i = 0; i < inst->m_blazes.size(); ++i)
        {
            if (!inst->m_blazes[i].second ||
                inst->m_blazes[i].second < WorldTimer::time_no_syscall())
            {
                if (inst->m_blazes[i].second)
                {
                    if (Creature* c = m_creature->GetMap()->GetCreature(
                            inst->m_blazes[i].first))
                        c->SetVisibility(VISIBILITY_ON);
                    inst->m_blazes[i].second = 0;
                }
                indices.push_back(i);
            }
        }

        if (indices.empty())
            return nullptr;

        auto* chosen = &inst->m_blazes[indices[urand(0, indices.size() - 1)]];
        Creature* c = m_creature->GetMap()->GetCreature(chosen->first);

        // Pick a random target close to a player, or if none found, just any
        // random target
        do
        {
            auto index = urand(0, indices.size() - 1);
            auto* pair = &inst->m_blazes[indices[index]];
            if (auto inner = m_creature->GetMap()->GetCreature(pair->first))
            {
                if (GetClosestPlayer(inner, 15.0f) != nullptr)
                {
                    c = inner;
                    chosen = pair;
                    break;
                }
            }

            indices.erase(indices.begin() + index);
        } while (indices.size() > 0);

        if (c && chosen)
            chosen->second = WorldTimer::time_no_syscall() + cooldown;

        return c;
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
        {
            if (m_oocSay <= uiDiff)
            {
                switch (urand(0, 5))
                {
                case 0:
                    DoScriptText(SAY_OOC_1, m_creature);
                    break;
                case 1:
                    DoScriptText(SAY_OOC_2, m_creature);
                    break;
                case 2:
                    DoScriptText(SAY_OOC_3, m_creature);
                    break;
                case 3:
                    DoScriptText(SAY_OOC_4, m_creature);
                    break;
                case 4:
                    DoScriptText(SAY_OOC_5, m_creature);
                    break;
                case 5:
                    DoScriptText(SAY_OOC_6, m_creature);
                    break;
                }
                m_oocSay = 3 * MINUTE * IN_MILLISECONDS;
            }
            else
                m_oocSay -= uiDiff;

            return;
        }

        // Don't update script while waiting for release
        if (IsPacified())
            return;

        if (!m_collapsedRoom && m_creature->GetHealthPercent() < 31.0f &&
            !m_creature->IsNonMeleeSpellCasted(false))
        {
            DoScriptText(SAY_COLLAPSE_PRISON, m_creature);
            DoCastSpellIfCan(m_creature, SPELL_ROOT, CAST_TRIGGERED);
            m_collapsedRoom = true;
            m_collapseTimer = 5000;
            m_collapsePhase = 1;
        }

        if (m_collapseTimer && m_instance)
        {
            if (m_collapseTimer <= uiDiff)
            {
                switch (m_collapsePhase)
                {
                case 1:
                    DoCastSpellIfCan(m_creature, SPELL_CAMERA_SHAKE);
                    m_instance->DoUseDoorOrButton(GO_RAID_FX);
                    m_collapseTimer = 8000;
                    break;
                case 2:
                    m_creature->remove_auras(SPELL_ROOT);
                    m_instance->DoUseDoorOrButton(GO_COLUMN_0);
                    m_instance->DoUseDoorOrButton(GO_COLUMN_1);
                    m_instance->DoUseDoorOrButton(GO_COLUMN_2);
                    m_instance->DoUseDoorOrButton(GO_COLUMN_3);
                    m_instance->DoUseDoorOrButton(GO_COLUMN_4);
                    m_instance->DoUseDoorOrButton(GO_COLUMN_5);
                    m_collapseTimer = 3500;
                    break;
                case 3:
                    if (DoCastSpellIfCan(m_creature, SPELL_DEBRIS_BIG_ONE,
                            CAST_TRIGGERED) != CAST_OK)
                        return;
                    break;
                case 4:
                    m_collapseTimer = 0;
                    break;
                }
                ++m_collapsePhase;
            }
            else
                m_collapseTimer -= uiDiff;

            // During phase 0 -> 2 we do not continue with our normal logic.
            if (m_collapsePhase < 3)
                return;
        }

        if (m_enrage)
        {
            if (m_enrage <= uiDiff)
            {
                if (DoCastSpellIfCan(m_creature, SPELL_ENRAGE) == CAST_OK)
                    m_enrage = 0;
            }
            else
                m_enrage -= uiDiff;
        }

        if (m_nova <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_BLAST_NOVA) == CAST_OK)
                m_nova = urand(50000, 60000);
        }
        else
            m_nova -= uiDiff;

        if (m_collapsedRoom)
        {
            if (m_debris <= uiDiff)
            {
                // Use the blazes to determine a good spot -- so we dont end up
                // in a fire with our debris
                if (auto target = GetBlazeTarget(8))
                {
                    float x, y, z;
                    target->GetPosition(x, y, z);
                    m_creature->SummonCreature(NPC_DEBRIS, x, y, z, 0.0f,
                        TEMPSUMMON_TIMED_DESPAWN, 8000);
                    m_debris = urand(10000, 20000);
                }
            }
            else
                m_debris -= uiDiff;
        }

        if (m_quake <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_QUAKE) ==
                CAST_OK)
                m_quake = urand(50000, 60000);
        }
        else
            m_quake -= uiDiff;

        if (m_cleave <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_CLEAVE) ==
                CAST_OK)
                m_cleave = urand(8000, 12000);
        }
        else
            m_cleave -= uiDiff;

        if (m_conflagration <= uiDiff)
        {
            // Pick a blaze location and mark it as not reusable for 2 minutes
            // and 10 seconds.
            if (CanCastSpell(m_creature, SPELL_BLAZE_FIREBALL, false) ==
                CAST_OK)
            {
                if (auto target = GetBlazeTarget(2 * 60 + 10))
                {
                    m_creature->CastSpell(target, SPELL_BLAZE_FIREBALL, false);
                    m_conflagration = 20000;
                }
            }
        }
        else
            m_conflagration -= uiDiff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_magtheridon(Creature* pCreature)
{
    return new boss_magtheridonAI(pCreature);
}

bool GOUse_manticron_cube(Player* p, GameObject* go)
{
    // Pressing with the aura results in interrupting the channel
    if (p->has_aura(SPELL_SHADOW_GRASP))
        p->InterruptNonMeleeSpells(false);
    else if (instance_magtheridons_lair* instance =
                 dynamic_cast<instance_magtheridons_lair*>(
                     go->GetInstanceData()))
        instance->OnCubeClick(go, p);

    return true;
}

void AddSC_boss_magtheridon()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_magtheridon";
    pNewScript->GetAI = &GetAI_boss_magtheridon;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "go_manticron_cube";
    pNewScript->pGOUse = &GOUse_manticron_cube;
    pNewScript->RegisterSelf();
}
