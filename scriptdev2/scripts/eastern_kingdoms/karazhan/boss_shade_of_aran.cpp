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
SDName: Boss_Shade_of_Aran
SD%Complete: 100
SDComment:
SDCategory: Karazhan
EndScriptData */

#include "GameObject.h"
#include "escort_ai.h"
#include "karazhan.h"
#include "precompiled.h"

enum
{
    SAY_AGGRO_1 = -1532073,
    SAY_AGGRO_2 = -1532074,
    SAY_AGGRO_3 = -1532075,
    SAY_FLAMEWREATH_1 = -1532076,
    SAY_FLAMEWREATH_2 = -1532077,
    SAY_BLIZZARD_1 = -1532078,
    SAY_BLIZZARD_2 = -1532079,
    SAY_EXPLOSION_1 = -1532080,
    SAY_EXPLOSION_2 = -1532081,
    SAY_DRINK = -1532082, // Low Mana / AoE Pyroblast
    SAY_ELEMENTALS = -1532083,
    SAY_KILL_1 = -1532084,
    SAY_KILL_2 = -1532085,
    SAY_TIMEOVER = -1532086,
    SAY_DEATH = -1532087,
    SAY_ATIESH = -1532088, // Atiesh is equipped by a raid member (FIXME)

    // Spells
    SPELL_FROSTBOLT = 29954,
    SPELL_FIREBALL = 29953,
    SPELL_ARCANE_MISSILES = 29955,
    SPELL_CHAINS_OF_ICE = 29991,
    SPELL_MASS_SLOW = 30035,
    SPELL_FLAME_WREATH = 29946,
    SPELL_CAST_FLAME_WREATH = 30004,
    SPELL_AOE_CS = 29961,
    SPELL_PLAYER_PULL = 32265,
    SPELL_ARCANE_EXPLOSION = 29973,
    SPELL_MASS_POLY = 29963,
    SPELL_BLINK_CENTER = 29967,
    SPELL_ELEMENTALS = 29962,
    SPELL_CONJURE = 29975,
    SPELL_DRINK = 30024,
    SPELL_POTION = 32453,
    SPELL_PRESENCE_OF_MIND = 29976,
    SPELL_AOE_PYROBLAST = 29978,
    SPELL_DRAGONS_BREATH = 29964,

    SPELL_EXPLOSION = 29949,
    SPELL_SUMMON_BLIZZARD = 29969,
    SPELL_SINGLE_SLOW = 29990,

    // Creature Spells
    SPELL_CIRCULAR_BLIZZARD_TICK = 29951,
    SPELL_CIRCULAR_BLIZZARD = 29952,
    SPELL_WATERBOLT = 31012,

    // Creatures
    NPC_WATER_ELEMENTAL = 17167,
    NPC_SHADOW_OF_ARAN = 18254,
    NPC_ARAN_BLIZZARD = 17161
};

struct SpawnPosition
{
    float X, Y, Z, R;
};

const SpawnPosition BlizzardPositions[] = {
    {-11157.84f, -1906.15f, 232.0f, 2.33f},
    {-11154.68f, -1912.63f, 232.0f, 1.50f},
    {-11158.54f, -1919.90f, 232.0f, 0.69f},
    {-11166.20f, -1922.48f, 232.0f, 6.21f},
    {-11173.10f, -1918.67f, 232.0f, 5.45f},
    {-11175.56f, -1911.00f, 232.0f, 4.63f},
    {-11172.91f, -1902.10f, 232.0f, 3.80f},
    {-11163.73f, -1901.53f, 232.0f, 3.11f}};
uint32 BlizzardPositions_Size = 8;

const SpawnPosition WaterElementalPositions[] = {
    {-11168.08f, -1937.48f, 232.0f, 1.43f},
    {-11140.18f, -1914.68f, 232.0f, 3.00f},
    {-11162.35f, -1886.80f, 232.0f, 4.63f},
    {-11190.00f, -1909.29f, 232.0f, 6.17f}};

const SpawnPosition ShadowOfAranPositions[] = {
    {-11182.40f, -1889.90f, 232.0f, 5.40f},
    {-11147.60f, -1933.80f, 232.0f, 2.20f},
    {-11185.00f, -1927.80f, 232.0f, 0.60f},
    {-11144.99f, -1895.94f, 232.0f, 3.82f},
    {-11168.08f, -1937.48f, 232.0f, 1.43f},
    {-11140.18f, -1914.68f, 232.0f, 3.00f},
    {-11162.35f, -1886.80f, 232.0f, 4.63f},
    {-11190.00f, -1909.29f, 232.0f, 6.17f}};

enum DrinkStage
{
    DS_NOT_INITIATED = 0,
    DS_POLYMORPH,
    DS_CONJURE,
    DS_DRINK,
    DS_POM,
    DS_PYRO,
    DS_FINISHED
};

enum SuperCasts
{
    SUPER_NONE,
    SUPER_FIRE,
    SUPER_FROST,
    SUPER_ARCANE
};

struct MANGOS_DLL_DECL mob_aran_blizzardAI : public ScriptedAI
{
    mob_aran_blizzardAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        Reset();
    }

    uint32 m_castTimer;
    uint32 m_despawnTimer;
    ObjectGuid m_aranGuid;

    // Disable all combat logic:
    void MoveInLineOfSight(Unit* /*pWho*/) override {}
    void AttackStart(Unit* /*pWho*/) override {}
    void AttackedBy(Unit* /*pWho*/) override {}
    void Aggro(Unit* /*pWho*/) override {}

    void Reset() override { m_despawnTimer = 41000; }

    void StartBlizzardIn(uint32 castTime, ObjectGuid guid)
    {
        m_despawnTimer = castTime + 10000;
        m_castTimer = castTime;
        m_aranGuid = guid;
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (m_despawnTimer <= uiDiff)
            m_creature->ForcedDespawn();
        else
            m_despawnTimer -= uiDiff;

        if (m_castTimer <= uiDiff)
        {
            DoCastSpellIfCan(m_creature, SPELL_CIRCULAR_BLIZZARD_TICK,
                CAST_TRIGGERED, m_aranGuid);
            m_castTimer = 50000;
        }
        else
            m_castTimer -= uiDiff;
    }
};

struct MANGOS_DLL_DECL boss_aranAI : public ScriptedAI
{
    boss_aranAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_instance = pCreature->GetMapId() == KARAZHAN_MAP_ID ?
                         (instance_karazhan*)pCreature->GetInstanceData() :
                         NULL;
        Reset();
    }

    instance_karazhan* m_instance;
    uint32 m_silenceTimer;
    uint32 m_normalCastTimer;
    uint32 m_superCastTimer;
    uint32 m_specialCastTimer; // Dragon's breath, slow and chains of ice
    uint32 m_berserkTimer;
    uint32 m_flameWreathTimer;
    ObjectGuid m_flameWreathTargetGuid[3];
    float m_fwTarPos[3][2];
    bool m_elementalsSpawned;
    DrinkStage m_drinkStage;
    uint32 m_drinkTimer;
    uint32 m_startFlameWreathTimer;
    SuperCasts m_lastSuper;
    ObjectGuid m_lastTarget;

    void Reset() override
    {
        m_creature->SetStandState(
            UNIT_STAND_STATE_STAND); // Make sure we're standing

        m_silenceTimer = urand(5000, 10000);
        m_normalCastTimer = 0;
        m_superCastTimer = urand(5000, 10000);
        m_specialCastTimer = 0;
        m_berserkTimer = 12 * 60 * 1000;
        m_flameWreathTimer = 0;
        m_startFlameWreathTimer = 0;
        m_drinkTimer = 0;
        m_elementalsSpawned = false;
        m_drinkStage = DS_NOT_INITIATED;
        m_lastSuper = SUPER_NONE;
    }

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_KILL_1, SAY_KILL_2);
    }

    void JustDied(Unit* /*pVictim*/) override
    {
        DoScriptText(SAY_DEATH, m_creature);

        if (m_instance)
            m_instance->SetData(TYPE_ARAN, DONE);
    }

    void JustReachedHome() override
    {
        if (m_instance)
            m_instance->SetData(TYPE_ARAN, FAIL);
    }

    void Aggro(Unit* /*pWho*/) override
    {
        switch (urand(0, 2))
        {
        case 0:
            DoScriptText(SAY_AGGRO_1, m_creature);
            break;
        case 1:
            DoScriptText(SAY_AGGRO_2, m_creature);
            break;
        case 2:
            DoScriptText(SAY_AGGRO_3, m_creature);
            break;
        }

        if (m_instance)
            m_instance->SetData(TYPE_ARAN, IN_PROGRESS);
    }

    Unit* SelectCastTarget()
    {
        std::vector<Unit*> targets;
        Unit* target_out = nullptr;
        Unit* last_guy = nullptr;

        const ThreatList& list = m_creature->getThreatManager().getThreatList();
        for (const auto& elem : list)
        {
            auto unit = elem->getTarget();
            if (unit->GetTypeId() == TYPEID_PLAYER &&
                unit->isTargetableForAttack() &&
                !unit->IsImmunedToDamage(SPELL_SCHOOL_MASK_ALL) &&
                m_creature->IsWithinLOSInMap(unit))
            {
                targets.push_back(unit);
                if (unit->GetObjectGuid() == m_lastTarget)
                    last_guy = unit;
            }
        }

        // Prefer any target with low health
        for (auto& target : targets)
            if (target->GetHealthPercent() < 41.0f)
            {
                target_out = target;
                goto found_spell_target;
            }

        // 40% chance to shoot at the same target as last time
        if (last_guy && rand_norm_f() < 0.4f)
        {
            target_out = last_guy;
            goto found_spell_target;
        }

        // Pick random target
        if (!targets.empty())
            target_out = targets[urand(0, targets.size() - 1)];

    found_spell_target:
        if (target_out)
            m_lastTarget = target_out->GetObjectGuid();
        return target_out;
    }

    void FlameWreathEffect()
    {
        std::vector<Unit*> targets;

        ThreatList const& tList =
            m_creature->getThreatManager().getThreatList();
        if (tList.empty())
            return;

        // Store the threat list in a different container
        for (const auto& elem : tList)
        {
            Unit* target = m_creature->GetMap()->GetUnit((elem)->getUnitGuid());

            // Only on alive players
            if (target && target->isAlive() &&
                target->GetTypeId() == TYPEID_PLAYER)
                targets.push_back(target);
        }

        // Cut down to size if we have more than 3 targets
        while (targets.size() > 3)
            targets.erase(targets.begin() + urand(0, targets.size() - 1));

        uint32 i = 0;
        for (std::vector<Unit*>::iterator itr = targets.begin();
             itr != targets.end() && i < 3; ++itr, ++i)
        {
            m_flameWreathTargetGuid[i] = (*itr)->GetObjectGuid();
            m_fwTarPos[i][0] = (*itr)->GetX();
            m_fwTarPos[i][1] = (*itr)->GetY();
            m_creature->CastSpell((*itr), SPELL_FLAME_WREATH, true);
        }
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (m_startFlameWreathTimer)
        {
            if (m_startFlameWreathTimer <= uiDiff)
            {
                m_flameWreathTimer = 20000;
                m_startFlameWreathTimer = 0;

                FlameWreathEffect();
            }
            else
                m_startFlameWreathTimer -= uiDiff;
        }

        // Are we doing drink stuff?
        if (m_drinkStage != DS_NOT_INITIATED && m_drinkStage != DS_FINISHED)
        {
            switch (m_drinkStage)
            {
            case DS_POLYMORPH:
                if (DoCastSpellIfCan(m_creature, SPELL_MASS_POLY) == CAST_OK)
                {
                    m_drinkStage = DS_CONJURE;
                    DoScriptText(SAY_DRINK, m_creature);

                    Pacify(true);
                    m_creature->movement_gens.push(
                        new movement::StoppedMovementGenerator(),
                        movement::EVENT_LEAVE_COMBAT);
                }
                break;
            case DS_CONJURE:
                if (DoCastSpellIfCan(m_creature, SPELL_CONJURE) == CAST_OK)
                    m_drinkStage = DS_DRINK;
                break;
            case DS_DRINK:
                if (!m_drinkTimer)
                {
                    if (DoCastSpellIfCan(m_creature, SPELL_DRINK) == CAST_OK)
                    {
                        m_creature->SetStandState(UNIT_STAND_STATE_SIT);
                        m_drinkTimer = 5000;
                    }
                }
                else
                {
                    if (m_drinkTimer <= uiDiff)
                    {
                        m_drinkTimer = 0;
                        m_drinkStage = DS_POM;
                    }
                    else
                        m_drinkTimer -= uiDiff;
                }
                break;
            case DS_POM:
                m_creature->SetStandState(UNIT_STAND_STATE_STAND);
                if (DoCastSpellIfCan(m_creature, SPELL_PRESENCE_OF_MIND) ==
                    CAST_OK)
                    m_drinkStage = DS_PYRO;
                break;
            case DS_PYRO:
                if (DoCastSpellIfCan(m_creature->getVictim(),
                        SPELL_AOE_PYROBLAST, CAST_TRIGGERED) == CAST_OK)
                {
                    m_creature->movement_gens.remove_all(
                        movement::gen::stopped);
                    Pacify(false);

                    m_creature->remove_auras(SPELL_PRESENCE_OF_MIND);
                    m_drinkStage = DS_FINISHED;
                }
                break;
            default:
                break;
            }

            // Keep updating flame wreath timer in drinking phase, but do not
            // trigger the effect:
            if (m_flameWreathTimer > uiDiff)
                m_flameWreathTimer -= uiDiff;
            return; // We're in drinking phase
        }

        // Are we OOM?
        if (((float)m_creature->GetPower(POWER_MANA) /
                (float)m_creature->GetMaxPower(POWER_MANA)) < 0.2f)
        {
            // We haven't drinken yet
            if (m_drinkStage == DS_NOT_INITIATED &&
                m_creature->GetPower(POWER_MANA) >
                    20000) // Polymorph is really expensive, if we do not have
                           // enough mana for it, pop a potion
            {
                m_drinkStage = DS_POLYMORPH;
                return;
            }
            else
            {
                // Okay, let's pop a potion
                if (DoCastSpellIfCan(m_creature, SPELL_POTION) == CAST_OK)
                    DoScriptText(SAY_DRINK, m_creature);
            }
        }

        if (m_superCastTimer <= uiDiff)
        {
            // Wait until GCD is over
            if (CanCastSpell(m_creature, SPELL_CAST_FLAME_WREATH, false) !=
                CAST_OK)
                return;

            // Teleport to middle
            m_creature->CastSpell(m_creature, SPELL_BLINK_CENTER, true);

            std::vector<SuperCasts> possible;
            possible.reserve(3);
            if (m_lastSuper != SUPER_FIRE)
                possible.push_back(SUPER_FIRE);
            if (m_lastSuper != SUPER_FROST)
                possible.push_back(SUPER_FROST);
            if (m_lastSuper != SUPER_ARCANE)
                possible.push_back(SUPER_ARCANE);
            if (possible.empty())
                return;
            // Do super cast logic
            switch (m_lastSuper = possible[urand(0, possible.size() - 1)])
            {
            case SUPER_FIRE:
                DoScriptText(
                    urand(0, 1) ? SAY_FLAMEWREATH_1 : SAY_FLAMEWREATH_2,
                    m_creature);
                DoCastSpellIfCan(m_creature, SPELL_CAST_FLAME_WREATH);
                m_startFlameWreathTimer = 5000;
                m_specialCastTimer = urand(20000, 30000);
                break;
            case SUPER_FROST:
            {
                DoScriptText(
                    urand(0, 1) ? SAY_BLIZZARD_1 : SAY_BLIZZARD_2, m_creature);
                DoCastSpellIfCan(m_creature, SPELL_SUMMON_BLIZZARD);

                uint32 startBlizzardIn = 3700;
                // Insert all from randomPos to end
                uint32 startPos = urand(0, BlizzardPositions_Size - 1);
                std::vector<SpawnPosition> blizzardPos(
                    BlizzardPositions + startPos,
                    BlizzardPositions + BlizzardPositions_Size);
                // Insert all from start to randomPos
                for (uint8 i = 0; i < startPos; ++i)
                    blizzardPos.push_back(BlizzardPositions[i]);

                // Spawn all blizzard positions and offset their cast time
                for (auto pos : blizzardPos)
                {
                    if (Creature* spawn = m_creature->SummonCreature(
                            NPC_ARAN_BLIZZARD, pos.X, pos.Y, pos.Z, pos.R,
                            TEMPSUMMON_TIMED_DESPAWN, 50000))
                    {
                        if (mob_aran_blizzardAI* blizzAI =
                                dynamic_cast<mob_aran_blizzardAI*>(spawn->AI()))
                        {
                            blizzAI->StartBlizzardIn(
                                startBlizzardIn, m_creature->GetObjectGuid());
                            startBlizzardIn += 3700;
                        }
                    }
                }
                m_specialCastTimer = urand(5000, 15000);
                break;
            }
            case SUPER_ARCANE:
            {
                DoScriptText(urand(0, 1) ? SAY_EXPLOSION_1 : SAY_EXPLOSION_2,
                    m_creature);

                m_creature->CastSpell(m_creature, SPELL_PLAYER_PULL, true);
                DoCastSpellIfCan(m_creature, SPELL_ARCANE_EXPLOSION);

                // Delay slow until playerpull spell has chance to take effect
                auto creature = m_creature;
                m_creature->queue_action(800, [creature]()
                    {
                        if (creature->isAlive())
                            creature->CastSpell(
                                creature, SPELL_MASS_SLOW, true);
                    });
                m_specialCastTimer = urand(10000, 20000);
                break;
            }
            default:
                break;
            }

            m_superCastTimer = 30000;
        }
        else
            m_superCastTimer -= uiDiff;

        if (m_silenceTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_AOE_CS) == CAST_OK)
                m_silenceTimer = urand(5000, 10000);
        }
        else
            m_silenceTimer -= uiDiff;

        if (m_specialCastTimer)
        {
            if (m_specialCastTimer <= uiDiff)
            {
                switch (m_lastSuper)
                {
                case SUPER_FIRE:
                    if (Unit* target = m_creature->SelectAttackingTarget(
                            ATTACKING_TARGET_RANDOM, 0, SPELL_DRAGONS_BREATH))
                        if (DoCastSpellIfCan(target, SPELL_DRAGONS_BREATH) !=
                            CAST_OK)
                            return;
                    break;
                case SUPER_FROST:
                    if (Unit* target = m_creature->SelectAttackingTarget(
                            ATTACKING_TARGET_RANDOM, 0, SPELL_CHAINS_OF_ICE))
                        if (DoCastSpellIfCan(target, SPELL_CHAINS_OF_ICE) !=
                            CAST_OK)
                            return;
                    break;
                case SUPER_ARCANE:
                    if (Unit* target = m_creature->SelectAttackingTarget(
                            ATTACKING_TARGET_RANDOM, 0, SPELL_SINGLE_SLOW))
                        if (DoCastSpellIfCan(target, SPELL_SINGLE_SLOW) !=
                            CAST_OK)
                            return;
                    break;
                default:
                    break;
                }
                m_specialCastTimer = 0;
            }
            else
                m_specialCastTimer -= uiDiff;
        }

        if (!m_specialCastTimer || m_specialCastTimer > 3000)
        {
            // Take a random spell of our 3 normal ones, assuming the school
            // isn't locked
            std::vector<uint32> availableSpells;
            availableSpells.reserve(3);
            if (!m_creature->IsSpellSchoolLocked(SPELL_SCHOOL_MASK_FIRE))
                availableSpells.push_back(SPELL_FIREBALL);
            if (!m_creature->IsSpellSchoolLocked(SPELL_SCHOOL_MASK_FIRE))
                availableSpells.push_back(SPELL_FROSTBOLT);
            if (!m_creature->IsSpellSchoolLocked(SPELL_SCHOOL_MASK_ARCANE))
                availableSpells.push_back(SPELL_ARCANE_MISSILES);

            if (!availableSpells.empty())
                if (Unit* tar = SelectCastTarget())
                    DoCastSpellIfCan(tar,
                        availableSpells[urand(0, availableSpells.size() - 1)]);
        }

        if (!m_elementalsSpawned && m_creature->GetHealthPercent() < 41.0f)
        {
            for (auto& WaterElementalPosition : WaterElementalPositions)
            {
                m_creature->SummonCreature(NPC_WATER_ELEMENTAL,
                    WaterElementalPosition.X, WaterElementalPosition.Y,
                    WaterElementalPosition.Z, WaterElementalPosition.R,
                    TEMPSUMMON_TIMED_DEATH, 90000,
                    SUMMON_OPT_DESPAWN_ON_SUMMONER_DEATH);
            }
            m_elementalsSpawned = true;
            DoScriptText(SAY_ELEMENTALS, m_creature);
        }

        if (m_berserkTimer)
        {
            if (m_berserkTimer <= uiDiff)
            {
                for (auto& ShadowOfAranPosition : ShadowOfAranPositions)
                {
                    m_creature->SummonCreature(NPC_SHADOW_OF_ARAN,
                        ShadowOfAranPosition.X, ShadowOfAranPosition.Y,
                        ShadowOfAranPosition.Z, ShadowOfAranPosition.R,
                        TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 5000,
                        SUMMON_OPT_DESPAWN_ON_SUMMONER_DEATH);
                }

                DoScriptText(SAY_TIMEOVER, m_creature);
                m_berserkTimer = 0;
            }
            else
                m_berserkTimer -= uiDiff;
        }

        // Flame Wreath check
        if (m_flameWreathTimer)
        {
            if (m_flameWreathTimer >= uiDiff)
                m_flameWreathTimer -= uiDiff;
            else
                m_flameWreathTimer = 0;

            for (uint32 i = 0; i < 3; ++i)
            {
                if (!m_flameWreathTargetGuid[i])
                    continue;

                Player* pPlayer =
                    m_creature->GetMap()->GetPlayer(m_flameWreathTargetGuid[i]);
                if (!pPlayer || !pPlayer->isAlive())
                {
                    m_flameWreathTargetGuid[i].Clear();
                    continue;
                }

                if (!pPlayer->IsWithinDist2d(
                        m_fwTarPos[i][0], m_fwTarPos[i][1], 3.0f))
                {
                    pPlayer->CastSpell(pPlayer, SPELL_EXPLOSION, true, 0, 0,
                        m_creature->GetObjectGuid());
                    m_flameWreathTargetGuid[i].Clear();
                }
            }
        }

        DoMeleeAttackIfReady();
    }

    void DamageTaken(Unit* /*pAttacker*/, uint32& damage) override
    {
        // We are not to be damaged while drinking
        if (m_drinkTimer != 0)
            damage = 0;
    }
};

CreatureAI* GetAI_boss_aran(Creature* pCreature)
{
    return new boss_aranAI(pCreature);
}

CreatureAI* GetAI_aran_blizzard(Creature* pCreature)
{
    return new mob_aran_blizzardAI(pCreature);
}

void AddSC_boss_shade_of_aran()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_shade_of_aran";
    pNewScript->GetAI = &GetAI_boss_aran;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "mob_aran_blizzard";
    pNewScript->GetAI = &GetAI_aran_blizzard;
    pNewScript->RegisterSelf();
}
