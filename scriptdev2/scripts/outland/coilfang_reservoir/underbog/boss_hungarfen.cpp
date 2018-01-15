/* Copyright (C) Corecraft
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
SDName: Boss_Hungarfen
SD%Complete: 100
SDComment:
SDCategory: Coilfang Resevoir, Underbog
EndScriptData */

#include "precompiled.h"

enum
{
    SPELL_FOUL_SPORES = 31673,
    SPELL_ACID_GEYSER = 38739,
    SPELL_DESPAWN_MUSHROOMS = 34874, // Not implemented, we do it manually

    // Mushroom spells
    SPELL_SPORE_CLOUD = 34168,
    SPELL_PUTRID_MUSHROOM = 31690,
    SPELL_GROW = 31698,

    NPC_UNDERBOG_MUSHROOM = 17990,
};

struct MANGOS_DLL_DECL boss_hungarfenAI : public ScriptedAI
{
    boss_hungarfenAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();
        Reset();
    }

    bool m_bIsRegularMode;
    bool m_bHasSpores;
    uint32 m_uiMushroomTimer;
    uint32 m_uiAcidGeyserTimer;
    std::vector<ObjectGuid> m_spawnedMushrooms;

    void DespawnMushrooms()
    {
        for (auto& elem : m_spawnedMushrooms)
            if (Creature* mush = m_creature->GetMap()->GetCreature(elem))
                mush->ForcedDespawn();
    }

    void JustDied(Unit* /*pKiller*/) override { DespawnMushrooms(); }

    void JustReachedHome() override { DespawnMushrooms(); }

    void Reset() override
    {
        m_bHasSpores = false;
        m_uiMushroomTimer = 5000; // 1 mushroom after 5s, then one per 10s.
                                  // Heroic seems between 1 and 5 seconds
        m_uiAcidGeyserTimer = 10000;
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (m_creature->GetHealthPercent() <= 20.0f && !m_bHasSpores)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_FOUL_SPORES) == CAST_OK)
                m_bHasSpores = true;
        }

        if (m_uiMushroomTimer < uiDiff)
        {
            // Summon a mushroom exactly on target position
            if (Unit* pTarget = m_creature->SelectAttackingTarget(
                    ATTACKING_TARGET_RANDOM, 0))
                if (Creature* mush = m_creature->SummonCreature(
                        NPC_UNDERBOG_MUSHROOM, pTarget->GetX(), pTarget->GetY(),
                        pTarget->GetZ(), 0, TEMPSUMMON_CORPSE_DESPAWN, 0))
                    m_spawnedMushrooms.push_back(mush->GetObjectGuid());

            m_uiMushroomTimer = m_bIsRegularMode ? 10000 : urand(1000, 5000);
        }
        else
            m_uiMushroomTimer -= uiDiff;

        if (m_uiAcidGeyserTimer < uiDiff && !m_bIsRegularMode)
        {
            if (Unit* pTarget = m_creature->SelectAttackingTarget(
                    ATTACKING_TARGET_RANDOM, 0))
            {
                if (DoCastSpellIfCan(pTarget, SPELL_ACID_GEYSER) == CAST_OK)
                    m_uiAcidGeyserTimer = urand(10000, 17500);
            }
        }
        else
            m_uiAcidGeyserTimer -= uiDiff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_hungarfen(Creature* pCreature)
{
    return new boss_hungarfenAI(pCreature);
}

struct MANGOS_DLL_DECL mob_underbog_mushroomAI : public ScriptedAI
{
    mob_underbog_mushroomAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        Reset();
    }

    uint32 m_uiGrowTimer;
    uint32 m_uiShrinkTimer;
    uint32 m_uiSporeTimer;
    uint32 m_uiDespawnAfterShrinkTimer;

    void Reset() override
    {
        m_uiGrowTimer = 1000;
        m_uiSporeTimer = 15000;
        m_uiShrinkTimer = 20000;
        m_uiDespawnAfterShrinkTimer = 0;

        DoCastSpellIfCan(m_creature, SPELL_PUTRID_MUSHROOM);
    }

    void MoveInLineOfSight(Unit* /*pWho*/) override {}
    void AttackStart(Unit* /*pWho*/) override {}
    void AttackedBy(Unit* /*pAttacker*/) override {}

    void UpdateAI(const uint32 uiDiff) override
    {
        if (m_uiSporeTimer)
        {
            if (m_uiSporeTimer <= uiDiff)
            {
                if (DoCastSpellIfCan(m_creature, SPELL_SPORE_CLOUD) == CAST_OK)
                {
                    m_creature->SetTargetGuid(ObjectGuid());
                    m_uiGrowTimer = 0;
                    m_uiSporeTimer = 0;
                }
            }
            else
                m_uiSporeTimer -= uiDiff;
        }

        if (m_uiGrowTimer)
        {
            if (m_uiGrowTimer <= uiDiff)
            {
                if (DoCastSpellIfCan(m_creature, SPELL_GROW) == CAST_OK)
                    m_uiGrowTimer = 3000;
            }
            else
                m_uiGrowTimer -= uiDiff;
        }

        if (m_uiShrinkTimer)
        {
            if (m_uiShrinkTimer <= uiDiff)
            {
                m_creature->remove_auras(SPELL_GROW);
                m_uiShrinkTimer = 0;
                m_uiDespawnAfterShrinkTimer = 1650;
            }
            else
                m_uiShrinkTimer -= uiDiff;
        }

        if (m_uiDespawnAfterShrinkTimer)
        {
            if (m_uiDespawnAfterShrinkTimer <= uiDiff)
            {
                m_creature->SetVisibility(VISIBILITY_OFF);
                m_uiDespawnAfterShrinkTimer = 0;
            }
            else
                m_uiDespawnAfterShrinkTimer -= uiDiff;
        }
    }
};

CreatureAI* GetAI_mob_underbog_mushroom(Creature* pCreature)
{
    return new mob_underbog_mushroomAI(pCreature);
}

void AddSC_boss_hungarfen()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_hungarfen";
    pNewScript->GetAI = &GetAI_boss_hungarfen;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "mob_underbog_mushroom";
    pNewScript->GetAI = &GetAI_mob_underbog_mushroom;
    pNewScript->RegisterSelf();
}
