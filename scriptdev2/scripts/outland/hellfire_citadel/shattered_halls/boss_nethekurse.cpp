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
SDName: Boss_Grand_Warlock_Nethekurse
SD%Complete:
SDComment:
SDCategory: Hellfire Citadel, Shattered Halls
EndScriptData */

/* ContentData
boss_grand_warlock_nethekurse
mob_lesser_shadow_fissure
EndContentData */

#include "precompiled.h"
#include "shattered_halls.h"

#define CONVERT_ATTACK_LEN 4
const int32 PeonAttacked[CONVERT_ATTACK_LEN] = {
    -1540001, -1540002, -1540003, -1540004,
};

#define CONVERT_DIE_LEN 3
const int32 PeonDies[CONVERT_DIE_LEN] = {
    -1540005, -1540006, -1540007,
};

enum
{
    SAY_AGGRO_ALL_PEONS = -1540000,
    SAY_SHADOW_SEER = -1540009,
    SAY_DEATH_COIL = -1540010,
    SAY_SHADOW_FISSURE = -1540011,
    SAY_AGGRO_NO_PEONS = -1540008,
    SAY_AGGRO_PARTIAL_PEONS = -1540013,
    SAY_KILL_1 = -1540012,
    SAY_KILL_2 = -1540014,
    SAY_KILL_3 = -1540015,
    SAY_KILL_4 = -1540016,
    SAY_DIE = -1540017,

    SPELL_DEATH_COIL = 30500,
    SPELL_DARK_SPIN = 30502, // NOTE: We've stopped dark spin casting shadowbolt
                             // altogether, instead we do it in the script
    SPELL_DARK_SHADOWBOLT = 30505, // This is the shadowbolt to cast
    SPELL_SHADOW_FISSURE = 30496,

    SPELL_SHADOW_CLEAVE = 30495,
    SPELL_SHADOW_SLAM_H = 35953,

    SPELL_AOE_DEATH_COIL = 30741,
    SPELL_SHADOW_SEAR = 30735,
    SPELL_HEMORRHAGE = 30478,

    SPELL_CONSUMPTION_N = 32250,
    SPELL_CONSUMPTION_H = 35952,

    NPC_FEL_ORC_CONVERT = 17083,
    GUID_FEL_ORC_ONE = 59479,
    GUID_FEL_ORC_TWO = 59478,
    GUID_FEL_ORC_THREE = 59480,
    GUID_FEL_ORC_FOUR = 59481,
};

struct MANGOS_DLL_DECL boss_grand_warlock_nethekurseAI : public ScriptedAI
{
    boss_grand_warlock_nethekurseAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_pInstance = (ScriptedInstance*)pCreature->GetInstanceData();
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();
        m_rpEventStarted = false;
        Reset();
    }

    ScriptedInstance* m_pInstance;
    bool m_bIsRegularMode;

    // OUT OF COMBAT EVENT
    bool m_rpEventStarted;
    void UpdateConverts();
    void OnConvertAttacked();
    void OnConvertDeath();
    void UpdateRPEvent(uint32 uiDiff);
    void DoTortureOne();
    void DoTortureTwo();
    void DoTortureThree();
    Creature* GetRandomConvert() const;
    uint32 GetConvertNr(uint32 dbguid) const;
    bool m_bLoadedConverts;
    std::vector<ObjectGuid> m_aliveConverts;
    bool m_saidAttacked[4];
    uint32 m_uiRpEventTimer;

    // In combat stuff
    uint32 m_uiLesserShadowFissureTimer;
    uint32 m_uiShadowCleaveTimer;
    uint32 m_uiDeathCoilTimer;
    uint32 m_uiDarkShadowboltTimer;

    void Reset() override
    {
        m_uiShadowCleaveTimer = m_bIsRegularMode ? 16000 : 8000;
        m_uiLesserShadowFissureTimer = urand(14000, 18000);
        m_uiDeathCoilTimer = urand(10000, 18000);

        m_bLoadedConverts = false;
        m_saidAttacked[0] = m_saidAttacked[1] = m_saidAttacked[2] =
            m_saidAttacked[3] = false;
        m_uiRpEventTimer = 2500;
        m_uiDarkShadowboltTimer = 0;
    }

    void JustReachedHome() override
    {
        m_pInstance->SetData(TYPE_NETHEKURSE, NOT_STARTED);
    }

    void Aggro(Unit* pWho) override
    {
        if (pWho->GetTypeId() != TYPEID_PLAYER) // For some reason he sometimes
                                                // says these emote after
                                                // resetting if this check's not
                                                // here (but doesn't actually
                                                // aggro someone)
            return;

        m_creature->InterruptNonMeleeSpells(false);
        if (m_aliveConverts.size() == 4)
            DoScriptText(SAY_AGGRO_ALL_PEONS, m_creature);
        else if (!m_aliveConverts.empty()) // Empty is handled in OnConvertDeath
            DoScriptText(SAY_AGGRO_PARTIAL_PEONS, m_creature);
        if (!m_aliveConverts.empty())
        {
            // Make all alive orcs assist us
            for (auto& elem : m_aliveConverts)
                if (Creature* c = m_creature->GetMap()->GetCreature(elem))
                    if (c->AI() && c->isAlive())
                        c->AI()->AttackStart(pWho);
        }
        m_pInstance->SetData(TYPE_NETHEKURSE, IN_PROGRESS);
    }

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(
            m_creature, victim, SAY_KILL_1, SAY_KILL_2, SAY_KILL_3, SAY_KILL_4);
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        DoScriptText(SAY_DIE, m_creature);

        if (!m_pInstance)
            return;

        m_pInstance->SetData(TYPE_NETHEKURSE, DONE);
        if (GameObject* doorOne = m_pInstance->GetSingleGameObjectFromStorage(
                GO_NETHEKURSE_ENTER_DOOR))
            doorOne->SetGoState(GO_STATE_ACTIVE);
        if (GameObject* doorTwo =
                m_pInstance->GetSingleGameObjectFromStorage(GO_NETHEKURSE_DOOR))
            doorTwo->SetGoState(GO_STATE_ACTIVE);
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
        {
            if (m_rpEventStarted)
                UpdateRPEvent(uiDiff);
            else if (m_pInstance)
            {
                if (Player* plr = m_creature->FindNearestPlayer(120.0f))
                {
                    if (plr->GetDistance(178.6f, 200.2f, -26.4f) <
                        20.0f) // Sewers start pos
                        m_rpEventStarted = true;
                    else if (GameObject* door =
                                 m_pInstance->GetSingleGameObjectFromStorage(
                                     GO_NETHEKURSE_ENTER_DOOR)) // Opening the
                                                                // door stats
                                                                // the event too
                        if (door->GetGoState() == GO_STATE_ACTIVE)
                            m_rpEventStarted = true;
                }
            }
            return;
        }

        if (m_creature->GetHealthPercent() > 30.0f)
        {
            if (m_uiShadowCleaveTimer < uiDiff)
            {
                if (DoCastSpellIfCan(m_creature->getVictim(),
                        m_bIsRegularMode ? SPELL_SHADOW_CLEAVE :
                                           SPELL_SHADOW_SLAM_H) == CAST_OK)
                {
                    if (m_bIsRegularMode)
                    {
                        if (urand(1, 4) == 4)
                            m_uiShadowCleaveTimer = 32000;
                        else
                            m_uiShadowCleaveTimer = 16000;
                    }
                    else
                        m_uiShadowCleaveTimer = urand(8000, 16000);
                }
            }
            else
                m_uiShadowCleaveTimer -= uiDiff;

            if (m_uiLesserShadowFissureTimer < uiDiff)
            {
                if (Unit* pTarget = m_creature->SelectAttackingTarget(
                        ATTACKING_TARGET_RANDOM, 0, SPELL_SHADOW_FISSURE))
                {
                    if (DoCastSpellIfCan(pTarget, SPELL_SHADOW_FISSURE) ==
                        CAST_OK)
                        m_uiLesserShadowFissureTimer = 8500;
                }
            }
            else
                m_uiLesserShadowFissureTimer -= uiDiff;

            if (m_uiDeathCoilTimer < uiDiff)
            {
                if (Unit* pTarget = m_creature->SelectAttackingTarget(
                        ATTACKING_TARGET_RANDOM, 0, SPELL_DEATH_COIL))
                {
                    if (DoCastSpellIfCan(pTarget, SPELL_DEATH_COIL) == CAST_OK)
                        m_uiDeathCoilTimer =
                            m_bIsRegularMode ?
                                urand(10000, 18000) :
                                urand(6000, 12000); // Heroic timer is custom;
                                                    // boss's just too easy on
                                                    // hc
                }
            }
            else
                m_uiDeathCoilTimer -= uiDiff;

            DoMeleeAttackIfReady();
        }
        else
        {
            if (!m_creature->has_aura(SPELL_DARK_SPIN))
            {
                DoCastSpellIfCan(m_creature, SPELL_DARK_SPIN);
                m_uiDarkShadowboltTimer = 1000;
            }
            else
            {
                if (m_uiDarkShadowboltTimer <= uiDiff)
                {
                    // Casting shadowbolt is now part of the script:
                    if (Unit* target = m_creature->SelectAttackingTarget(
                            ATTACKING_TARGET_RANDOM, 0, SPELL_DARK_SHADOWBOLT))
                    {
                        DoCastSpellIfCan(
                            target, SPELL_DARK_SHADOWBOLT, CAST_TRIGGERED);
                        m_uiDarkShadowboltTimer = 1000;
                    }
                }
                else
                    m_uiDarkShadowboltTimer -= uiDiff;
            }
        }
    }
};

/* RP IMPLEMENTATIONS */
void boss_grand_warlock_nethekurseAI::UpdateConverts()
{
    if (!m_bLoadedConverts)
    {
        Creature* orc = NULL;
        orc = m_creature->GetMap()->GetCreature(ObjectGuid(HIGHGUID_UNIT,
            (uint32)NPC_FEL_ORC_CONVERT, (uint32)GUID_FEL_ORC_ONE));
        if (orc)
        {
            if (!orc->isAlive())
                orc->Respawn();
            m_aliveConverts.push_back(orc->GetObjectGuid());
            orc->SetAggroDistance(1);
        }
        orc = m_creature->GetMap()->GetCreature(ObjectGuid(HIGHGUID_UNIT,
            (uint32)NPC_FEL_ORC_CONVERT, (uint32)GUID_FEL_ORC_TWO));
        if (orc)
        {
            if (!orc->isAlive())
                orc->Respawn();
            m_aliveConverts.push_back(orc->GetObjectGuid());
            orc->SetAggroDistance(1);
        }
        orc = m_creature->GetMap()->GetCreature(ObjectGuid(HIGHGUID_UNIT,
            (uint32)NPC_FEL_ORC_CONVERT, (uint32)GUID_FEL_ORC_THREE));
        if (orc)
        {
            if (!orc->isAlive())
                orc->Respawn();
            m_aliveConverts.push_back(orc->GetObjectGuid());
            orc->SetAggroDistance(1);
        }
        orc = m_creature->GetMap()->GetCreature(ObjectGuid(HIGHGUID_UNIT,
            (uint32)NPC_FEL_ORC_CONVERT, (uint32)GUID_FEL_ORC_FOUR));
        if (orc)
        {
            if (!orc->isAlive())
                orc->Respawn();
            m_aliveConverts.push_back(orc->GetObjectGuid());
            orc->SetAggroDistance(1);
        }
        m_bLoadedConverts = true;
        return;
    }

    // Converts are scripted with SmartAI and right now there's no communication
    // between these two AIs, so we have to do this ghetto hackily:
    for (std::vector<ObjectGuid>::iterator itr = m_aliveConverts.begin();
         itr != m_aliveConverts.end();)
    {
        bool remove = true;
        if (Creature* orc = m_creature->GetMap()->GetCreature(*itr))
            if (orc->isAlive())
            {
                remove = false;
                if (orc->isInCombat() && orc->getVictim() &&
                    !m_saidAttacked[GetConvertNr(orc->GetGUIDLow()) - 1])
                {
                    m_saidAttacked[GetConvertNr(orc->GetGUIDLow()) - 1] = true;
                    OnConvertAttacked();
                }
            }
        if (remove)
        {
            itr = m_aliveConverts.erase(itr); // delete before call
            OnConvertDeath();
        }
        else
            ++itr;
    }
}

void boss_grand_warlock_nethekurseAI::OnConvertDeath()
{
    // That was the last one, time to attack
    if (m_aliveConverts.empty())
    {
        DoScriptText(SAY_AGGRO_NO_PEONS, m_creature);
        if (Player* pl = m_creature->FindNearestPlayer(120))
            m_creature->AI()->AttackStart(pl);
        return;
    }

    if (Player* pl = m_creature->FindNearestPlayer(120))
    {
        m_creature->movement_gens.push(
            new movement::StoppedMovementGenerator(3000));
        m_creature->SetFacingTo(m_creature->GetAngle(pl));
        m_creature->HandleEmoteCommand(21); // Applaud
        DoScriptText(PeonDies[urand(0, CONVERT_DIE_LEN)], m_creature);
        m_uiRpEventTimer += 3500;
    }
}

void boss_grand_warlock_nethekurseAI::OnConvertAttacked()
{
    if (Player* pl = m_creature->FindNearestPlayer(120))
    {
        m_creature->movement_gens.push(
            new movement::StoppedMovementGenerator(1500));
        m_creature->SetFacingTo(m_creature->GetAngle(pl));
        DoScriptText(PeonAttacked[urand(0, CONVERT_ATTACK_LEN)], m_creature);
        m_uiRpEventTimer += 2000;
    }
}

uint32 boss_grand_warlock_nethekurseAI::GetConvertNr(uint32 dbguid) const
{
    switch (dbguid)
    {
    case GUID_FEL_ORC_ONE:
        return 1;
    case GUID_FEL_ORC_TWO:
        return 2;
    case GUID_FEL_ORC_THREE:
        return 3;
    case GUID_FEL_ORC_FOUR:
        return 4;
    default:
        return 0;
    }
}

Creature* boss_grand_warlock_nethekurseAI::GetRandomConvert() const
{
    std::vector<Creature*> potentialTargets;
    for (const auto& elem : m_aliveConverts)
    {
        if (Creature* c = m_creature->GetMap()->GetCreature(elem))
            if (!c->isInCombat())
                potentialTargets.push_back(c);
    }

    return potentialTargets.empty() ?
               NULL :
               *(potentialTargets.begin() +
                   urand(0, potentialTargets.size() - 1));
}

void boss_grand_warlock_nethekurseAI::DoTortureOne()
{
    if (Creature* convert = GetRandomConvert())
    {
        m_creature->SetFacingTo(m_creature->GetAngle(convert));
        DoCastSpellIfCan(convert, SPELL_SHADOW_SEAR);
        DoScriptText(SAY_SHADOW_SEER, m_creature);
        switch (urand(0, 3))
        {
        case 0:
            convert->MonsterYell("Skin on fire!", 0);
            break;
        case 1:
            convert->MonsterYell("This not good tickle!", 0);
            break;
        case 2:
            convert->MonsterYell("No more scary!", 0);
            break;
        case 3:
            convert->MonsterYell("Augh! No more hurt!", 0);
            break;
        }
    }
    m_creature->movement_gens.push(
        new movement::StoppedMovementGenerator(5500));
    m_uiRpEventTimer = 12500;
}

void boss_grand_warlock_nethekurseAI::DoTortureTwo()
{
    if (Creature* convert = GetRandomConvert())
    {
        m_creature->SetFacingTo(m_creature->GetAngle(convert));
        DoCastSpellIfCan(convert, SPELL_AOE_DEATH_COIL);
        DoScriptText(SAY_DEATH_COIL, m_creature);
    }
    m_creature->movement_gens.push(
        new movement::StoppedMovementGenerator(2500));
    m_uiRpEventTimer = 12500;
}

void boss_grand_warlock_nethekurseAI::DoTortureThree()
{
    if (Creature* convert = GetRandomConvert())
    {
        m_creature->SetFacingTo(m_creature->GetAngle(convert));
        DoCastSpellIfCan(convert, SPELL_SHADOW_FISSURE);
        DoScriptText(SAY_SHADOW_FISSURE, m_creature);
        switch (urand(0, 3))
        {
        case 0:
            convert->MonsterYell("Graaagggh!!", 0);
            break;
        case 1:
            convert->MonsterYell("Pain!", 0);
            break;
        case 2:
            convert->MonsterYell("It hurts!", 0);
            break;
        case 3:
            convert->MonsterYell("Aahhh!", 0);
            break;
        }
    }
    m_creature->movement_gens.push(
        new movement::StoppedMovementGenerator(1500));
    m_uiRpEventTimer = 8000;
}

void boss_grand_warlock_nethekurseAI::UpdateRPEvent(uint32 uiDiff)
{
    UpdateConverts();

    if (m_aliveConverts.empty())
        return;

    if (m_uiRpEventTimer <= uiDiff)
    {
        // Selet new torture funsie event
        switch (urand(0, 2))
        {
        case 0:
            DoTortureOne();
            break;
        case 1:
            DoTortureTwo();
            break;
        case 2:
            DoTortureThree();
            break;
        }
    }
    else
        m_uiRpEventTimer -= uiDiff;
}

// NOTE: this creature are also summoned by other spells, for different
// creatures (Shiro: if this is true, which I doubt, add some exception to
// updateai)
struct MANGOS_DLL_DECL mob_lesser_shadow_fissureAI : public ScriptedAI
{
    mob_lesser_shadow_fissureAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();
        Creature* nethekurse =
            m_creature->GetMap()->GetCreature(ObjectGuid(HIGHGUID_UNIT,
                (uint32)NPC_NETHEKURSE, (uint32)NPC_NETHEKURSE_GUID));
        if (nethekurse && nethekurse->isInCombat() && nethekurse->getVictim())
            m_uiDespawnTimer = 40000;
        else
            m_uiDespawnTimer = 6000; // Only keep it for a short while if part
                                     // of the RP event (aka nethekurse not in
                                     // combat)
        Reset();
    }

    bool m_bIsRegularMode;

    void Reset() override { SetCombatMovement(false); }

    void MoveInLineOfSight(Unit* /*pWho*/) override {}
    void AttackStart(Unit* /*pWho*/) override {}

    uint32 m_uiDespawnTimer;

    void UpdateAI(const uint32 uiDiff) override
    {
        if (m_uiDespawnTimer < uiDiff)
            m_creature->ForcedDespawn();
        else
            m_uiDespawnTimer -= uiDiff;

        if (m_bIsRegularMode)
        {
            if (!m_creature->has_aura(SPELL_CONSUMPTION_N))
                m_creature->CastSpell(m_creature, SPELL_CONSUMPTION_N, true);
        }
        else
        {
            if (!m_creature->has_aura(SPELL_CONSUMPTION_H))
                m_creature->CastSpell(m_creature, SPELL_CONSUMPTION_H, true);
        }
    }
};

CreatureAI* GetAI_boss_grand_warlock_nethekurse(Creature* pCreature)
{
    return new boss_grand_warlock_nethekurseAI(pCreature);
}

CreatureAI* GetAI_mob_lesser_shadow_fissure(Creature* pCreature)
{
    return new mob_lesser_shadow_fissureAI(pCreature);
}

void AddSC_boss_grand_warlock_nethekurse()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_grand_warlock_nethekurse";
    pNewScript->GetAI = &GetAI_boss_grand_warlock_nethekurse;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "mob_lesser_shadow_fissure";
    pNewScript->GetAI = &GetAI_mob_lesser_shadow_fissure;
    pNewScript->RegisterSelf();
}
