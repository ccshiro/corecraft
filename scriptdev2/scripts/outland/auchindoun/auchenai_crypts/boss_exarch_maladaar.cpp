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
SDName: Boss_Exarch_Maladaar
SD%Complete: 100
SDComment:
SDCategory: Auchindoun, Auchenai Crypts
EndScriptData */

/* ContentData
mob_stolen_soul
boss_exarch_maladaar
mob_avatar_of_martyred
EndContentData */

#include "auchenai_crypts.h"
#include "precompiled.h"

enum
{
    SPELL_MOONFIRE = 37328,
    SPELL_FIREBALL = 37329,
    SPELL_MIND_FLAY = 37330,
    SPELL_HEMORRHAGE = 37331,
    SPELL_FROSTSHOCK = 37332,
    SPELL_CURSE_OF_AGONY = 37334,
    SPELL_PL_MORTAL_STRIKE = 37335,
    SPELL_FREEZING_TRAP = 37368,
    SPELL_HAMMER_OF_JUSTICE = 37369,

    SPELL_STOLEN_SOUL = 32346,
};

struct MANGOS_DLL_DECL mob_stolen_soulAI : public ScriptedAI
{
    mob_stolen_soulAI(Creature* pCreature) : ScriptedAI(pCreature) { Reset(); }

    uint8 m_uiStolenClass;
    uint32 m_uiSpellTimer;
    ObjectGuid m_stolenTarget;

    void Reset() override { m_uiSpellTimer = 2000; }

    void JustDied(Unit* /*pKiller*/) override
    {
        if (Unit* target = m_creature->GetMap()->GetUnit(m_stolenTarget))
            target->remove_auras(SPELL_STOLEN_SOUL);
    }

    void SetSoulInfo(Unit* pTarget)
    {
        m_uiStolenClass = pTarget->getClass();
        m_creature->SetDisplayId(pTarget->GetDisplayId());
        m_stolenTarget = pTarget->GetObjectGuid();
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (m_uiSpellTimer < uiDiff)
        {
            switch (m_uiStolenClass)
            {
            case CLASS_WARRIOR:
                DoCastSpellIfCan(
                    m_creature->getVictim(), SPELL_PL_MORTAL_STRIKE);
                m_uiSpellTimer = 6000;
                break;
            case CLASS_PALADIN:
                DoCastSpellIfCan(
                    m_creature->getVictim(), SPELL_HAMMER_OF_JUSTICE);
                m_uiSpellTimer = 6000;
                break;
            case CLASS_HUNTER:
                DoCastSpellIfCan(m_creature->getVictim(), SPELL_FREEZING_TRAP);
                m_uiSpellTimer = 20000;
                break;
            case CLASS_ROGUE:
                DoCastSpellIfCan(m_creature->getVictim(), SPELL_HEMORRHAGE);
                m_uiSpellTimer = 10000;
                break;
            case CLASS_PRIEST:
                DoCastSpellIfCan(m_creature->getVictim(), SPELL_MIND_FLAY);
                m_uiSpellTimer = 5000;
                break;
            case CLASS_SHAMAN:
                DoCastSpellIfCan(m_creature->getVictim(), SPELL_FROSTSHOCK);
                m_uiSpellTimer = 8000;
                break;
            case CLASS_MAGE:
                DoCastSpellIfCan(m_creature->getVictim(), SPELL_FIREBALL);
                m_uiSpellTimer = 5000;
                break;
            case CLASS_WARLOCK:
                DoCastSpellIfCan(m_creature->getVictim(), SPELL_CURSE_OF_AGONY);
                m_uiSpellTimer = 20000;
                break;
            case CLASS_DRUID:
                DoCastSpellIfCan(m_creature->getVictim(), SPELL_MOONFIRE);
                m_uiSpellTimer = 10000;
                break;
            }
        }
        else
            m_uiSpellTimer -= uiDiff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_mob_stolen_soul(Creature* pCreature)
{
    return new mob_stolen_soulAI(pCreature);
}

enum
{
    SAY_INTRO = -1558000,
    SAY_SUMMON = -1558001,
    SAY_AGGRO_1 = -1558002,
    SAY_AGGRO_2 = -1558003,
    SAY_AGGRO_3 = -1558004,
    SAY_ROAR = -1558005,
    SAY_SOUL_CLEAVE = -1558006,
    SAY_KILL_1 = -1558007,
    SAY_KILL_2 = -1558008,
    SAY_DEATH = -1558009,

    SPELL_RIBBON_OF_SOULS = 32422,
    SPELL_SOUL_SCREAM = 32421,
    SPELL_STOLEN_SOUL_VISUAL = 32395,
    SPELL_SUMMON_AVATAR = 32424,

    SPELL_PHASE_IN = 33422,
    NPC_AVATAR = 18478,

    NPC_STOLEN_SOUL = 18441,
    NPC_DORE = 19412
};

struct MANGOS_DLL_DECL boss_exarch_maladaarAI : public ScriptedAI
{
    boss_exarch_maladaarAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_pInstance = (instance_auchenai_crypts*)pCreature->GetInstanceData();
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();
        m_bHasTaunted = false;
        Reset();
    }

    instance_auchenai_crypts* m_pInstance;
    bool m_bIsRegularMode;

    ObjectGuid m_targetGuid;

    uint32 m_uiFearTimer;
    uint32 m_uiRibbonOfSoulsTimer;
    uint32 m_uiStolenSoulTimer;

    bool m_bHasTaunted;
    bool m_bHasSummonedAvatar;

    void Reset() override
    {
        m_targetGuid.Clear();

        m_uiFearTimer = urand(15000, 30000);
        m_uiRibbonOfSoulsTimer = urand(4000, 12000);
        m_uiStolenSoulTimer = urand(20000, 25000);

        m_bHasSummonedAvatar = false;
        m_bHasTaunted = false;
    }

    void MoveInLineOfSight(Unit* pWho) override
    {
        if (!m_bHasTaunted && pWho->GetTypeId() == TYPEID_PLAYER &&
            m_creature->IsWithinDistInMap(pWho, 150.0))
        {
            DoScriptText(SAY_INTRO, m_creature);
            m_bHasTaunted = true;
        }

        ScriptedAI::MoveInLineOfSight(pWho);
    }

    void JustReachedHome() override
    {
        if (m_pInstance)
            m_pInstance->SetData(TYPE_EXARCH_MALADAAR, FAIL);
    }

    void Aggro(Unit* /*pWho*/) override
    {
        if (m_pInstance)
            m_pInstance->SetData(TYPE_EXARCH_MALADAAR, IN_PROGRESS);

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
    }

    void JustSummoned(Creature* pSummoned) override
    {
        if (pSummoned->GetEntry() == NPC_STOLEN_SOUL)
        {
            // SPELL_STOLEN_SOUL_VISUAL has shapeshift effect, but not
            // implemented feature in mangos for this spell.
            pSummoned->CastSpell(pSummoned, SPELL_STOLEN_SOUL_VISUAL, false);
            pSummoned->setFaction(m_creature->getFaction());

            if (Player* pTarget = m_creature->GetMap()->GetPlayer(m_targetGuid))
            {
                if (mob_stolen_soulAI* pSoulAI =
                        dynamic_cast<mob_stolen_soulAI*>(pSummoned->AI()))
                {
                    pSoulAI->SetSoulInfo(pTarget);
                    pSoulAI->AttackStart(pTarget);
                }
            }
        }
        else if (pSummoned->GetEntry() == NPC_AVATAR)
        {
            pSummoned->CastSpell(pSummoned, SPELL_PHASE_IN, true);
            if (pSummoned->AI() && m_creature->getVictim())
                pSummoned->AI()->AttackStart(m_creature->getVictim());
        }
    }

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_KILL_1, SAY_KILL_2);
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        if (m_pInstance)
            m_pInstance->SetData(TYPE_EXARCH_MALADAAR, DONE);

        DoScriptText(SAY_DEATH, m_creature);

        // When Exarch Maladaar is defeated D'ore appear.
        DoSpawnCreature(
            NPC_DORE, 0.0f, 0.0f, 0.0f, 0.0f, TEMPSUMMON_TIMED_DESPAWN, 600000);
    }

    Unit* GetSoulStealTarget()
    {
        std::vector<Unit*> targets;
        const ThreatList& tl = m_creature->getThreatManager().getThreatList();
        for (const auto& elem : tl)
        {
            if (Unit* temp =
                    m_creature->GetMap()->GetUnit((elem)->getUnitGuid()))
            {
                if (temp->GetTypeId() == TYPEID_PLAYER &&
                    !temp->has_aura(SPELL_STOLEN_SOUL))
                    targets.push_back(temp);
            }
        }

        if (targets.empty())
            return NULL;

        return targets[urand(0, targets.size() - 1)];
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (!m_bHasSummonedAvatar && m_creature->GetHealthPercent() <= 25.0f)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_SUMMON_AVATAR) == CAST_OK)
            {
                DoScriptText(SAY_SUMMON, m_creature);
                m_bHasSummonedAvatar = true;
                m_uiStolenSoulTimer = urand(15000, 30000);
            }
        }

        if (m_uiStolenSoulTimer <= uiDiff)
        {
            if (Unit* pTarget = GetSoulStealTarget())
            {
                if (DoCastSpellIfCan(pTarget, SPELL_STOLEN_SOUL) == CAST_OK)
                {
                    DoScriptText(
                        urand(0, 1) ? SAY_ROAR : SAY_SOUL_CLEAVE, m_creature);
                    m_targetGuid = pTarget->GetObjectGuid();
                    DoSpawnCreature(NPC_STOLEN_SOUL, 0.0f, 0.0f, 0.0f, 0.0f,
                        TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 10000);
                    m_uiStolenSoulTimer = urand(20000, 25000);
                }
            }
        }
        else
            m_uiStolenSoulTimer -= uiDiff;

        if (m_uiRibbonOfSoulsTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(
                    m_creature->getVictim(), SPELL_RIBBON_OF_SOULS) == CAST_OK)
                m_uiRibbonOfSoulsTimer = urand(4000, 12000);
        }
        else
            m_uiRibbonOfSoulsTimer -= uiDiff;

        if (m_uiFearTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_SOUL_SCREAM) == CAST_OK)
                m_uiFearTimer = urand(15000, 30000);
        }
        else
            m_uiFearTimer -= uiDiff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_exarch_maladaar(Creature* pCreature)
{
    return new boss_exarch_maladaarAI(pCreature);
}

enum
{
    SPELL_MORTAL_STRIKE = 15708, // Mortal strike was changed from either 300%
                                 // or 250% (unsure which of the two) to 200% in
                                 // 2.1; this id is the 300% MS
    SPELL_SUNDER_ARMOR = 16145,
};

struct MANGOS_DLL_DECL mob_avatar_of_martyredAI : public ScriptedAI
{
    mob_avatar_of_martyredAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_pInstance = (instance_auchenai_crypts*)pCreature->GetInstanceData();
        Reset();
    }

    void Aggro(Unit* /*pWho*/) override
    {
        if (m_pInstance && m_pInstance->GetData(TYPE_AVATAR) != DONE)
            m_pInstance->SetData(TYPE_AVATAR, IN_PROGRESS);
    }

    void JustReachedHome() override
    {
        if (m_pInstance && m_pInstance->GetData(TYPE_AVATAR) != DONE)
            m_pInstance->SetData(TYPE_AVATAR, FAIL);
    }

    void JustDied(Unit* /*pWho*/) override
    {
        // Do not award loot if we've been slain before
        if (m_pInstance && m_pInstance->GetData(TYPE_AVATAR) == DONE)
            m_creature->ResetLootRecipients();
        else
            m_pInstance->SetData(TYPE_AVATAR, DONE);
    }

    void Reset() override
    {
        m_uiMortalStrikeTimer = 5000;
        m_uiSunderTimer = urand(7000, 10000);
    }

    instance_auchenai_crypts* m_pInstance;
    uint32 m_uiMortalStrikeTimer;
    uint32 m_uiSunderTimer;

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (m_uiMortalStrikeTimer <= uiDiff)
        {
            DoCastSpellIfCan(m_creature->getVictim(), SPELL_MORTAL_STRIKE);
            m_uiMortalStrikeTimer = urand(10000, 20000);
        }
        else
            m_uiMortalStrikeTimer -= uiDiff;

        if (m_uiSunderTimer <= uiDiff)
        {
            DoCastSpellIfCan(m_creature->getVictim(), SPELL_SUNDER_ARMOR);
            m_uiSunderTimer = urand(7000, 10000);
        }
        else
            m_uiSunderTimer -= uiDiff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_mob_avatar_of_martyred(Creature* pCreature)
{
    return new mob_avatar_of_martyredAI(pCreature);
}

void AddSC_boss_exarch_maladaar()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_exarch_maladaar";
    pNewScript->GetAI = &GetAI_boss_exarch_maladaar;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "mob_avatar_of_martyred";
    pNewScript->GetAI = &GetAI_mob_avatar_of_martyred;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "mob_stolen_soul";
    pNewScript->GetAI = &GetAI_mob_stolen_soul;
    pNewScript->RegisterSelf();
}
