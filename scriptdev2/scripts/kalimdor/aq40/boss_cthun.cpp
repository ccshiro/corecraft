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
#include "maps/visitors.h"

enum
{
    AGGRO_DIST = 95,

    SPELL_ROOT_SELF = 23973,
    SPELL_TRANSFORM = 26232,
    SPELL_CARAPACE = 26156,
    SPELL_VULNERABLE = 26235,
    SPELL_MOUTH_TENTACLE = 26332,
    SPELL_DIGESTIVE_ACID_TP = 26220,
    SPELL_DIGESTIVE_ACID = 26476,

    SPELL_EYE_BEAM = 26134,

    EMOTE_WEAKENED = -1531011,

    NPC_EYE_OF_CTHUN = 15589,
    NPC_CLAW_TENTACLE = 15725,
    NPC_EYE_TENTACLE = 15726,
    NPC_TENTACLE_PORTAL = 15904,
    NPC_GIANT_CLAW_TENTACLE = 15728,
    NPC_GIANT_EYE_TENTACLE = 15334,
    NPC_GIANT_TENTACLE_PORTAL = 15910,
    NPC_FLESH_TENTACLE = 15802,

    PHASE_EYE = 1,
    PHASE_CTHUN = 2,
    PHASE_VULNERABLE = 3,
    PHASE_TRANSITION = 100,
};

struct MANGOS_DLL_DECL boss_cthunAI : public ScriptedAI
{
    boss_cthunAI(Creature* creature) : ScriptedAI(creature)
    {
        instance = (ScriptedInstance*)creature->GetInstanceData();
        Reset();
        summoned_eye = false;
    }

    bool summoned_eye;
    ScriptedInstance* instance;
    uint32 phase;
    uint32 transition_timer;
    // Phase One
    ObjectGuid eye;
    uint32 tentacle;
    uint32 eye_tentacles;
    uint32 eye_beam;
    // Phase two
    uint32 giant_claw;
    uint32 giant_eye;
    uint32 mouth_tentacle;
    uint32 dead_flesh_tentacles;
    bool summoned_flesh_tentacles;

    void Reset() override
    {
        m_creature->SetFlag(UNIT_FIELD_FLAGS,
            UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE);
        Pacify(true);
        m_creature->CastSpell(m_creature, SPELL_ROOT_SELF, true);

        phase = PHASE_EYE;
        transition_timer = 0;
        tentacle = urand(5000, 10000);
        eye_tentacles = 45000;

        giant_claw = 7000;
        giant_eye = 20000;
        mouth_tentacle = 15000;
        dead_flesh_tentacles = 0;
        summoned_flesh_tentacles = false;
    }

    void JustSummoned(Creature* c) override
    {
        if (c->GetEntry() == NPC_EYE_OF_CTHUN)
            eye = c->GetObjectGuid();
    }

    Player* select_summon_target()
    {
        m_creature->SetInCombatWithZone();

        std::vector<Player*> set;
        const ThreatList& tl = m_creature->getThreatManager().getThreatList();
        for (auto& ref : tl)
        {
            auto target = ref->getTarget();
            if (target->GetTypeId() == TYPEID_PLAYER)
            {
                auto player = static_cast<Player*>(target);
                if (!player->isGameMaster() && !player->IsKnockbacked() &&
                    player->isAlive() &&
                    !player->has_aura(SPELL_MOUTH_TENTACLE) &&
                    !player->has_aura(SPELL_DIGESTIVE_ACID))
                {
                    set.push_back(player);
                }
            }
        }
        if (!set.empty())
            return set[urand(0, set.size() - 1)];
        return nullptr;
    }

    template <typename... Args>
    void summon_on_player(Args... args)
    {
        if (auto player = select_summon_target())
            _summon(player, args...);
    }

    template <typename... Args>
    void _summon(Player* player, uint32 entry, Args... args)
    {
        m_creature->SummonCreature(entry, player->GetX(), player->GetY(),
            player->GetZ(), player->GetO(), TEMPSUMMON_CORPSE_TIMED_DESPAWN,
            4000);
        _summon(player, args...);
    }

    void _summon(Player*) {}

    void Notify(uint32 id, Unit* source) override
    {
        if (id == 1 && source != nullptr)
        {
            if (auto p = select_summon_target())
            {
                if (auto portal = GetClosestCreatureWithEntry(
                        source, NPC_GIANT_TENTACLE_PORTAL, 5.0f))
                    portal->NearTeleportTo(
                        p->GetX(), p->GetY(), p->GetZ(), p->GetO());
                source->NearTeleportTo(
                    p->GetX(), p->GetY(), p->GetZ(), p->GetO());
            }
        }
        else if (id == 2 && source != nullptr)
        {
            if (auto p = select_summon_target())
                source->CastSpell(p, SPELL_EYE_BEAM, false);
        }
        else if (id == 100)
        {
            phase = PHASE_TRANSITION;
            transition_timer = 4000;
            summoned_eye = false;
        }
        else if (id == 200)
        {
            if (++dead_flesh_tentacles == 2)
            {
                phase = PHASE_VULNERABLE;
                DoScriptText(EMOTE_WEAKENED, m_creature);
                m_creature->remove_auras(SPELL_CARAPACE);
                m_creature->CastSpell(m_creature, SPELL_VULNERABLE, true);
            }
        }
    }

    void UpdateAI(uint32 diff) override
    {
        if (!summoned_eye && phase == PHASE_EYE)
        {
            m_creature->SummonCreature(NPC_EYE_OF_CTHUN, m_creature->GetX(),
                m_creature->GetY(), m_creature->GetZ(), m_creature->GetO(),
                TEMPSUMMON_CORPSE_TIMED_DESPAWN, 4000);
            summoned_eye = true;
        }

        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
        {
            auto p = maps::visitors::yield_best_match<Player>{}(m_creature,
                AGGRO_DIST, [this](Player* p)
                {
                    return !p->isGameMaster() &&
                           m_creature->IsWithinLOSInMap(p);
                });
            if (p)
            {
                m_creature->SetInCombatWithZone();
                if (auto eye_of_cthun = m_creature->GetMap()->GetCreature(eye))
                {
                    eye_of_cthun->AI()->AttackStart(p);
                    eye_of_cthun->SetInCombatWithZone();
                }
            }
            return;
        }

        if (phase == PHASE_EYE)
        {
            if (tentacle <= diff)
            {
                summon_on_player(NPC_CLAW_TENTACLE, NPC_TENTACLE_PORTAL);
                tentacle = urand(5000, 10000);
            }
            else
                tentacle -= diff;
        }
        else if (phase == PHASE_CTHUN)
        {
            if (giant_claw <= diff)
            {
                summon_on_player(
                    NPC_GIANT_CLAW_TENTACLE, NPC_GIANT_TENTACLE_PORTAL);
                giant_claw = 40000;
            }
            else
                giant_claw -= diff;

            if (giant_eye <= diff)
            {
                summon_on_player(
                    NPC_GIANT_EYE_TENTACLE, NPC_GIANT_TENTACLE_PORTAL);
                giant_eye = 40000;
            }
            else
                giant_eye -= diff;

            if (mouth_tentacle <= diff)
            {
                if (auto player = select_summon_target())
                {
                    player->CastSpell(player, SPELL_MOUTH_TENTACLE, true);
                    player->queue_action(4000, [player]()
                        {
                            player->CastSpell(
                                player, SPELL_DIGESTIVE_ACID_TP, true);
                        });
                    mouth_tentacle = 10000;
                }
            }
            else
                mouth_tentacle -= diff;

            if (!summoned_flesh_tentacles)
            {
                dead_flesh_tentacles = 0;
                summoned_flesh_tentacles = true;
                m_creature->SummonCreature(NPC_FLESH_TENTACLE, -8570.8f,
                    1991.9f, -98.9f, 0.7f, TEMPSUMMON_CORPSE_TIMED_DESPAWN,
                    4000);
                m_creature->SummonCreature(NPC_FLESH_TENTACLE, -8523.6f,
                    1995.3f, -98.3f, 3.3f, TEMPSUMMON_CORPSE_TIMED_DESPAWN,
                    4000);
            }
        }

        if (phase == PHASE_EYE || phase == PHASE_CTHUN)
        {
            if (eye_tentacles <= diff)
            {
                for (int i = 0; i < 8; ++i)
                {
                    float ori = 0.785f * i;
                    auto pos = m_creature->GetPoint(ori, 30.0f);
                    m_creature->SummonCreature(NPC_EYE_TENTACLE, pos.x, pos.y,
                        pos.z, ori, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 4000);
                    m_creature->SummonCreature(NPC_TENTACLE_PORTAL, pos.x,
                        pos.y, pos.z, ori, TEMPSUMMON_CORPSE_TIMED_DESPAWN,
                        4000);
                }
                eye_tentacles = 45000;
            }
            else
                eye_tentacles -= diff;
        }

        if (phase == PHASE_TRANSITION)
        {
            if (transition_timer <= diff)
            {
                m_creature->CastSpell(m_creature, SPELL_CARAPACE, true);
                m_creature->CastSpell(m_creature, SPELL_TRANSFORM, false);
                m_creature->RemoveFlag(UNIT_FIELD_FLAGS,
                    UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE);
                phase = PHASE_CTHUN;
                transition_timer = 0;
            }
            else
                transition_timer -= diff;
        }

        if (phase == PHASE_VULNERABLE)
        {
            if (!m_creature->has_aura(SPELL_VULNERABLE))
            {
                phase = PHASE_CTHUN;
                m_creature->CastSpell(m_creature, SPELL_CARAPACE, true);
                summoned_flesh_tentacles = false;
            }
        }
    }
};

CreatureAI* GetAI_boss_cthunAI(Creature* creature)
{
    return new boss_cthunAI(creature);
}

void AddSC_boss_cthun()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_cthun";
    pNewScript->GetAI = &GetAI_boss_cthunAI;
    pNewScript->RegisterSelf();
}
