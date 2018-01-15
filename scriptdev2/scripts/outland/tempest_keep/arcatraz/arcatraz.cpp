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
SDName: Arcatraz
SD%Complete: 100
SDComment:
SDCategory: Tempest Keep, The Arcatraz
EndScriptData */

/* ContentData
npc_millhouse_manastorm
npc_warden_mellichar
mob_zerekethvoidzone
EndContentData */

#include "arcatraz.h"

/*#####
# npc_millhouse_manastorm
#####*/

enum
{
    SAY_INTRO_1 = -1552010,
    SAY_INTRO_2 = -1552011,
    SAY_INTRO_3 = -1552031, // Blizzard put the sound of this one in the same as
                            // the one above, but they made seperate yells.
    SAY_INTRO_4 = -1552033, // Same with this one
    SAY_WATER = -1552012,
    SAY_BUFFS = -1552013,
    SAY_DRINK = -1552014,
    SAY_READY = -1552015,
    SAY_KILL_1 = -1552016,
    SAY_KILL_2 = -1552017,
    SAY_PYRO = -1552018,
    SAY_ICEBLOCK = -1552019,
    SAY_LOWHP = -1552020,
    SAY_DEATH = -1552021,
    SAY_COMPLETE = -1552022,

    SPELL_SIMPLE_TELEPORT = 12980,
    SPELL_DRINK = 30024,
    SPELL_CONJURE_WATER = 36879,
    SPELL_ARCANE_INTELLECT = 36880,
    SPELL_ICE_ARMOR = 36881,

    // All Millhouse spells but pyroblast are handled by BehavioralAI
    SPELL_ARCANE_MISSILES = 33833,
    SPELL_CONE_OF_COLD = 12611,
    SPELL_FIRE_BLAST = 13341,
    SPELL_FIREBALL = 14034,
    SPELL_FROSTBOLT = 15497,
    SPELL_PYROBLAST = 33975,
};

struct MANGOS_DLL_DECL npc_millhouse_manastormAI : public Scripted_BehavioralAI
{
    npc_millhouse_manastormAI(Creature* pCreature)
      : Scripted_BehavioralAI(pCreature)
    {
        m_pInstance = (ScriptedInstance*)pCreature->GetInstanceData();
        m_creature->SetFlag(
            UNIT_FIELD_FLAGS, UNIT_FLAG_PASSIVE | UNIT_FLAG_NON_ATTACKABLE);
        Reset();
    }

    ScriptedInstance* m_pInstance;

    uint32 m_uiEventProgressTimer;
    uint32 m_uiPhase;
    bool m_bInitFinished;
    bool m_bHasLowHp;

    uint32 m_uiPyroblastTimer;
    uint32 m_uiFireballTimer;

    void Reset() override
    {
        m_uiEventProgressTimer = 200;
        m_bHasLowHp = false;
        m_bInitFinished = false;
        m_uiPhase = 1;

        m_uiPyroblastTimer = 1000;
        m_uiFireballTimer = 2500;

        if (m_pInstance)
        {
            if (m_pInstance->GetData(TYPE_WARDEN_2) == DONE)
                m_bInitFinished = true;

            if (m_pInstance->GetData(TYPE_HARBINGERSKYRISS) == DONE)
            {
                DoScriptText(SAY_COMPLETE, m_creature);
                m_creature->SetFlag(UNIT_NPC_FLAGS,
                    UNIT_NPC_FLAG_GOSSIP | UNIT_NPC_FLAG_QUESTGIVER);
            }
        }

        Scripted_BehavioralAI::Reset();
    }

    void KilledUnit(Unit* /*pVictim*/) override
    {
        DoScriptText(urand(0, 1) ? SAY_KILL_1 : SAY_KILL_2, m_creature);
    }

    void JustDied(Unit* /*pVictim*/) override
    {
        DoScriptText(SAY_DEATH, m_creature);
    }

    void MovementInform(movement::gen uiMotionType, uint32 uiPointId) override
    {
        if (uiMotionType == movement::gen::point && uiPointId == 100)
        {
            m_creature->RemoveFlag(
                UNIT_FIELD_FLAGS, UNIT_FLAG_PASSIVE | UNIT_FLAG_NON_ATTACKABLE);
            m_bInitFinished = true;
            m_creature->SetFacingTo(4.6f);
            m_creature->SetOrientation(4.6f);
            m_creature->movement_gens.remove_all(movement::gen::idle);
            m_creature->movement_gens.push(
                new movement::IdleMovementGenerator());
        }
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_bInitFinished && m_uiEventProgressTimer)
        {
            if (m_uiEventProgressTimer <= uiDiff)
            {
                if (m_uiPhase < 13)
                {
                    switch (m_uiPhase)
                    {
                    case 1:
                        DoCast(m_creature, SPELL_SIMPLE_TELEPORT, true);
                        m_uiEventProgressTimer = 1800;
                        break;
                    case 2:
                        DoScriptText(SAY_INTRO_1, m_creature);
                        m_uiEventProgressTimer = 9000;
                        break;
                    case 3:
                        if (m_pInstance)
                            m_pInstance->SetData(TYPE_WARDEN_2, DONE);
                        m_uiEventProgressTimer = 9000;
                        break;
                    case 4:
                        DoScriptText(SAY_INTRO_2, m_creature);
                        m_uiEventProgressTimer = 8500;
                        break;
                    case 5:
                        DoScriptText(SAY_INTRO_3, m_creature);
                        m_uiEventProgressTimer = 4500;
                        break;
                    case 6:
                        DoScriptText(SAY_INTRO_4, m_creature);
                        m_uiEventProgressTimer = 3000;
                        break;
                    case 7:
                        m_uiEventProgressTimer = 4000;
                        break;
                    case 8:
                        DoScriptText(SAY_WATER, m_creature);
                        DoCastSpellIfCan(m_creature, SPELL_CONJURE_WATER);
                        m_uiEventProgressTimer = 7000;
                        break;
                    case 9:
                        DoScriptText(SAY_BUFFS, m_creature);
                        DoCastSpellIfCan(m_creature, SPELL_ICE_ARMOR);
                        m_uiEventProgressTimer = 2500;
                        break;
                    case 10:
                        DoCastSpellIfCan(m_creature, SPELL_ARCANE_INTELLECT);
                        m_uiEventProgressTimer = 4500;
                        break;
                    case 11:
                        DoScriptText(SAY_DRINK, m_creature);
                        DoCastSpellIfCan(m_creature, SPELL_DRINK);
                        m_uiEventProgressTimer = 9000;
                        break;
                    case 12:
                        DoScriptText(SAY_READY, m_creature);
                        m_creature->remove_auras(SPELL_DRINK);
                        m_creature->SetStandState(UNIT_STAND_STATE_STAND);
                        m_creature->movement_gens.push(
                            new movement::PointMovementGenerator(
                                100, 445.9f, -160.0f, 43.1f, false, true));
                        m_uiEventProgressTimer = 0;
                        break;
                    }
                    ++m_uiPhase;
                }
            }
            else
                m_uiEventProgressTimer -= uiDiff;
        }

        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        Scripted_BehavioralAI::UpdateInCombatAI(uiDiff);

        if (!m_bHasLowHp && m_creature->GetHealthPercent() <= 20.0f)
        {
            DoScriptText(SAY_LOWHP, m_creature);
            m_bHasLowHp = true;
        }

        if (m_uiPyroblastTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_PYROBLAST) ==
                CAST_OK)
            {
                m_uiPyroblastTimer = 40000;
                DoScriptText(SAY_PYRO, m_creature);
            }
        }
        else
            m_uiPyroblastTimer -= uiDiff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_npc_millhouse_manastorm(Creature* pCreature)
{
    return new npc_millhouse_manastormAI(pCreature);
}

/*#####
# npc_warden_mellichar
#####*/

enum
{
    YELL_INTRO = -1552023,
    YELL_RELEASE_1 = -1552024,
    YELL_RELEASE_2 = -1552025,
    YELL_RELEASE_2B = -1552026,
    YELL_RELEASE_3 = -1552027,
    YELL_RELEASE_4 = -1552028,
    YELL_RELEASE_5 = -1552029,
    YELL_WELCOME = -1552030,

    // phase 2
    ENTRY_TRICKSTER = 20905,
    ENTRY_PH_HUNTER = 20906,
    // phase 3
    // ENTRY_MILLHOUSE         = 20977,
    // phase 4
    ENTRY_AKKIRIS = 20908,
    ENTRY_SULFURON = 20909,
    // phase 5
    ENTRY_TW_DRAK = 20910,
    ENTRY_BL_DRAK = 20911,
    // phase 6
    ENTRY_SKYRISS = 20912,

    SPELL_TARGET_ALPHA = 36856,
    SPELL_TARGET_BETA = 36854,
    SPELL_TARGET_DELTA = 36857,
    SPELL_TARGET_GAMMA = 36858,
    SPELL_TARGET_OMEGA = 36852,
    SPELL_BUBBLE_VISUAL = 36849,

    POD_OPEN_TIME = 9000,
    WAVE1_TIMEOUT = 50 * 1000,
    WAVE2_TIMEOUT = 60 * 1000,
    WAVE3_TIMEOUT = 90 * 1000,
};

static const float aSummonPosition[5][4] = {
    {476.0f, -149.1f, 42.6f, 3.1f}, // Trickster or Phase Hunter
    {417.3f, -149.3f, 42.6f, 6.3f}, // Millhouse
    {420.6f, -174.3f, 42.6f, 0.3f}, // Akkiris or Sulfuron
    {471.5f, -174.1f, 42.6f, 2.8f}, // Twilight or Blackwing Drakonaar
    {445.8f, -191.6f, 44.6f, 1.6f}  // Skyriss
};

struct MANGOS_DLL_DECL npc_warden_mellicharAI : public ScriptedAI
{
    npc_warden_mellicharAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_pInstance = (instance_arcatraz*)pCreature->GetInstanceData();
        Reset();
    }

    instance_arcatraz* m_pInstance;

    bool m_bIsEventRunning;

    uint32 m_uiEventProgressTimer;
    uint32 m_uiPhase;
    uint32 m_uiTimeout;
    std::vector<ObjectGuid> m_spawnedTargets;

    void Reset() override
    {
        m_bIsEventRunning = false;

        m_uiEventProgressTimer = 2000;
        m_uiPhase = 1;
        m_uiTimeout = 0;

        if (m_pInstance)
            m_pInstance->SetData(TYPE_HARBINGERSKYRISS, NOT_STARTED);

        for (auto& elem : m_spawnedTargets)
            if (Creature* summon = m_creature->GetMap()->GetCreature(elem))
                summon->ForcedDespawn();
        m_spawnedTargets.clear();

        if (GameObject* pSphere =
                m_pInstance->GetSingleGameObjectFromStorage(GO_SEAL_SPHERE))
            pSphere->SetGoState(GO_STATE_ACTIVE);

        m_creature->InterruptNonMeleeSpells(false);
        m_creature->remove_auras(SPELL_AURA_DUMMY);

        m_creature->SetFacingTo(4.668f);
    }

    void AttackStart(Unit* /*pWho*/) override {}

    void JustSummoned(Creature* pCreature) override
    {
        m_spawnedTargets.push_back(pCreature->GetObjectGuid());
    }

    void MoveInLineOfSight(Unit* pWho) override
    {
        if (m_bIsEventRunning)
            return;

        if (!(pWho->GetTypeId() == TYPEID_PLAYER &&
                pWho->IsHostileTo(m_creature) &&
                pWho->IsWithinDistInMap(m_creature, 10.0f)))
            return;

        Aggro(pWho);
    }

    void Aggro(Unit* /*pWho*/) override
    {
        if (m_pInstance)
        {
            m_creature->movement_gens.remove_all(movement::gen::idle);

            DoScriptText(YELL_INTRO, m_creature);
            DoCastSpellIfCan(m_creature, SPELL_BUBBLE_VISUAL);
            m_creature->SetFacingTo(1.6f);

            m_pInstance->SetData(TYPE_HARBINGERSKYRISS, IN_PROGRESS);
            m_bIsEventRunning = true;
        }
    }

    void SpawnWave(uint32 wave);

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_bIsEventRunning || !m_pInstance)
            return;

        if (m_pInstance->GetData(TYPE_HARBINGERSKYRISS) == FAIL)
        {
            Reset();
            return;
        }

        if (!m_uiEventProgressTimer)
            return;

        if (m_uiTimeout)
        {
            if (m_uiTimeout <= uiDiff)
            {
                if (m_pInstance->GetData(TYPE_WARDEN_1) != DONE)
                    m_pInstance->SetData(TYPE_WARDEN_1, DONE);
                else if (m_pInstance->GetData(TYPE_WARDEN_2) != DONE)
                    m_pInstance->SetData(TYPE_WARDEN_2, DONE);
                else if (m_pInstance->GetData(TYPE_WARDEN_3) != DONE)
                    m_pInstance->SetData(TYPE_WARDEN_3, DONE);
                else if (m_pInstance->GetData(TYPE_WARDEN_4) != DONE)
                    m_pInstance->SetData(TYPE_WARDEN_4, DONE);
                m_uiTimeout = 0;
            }
            else
                m_uiTimeout -= uiDiff;
        }

        if (m_uiEventProgressTimer <= uiDiff)
        {
            switch (m_uiPhase)
            {
            case 1: // Bubble
                if (GameObject* pSphere =
                        m_pInstance->GetSingleGameObjectFromStorage(
                            GO_SEAL_SPHERE))
                    pSphere->SetGoState(GO_STATE_READY);
                m_uiEventProgressTimer = 20000;
                break;
            //
            // FIRST WAVE
            //
            case 2: // The naru kept some...
                DoScriptText(YELL_RELEASE_1, m_creature);
                m_uiEventProgressTimer = 7000;
                break;
            case 3:
                m_creature->SetFacingTo(0.5f);
                m_uiEventProgressTimer = 1000;
                break;
            case 4:
                DoCastSpellIfCan(m_creature, SPELL_TARGET_ALPHA);
                m_pInstance->SetData(
                    TYPE_WARDEN_1, IN_PROGRESS); // Opens the pod
                m_uiEventProgressTimer = POD_OPEN_TIME;
                break;
            case 5:
                SpawnWave(1);
                m_creature->SetFacingTo(4.6f);
                m_uiEventProgressTimer = 1000;
                break;
            case 6:
                DoCastSpellIfCan(m_creature, SPELL_TARGET_OMEGA);
                m_uiTimeout = WAVE1_TIMEOUT;
                m_uiEventProgressTimer = 1000;
                break;
            case 7:
                m_uiEventProgressTimer = 1000;
                if (m_pInstance->GetData(TYPE_WARDEN_1) != DONE)
                    return;
                m_uiTimeout = 0;
                m_creature->InterruptNonMeleeSpells(false);
                m_creature->remove_auras(SPELL_AURA_DUMMY);
                break;
            //
            // SECOND WAVE
            //
            case 8: // Yes, yes... another! Your will is mine!
                m_creature->SetFacingTo(1.6f);
                DoScriptText(YELL_RELEASE_2, m_creature);
                m_uiEventProgressTimer = 5000;
                break;
            case 9:
                m_creature->SetFacingTo(2.6f);
                m_uiEventProgressTimer = 1000;
                break;
            case 10: // Millhouse pod opening
                DoCastSpellIfCan(m_creature, SPELL_TARGET_BETA);
                m_pInstance->SetData(
                    TYPE_WARDEN_2, IN_PROGRESS); // Opens the pod
                m_uiEventProgressTimer = 2000;
                break;
            case 11:
                m_creature->SetFacingTo(4.6f);
                m_uiEventProgressTimer = 1000;
                break;
            case 12: // Behold, yet another terrifying...
                DoScriptText(YELL_RELEASE_2B, m_creature);
                DoCastSpellIfCan(m_creature, SPELL_TARGET_OMEGA);
                m_uiEventProgressTimer = POD_OPEN_TIME - 2000;
                break;
            case 13: // Spawn Millhouse
                SpawnWave(2);
                m_uiEventProgressTimer = 1000;
                break;
            case 14:
                m_uiEventProgressTimer = 1000;
                if (m_pInstance->GetData(TYPE_WARDEN_2) != DONE)
                    return;
                m_uiTimeout = 0;
                m_creature->InterruptNonMeleeSpells(false);
                m_creature->remove_auras(SPELL_AURA_DUMMY);
                break;
            //
            // THIRD WAVE
            //
            case 15: // What is this? A lowly gnome? ...
                DoScriptText(YELL_RELEASE_3, m_creature);
                m_creature->SetFacingTo(2.6f);
                m_uiEventProgressTimer = 4000;
                break;
            case 16:
                m_pInstance->SetData(
                    TYPE_WARDEN_3, IN_PROGRESS); // Opens the pod
                m_creature->SetFacingTo(3.4f);
                m_uiEventProgressTimer = 1000;
                break;
            case 17:
                DoCastSpellIfCan(m_creature, SPELL_TARGET_DELTA);
                m_uiEventProgressTimer = POD_OPEN_TIME;
                break;
            case 18:
                SpawnWave(3);
                m_creature->SetFacingTo(4.6f);
                m_uiEventProgressTimer = 1000;
                break;
            case 19:
                DoCastSpellIfCan(m_creature, SPELL_TARGET_OMEGA);
                m_uiTimeout = WAVE2_TIMEOUT;
                m_uiEventProgressTimer = 1000;
                break;
            case 20:
                m_uiEventProgressTimer = 1000;
                if (m_pInstance->GetData(TYPE_WARDEN_3) != DONE)
                    return;
                m_uiTimeout = 0;
                m_creature->InterruptNonMeleeSpells(false);
                m_creature->remove_auras(SPELL_AURA_DUMMY);
                break;
            //
            // FOURTH WAVE
            //
            case 21: // Anarchy! Bedlam! ...
                DoScriptText(YELL_RELEASE_4, m_creature);
                m_creature->SetFacingTo(6.2f);
                m_uiEventProgressTimer = 1000;
                break;
            case 22:
                m_pInstance->SetData(
                    TYPE_WARDEN_4, IN_PROGRESS); // Opens the pod
                DoCastSpellIfCan(m_creature, SPELL_TARGET_GAMMA);
                m_uiEventProgressTimer = POD_OPEN_TIME;
                break;
            case 23:
                SpawnWave(4);
                m_creature->SetFacingTo(4.6f);
                m_uiEventProgressTimer = 1000;
                break;
            case 24:
                DoCastSpellIfCan(m_creature, SPELL_TARGET_OMEGA);
                m_uiEventProgressTimer = 1000;
                m_uiTimeout = WAVE3_TIMEOUT;
                break;
            case 25:
                m_uiEventProgressTimer = 1000;
                if (m_pInstance->GetData(TYPE_WARDEN_4) != DONE)
                    return;
                m_uiTimeout = 0;
                m_creature->InterruptNonMeleeSpells(false);
                m_creature->remove_auras(SPELL_AURA_DUMMY);
                break;
            //
            // BOSS WAVE
            //
            case 26: // One final cell remains...
                m_creature->SetFacingTo(4.6f);
                DoScriptText(YELL_RELEASE_5, m_creature);
                m_pInstance->SetData(
                    TYPE_WARDEN_5, IN_PROGRESS); // Opens the pod
                m_uiEventProgressTimer = 7000;
                break;
            case 27:
                SpawnWave(5);
                m_uiEventProgressTimer = 26000;
                break;
            case 28: // Welcome, O great one...
                DoScriptText(YELL_WELCOME, m_creature);
                m_uiEventProgressTimer = 7000;
                break;
            default:
                m_uiEventProgressTimer = 0;
                break;
            }
            ++m_uiPhase;
        }
        else
            m_uiEventProgressTimer -= uiDiff;
    }
};

void npc_warden_mellicharAI::SpawnWave(uint32 wave)
{
    switch (wave)
    {
    case 1:
        m_creature->SummonCreature(
            urand(0, 1) ? ENTRY_TRICKSTER : ENTRY_PH_HUNTER,
            aSummonPosition[0][0], aSummonPosition[0][1], aSummonPosition[0][2],
            aSummonPosition[0][3], TEMPSUMMON_CORPSE_TIMED_DESPAWN,
            2 * 60 * 1000);
        break;
    case 2:
        m_creature->SummonCreature(NPC_MILLHOUSE, aSummonPosition[1][0],
            aSummonPosition[1][1], aSummonPosition[1][2], aSummonPosition[1][3],
            TEMPSUMMON_CORPSE_TIMED_DESPAWN, 2 * 60 * 1000);
        break;
    case 3:
        m_creature->SummonCreature(urand(0, 1) ? ENTRY_AKKIRIS : ENTRY_SULFURON,
            aSummonPosition[2][0], aSummonPosition[2][1], aSummonPosition[2][2],
            aSummonPosition[2][3], TEMPSUMMON_CORPSE_TIMED_DESPAWN,
            2 * 60 * 1000);
        break;
    case 4:
        m_creature->SummonCreature(urand(0, 1) ? ENTRY_TW_DRAK : ENTRY_BL_DRAK,
            aSummonPosition[3][0], aSummonPosition[3][1], aSummonPosition[3][2],
            aSummonPosition[3][3], TEMPSUMMON_CORPSE_TIMED_DESPAWN,
            2 * 60 * 1000);
        break;
    case 5:
        m_creature->SummonCreature(ENTRY_SKYRISS, aSummonPosition[4][0],
            aSummonPosition[4][1], aSummonPosition[4][2], aSummonPosition[4][3],
            TEMPSUMMON_CORPSE_TIMED_DESPAWN, 30 * 60 * 1000);
        break;
    default:
        break;
    }
}

CreatureAI* GetAI_npc_warden_mellichar(Creature* pCreature)
{
    return new npc_warden_mellicharAI(pCreature);
}

/*#####
# mob_zerekethvoidzone (this script probably not needed in future ->
`creature_template_addon`.`auras`='36120 0')
#####*/

enum
{
    SPELL_CONSUMPTION_N = 36120,
    SPELL_CONSUMPTION_H = 39003,
};

struct MANGOS_DLL_DECL mob_zerekethvoidzoneAI : public ScriptedAI
{
    mob_zerekethvoidzoneAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();
        m_uiDespawnTimer = 20000;
        Reset();
    }

    uint32 m_uiDespawnTimer;
    bool m_bIsRegularMode;

    void Reset() override
    {
        SetCombatMovement(false);
        DoCastSpellIfCan(m_creature,
            m_bIsRegularMode ? SPELL_CONSUMPTION_N : SPELL_CONSUMPTION_H);
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (m_uiDespawnTimer <= uiDiff)
            m_creature->ForcedDespawn();
        else
            m_uiDespawnTimer -= uiDiff;
    }
};
CreatureAI* GetAI_mob_zerekethvoidzoneAI(Creature* pCreature)
{
    return new mob_zerekethvoidzoneAI(pCreature);
}

enum
{
    SAY_DRAKONARR_AGGRO = -1552032,
    MAX_DRAKONAAR_SPELLS = 5,
};

uint32 drakonaarSpells[MAX_DRAKONAAR_SPELLS][2] = {
    {22560, 39033}, // Black
    {22559, 39037}, // Blue
    {22642, 39036}, // Bronze
    {22561, 22561}, // Green
    {22558, 39034}, // Red
};

struct MANGOS_DLL_DECL boss_twilight_drakonaarAI : public ScriptedAI
{
    boss_twilight_drakonaarAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();
        m_pInstance = (instance_arcatraz*)m_creature->GetInstanceData();
        Reset();
    }

    void Reset() override
    {
        SelectOrder();
        m_uiCooldown = 2000;
    }

    bool m_bIsRegularMode;
    instance_arcatraz* m_pInstance;
    std::vector<uint32> m_spellIndices;
    uint32 m_uiCooldown;

    void Aggro(Unit* /*pWho*/) override
    {
        DoScriptText(SAY_DRAKONARR_AGGRO, m_creature);
    }
    void EnterEvadeMode(bool by_group = false) override
    {
        if (m_pInstance)
            m_pInstance->SetData(TYPE_HARBINGERSKYRISS, FAIL);
        ScriptedAI::EnterEvadeMode(by_group);
    }
    void JustDied(Unit* /*pKiller*/) override
    {
        if (m_pInstance)
            m_pInstance->SetData(TYPE_WARDEN_4, DONE);

        DoScriptText(TYPE_WARDEN_4, m_creature);
    }

    void SelectOrder()
    {
        m_spellIndices.clear();
        m_spellIndices.reserve(MAX_DRAKONAAR_SPELLS);
        std::vector<uint32> indices(MAX_DRAKONAAR_SPELLS);
        for (uint32 i = 0; i < MAX_DRAKONAAR_SPELLS; ++i)
            indices[i] = i;
        while (!indices.empty())
        {
            uint32 i = urand(0, indices.size() - 1);
            m_spellIndices.push_back(indices[i]);
            indices.erase(indices.begin() + i);
        }
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        DoMeleeAttackIfReady();

        if (m_uiCooldown)
        {
            if (m_uiCooldown <= uiDiff)
                m_uiCooldown = 0;
            else
            {
                m_uiCooldown -= uiDiff;
                return;
            }
        }

        if (m_spellIndices.empty())
        {
            SelectOrder();
            m_uiCooldown = 10000;
            return;
        }

        if (DoCastSpellIfCan(m_creature,
                m_bIsRegularMode ?
                    drakonaarSpells[m_spellIndices[0]][0] :
                    drakonaarSpells[m_spellIndices[0]][1]) == CAST_OK)
        {
            m_spellIndices.erase(m_spellIndices.begin());
            m_uiCooldown = 1500;
        }

        // DoMeleeAttackIfReady() Above
    }
};
CreatureAI* GetAI_boss_twilight_drakonaarAI(Creature* pCreature)
{
    return new boss_twilight_drakonaarAI(pCreature);
}

struct MANGOS_DLL_DECL npc_negaton_screamerAI : public ScriptedAI
{
    npc_negaton_screamerAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();
        Reset();
    }

    void Reset() override
    {
        m_uiSpellTimer = 4000;
        m_uiFearTimer = urand(10000, 20000);
        m_bSchoolSelectDone = false;
        m_bSchoolReselectCd = 0;
        m_uiSelectedSpellId = 0;
    }

    void SpellHit(Unit* /*pCaster*/, const SpellEntry* pSpell) override
    {
        if (m_bSchoolSelectDone && m_bSchoolReselectCd)
            return;

        if (!pSpell->HasEffect(SPELL_EFFECT_SCHOOL_DAMAGE) &&
            !pSpell->HasApplyAuraName(SPELL_AURA_PERIODIC_DAMAGE))
            return;

        uint32 def_spell = 0, sel_spell;
        std::string school;
        switch (pSpell->SchoolMask)
        {
        case SPELL_SCHOOL_MASK_ARCANE:
            def_spell = 34331;
            sel_spell = m_bIsRegularMode ? 36738 : 38835;
            school = "arcane";
            break;
        case SPELL_SCHOOL_MASK_FIRE:
            def_spell = 34333;
            sel_spell = m_bIsRegularMode ? 36742 : 38836;
            school = "fire";
            break;
        case SPELL_SCHOOL_MASK_FROST:
            def_spell = 34334;
            sel_spell = m_bIsRegularMode ? 36741 : 38837;
            school = "frost";
            break;
        case SPELL_SCHOOL_MASK_HOLY:
            def_spell = 34336;
            sel_spell = m_bIsRegularMode ? 36743 : 38838;
            school = "holy";
            break;
        case SPELL_SCHOOL_MASK_NATURE:
            def_spell = 34335;
            sel_spell = m_bIsRegularMode ? 36740 : 38839;
            school = "nature";
            break;
        case SPELL_SCHOOL_MASK_SHADOW:
            def_spell = 34338;
            sel_spell = m_bIsRegularMode ? 36736 : 38840;
            school = "shadow";
            break;
        default:
            return;
        };

        // Don't change the school if it's the one we already have
        if (m_uiSelectedSpellId == sel_spell)
            return;

        // Remove the other auras
        m_creature->remove_auras(34331);
        m_creature->remove_auras(34333);
        m_creature->remove_auras(34334);
        m_creature->remove_auras(34335);
        m_creature->remove_auras(34336);
        m_creature->remove_auras(34338);

        // Apply new one
        m_uiSelectedSpellId = sel_spell;
        DoCastSpellIfCan(m_creature, def_spell, CAST_TRIGGERED);
        m_bSchoolSelectDone = true;
        m_bSchoolReselectCd = 15000;

        // Do the quote
        std::string quote("%s absorbs the ");
        quote.append(school);
        quote.append(" energy of the attack.");
        m_creature->MonsterTextEmote(quote.c_str(), NULL);
    }

    bool m_bIsRegularMode;
    bool m_bSchoolSelectDone;
    uint32 m_uiSpellTimer;
    uint32 m_bSchoolReselectCd;
    uint32 m_uiSelectedSpellId;
    uint32 m_uiFearTimer;

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (m_bSchoolReselectCd)
        {
            if (m_bSchoolReselectCd > uiDiff)
                m_bSchoolReselectCd -= uiDiff;
            else
                m_bSchoolReselectCd = 0;
        }

        if (m_bSchoolSelectDone)
        {
            if (m_uiSpellTimer <= uiDiff)
            {
                if (DoCastSpellIfCan(m_creature, m_uiSelectedSpellId) ==
                    CAST_OK)
                    m_uiSpellTimer = 10000;
            }
            else
                m_uiSpellTimer -= uiDiff;
        }

        if (m_uiFearTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, 13704) == CAST_OK)
                m_uiFearTimer = urand(10000, 20000);
            ;
        }
        else
            m_uiFearTimer -= uiDiff;

        DoMeleeAttackIfReady();
    }
};
CreatureAI* GetAI_npc_negaton_screamerAI(Creature* pCreature)
{
    return new npc_negaton_screamerAI(pCreature);
}

void AddSC_arcatraz()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "npc_millhouse_manastorm";
    pNewScript->GetAI = &GetAI_npc_millhouse_manastorm;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_warden_mellichar";
    pNewScript->GetAI = &GetAI_npc_warden_mellichar;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "mob_zerekethvoidzone";
    pNewScript->GetAI = &GetAI_mob_zerekethvoidzoneAI;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "boss_twilight_drakonaar";
    pNewScript->GetAI = &GetAI_boss_twilight_drakonaarAI;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_negaton_screamer";
    pNewScript->GetAI = &GetAI_npc_negaton_screamerAI;
    pNewScript->RegisterSelf();
}
