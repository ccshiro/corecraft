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
SDName: Hellfire_Peninsula
SD%Complete: 100
SDComment: Quest support: 9375, 9410, 9418
SDCategory: Hellfire Peninsula
EndScriptData */

/* ContentData
npc_aeranas
npc_ancestral_wolf
npc_wounded_blood_elf
EndContentData */

#include "escort_ai.h"
#include "precompiled.h"

/*######
## npc_aeranas
######*/

#define SAY_SUMMON -1000138
#define SAY_FREE -1000139

#define FACTION_HOSTILE 16
#define FACTION_FRIENDLY 35

#define SPELL_ENVELOPING_WINDS 15535
#define SPELL_SHOCK 12553

struct MANGOS_DLL_DECL npc_aeranasAI : public ScriptedAI
{
    npc_aeranasAI(Creature* pCreature) : ScriptedAI(pCreature) { Reset(); }

    uint32 Faction_Timer;
    uint32 EnvelopingWinds_Timer;
    uint32 Shock_Timer;

    void Reset() override
    {
        Faction_Timer = 8000;
        EnvelopingWinds_Timer = 9000;
        Shock_Timer = 5000;

        m_creature->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER);
        m_creature->setFaction(FACTION_FRIENDLY);

        DoScriptText(SAY_SUMMON, m_creature);
    }

    void UpdateAI(const uint32 diff) override
    {
        if (Faction_Timer)
        {
            if (Faction_Timer <= diff)
            {
                m_creature->setFaction(FACTION_HOSTILE);
                Faction_Timer = 0;
            }
            else
                Faction_Timer -= diff;
        }

        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (m_creature->GetHealthPercent() < 30.0f)
        {
            m_creature->setFaction(FACTION_FRIENDLY);
            m_creature->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER);
            m_creature->remove_auras();
            m_creature->DeleteThreatList();
            m_creature->CombatStop(true);
            DoScriptText(SAY_FREE, m_creature);
            return;
        }

        if (Shock_Timer < diff)
        {
            DoCastSpellIfCan(m_creature->getVictim(), SPELL_SHOCK);
            Shock_Timer = 10000;
        }
        else
            Shock_Timer -= diff;

        if (EnvelopingWinds_Timer < diff)
        {
            DoCastSpellIfCan(m_creature->getVictim(), SPELL_ENVELOPING_WINDS);
            EnvelopingWinds_Timer = 25000;
        }
        else
            EnvelopingWinds_Timer -= diff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_npc_aeranas(Creature* pCreature)
{
    return new npc_aeranasAI(pCreature);
}

/*######
## npc_ancestral_wolf
######*/

enum
{
    EMOTE_WOLF_LIFT_HEAD = -1000496,
    EMOTE_WOLF_HOWL = -1000497,
    SAY_WOLF_WELCOME = -1000498,

    SPELL_ANCESTRAL_WOLF_BUFF = 29981,

    NPC_RYGA = 17123
};

struct MANGOS_DLL_DECL npc_ancestral_wolfAI : public npc_escortAI
{
    npc_ancestral_wolfAI(Creature* pCreature) : npc_escortAI(pCreature)
    {
        if (pCreature->GetOwner() &&
            pCreature->GetOwner()->GetTypeId() == TYPEID_PLAYER)
            Start(false, (Player*)pCreature->GetOwner());
        else
            logging.error(
                "SD2: npc_ancestral_wolf can not obtain owner or owner is not "
                "a player.");

        Reset();
    }

    void Reset() override
    {
        m_creature->CastSpell(m_creature, SPELL_ANCESTRAL_WOLF_BUFF, true);
    }

    void WaypointReached(uint32 uiPointId) override
    {
        switch (uiPointId)
        {
        case 0:
            DoScriptText(EMOTE_WOLF_LIFT_HEAD, m_creature);
            break;
        case 2:
            DoScriptText(EMOTE_WOLF_HOWL, m_creature);
            break;
        case 50:
            Creature* pRyga =
                GetClosestCreatureWithEntry(m_creature, NPC_RYGA, 30.0f);
            if (pRyga && pRyga->isAlive() && !pRyga->isInCombat())
                DoScriptText(SAY_WOLF_WELCOME, pRyga);
            break;
        }
    }
};

CreatureAI* GetAI_npc_ancestral_wolf(Creature* pCreature)
{
    return new npc_ancestral_wolfAI(pCreature);
}

/*######
## npc_wounded_blood_elf
######*/

#define SAY_ELF_START -1000117
#define SAY_ELF_SUMMON1 -1000118
#define SAY_ELF_RESTING -1000119
#define SAY_ELF_SUMMON2 -1000120
#define SAY_ELF_COMPLETE -1000121
#define SAY_ELF_AGGRO -1000122

#define QUEST_ROAD_TO_FALCON_WATCH 9375

struct MANGOS_DLL_DECL npc_wounded_blood_elfAI : public npc_escortAI
{
    npc_wounded_blood_elfAI(Creature* pCreature) : npc_escortAI(pCreature)
    {
        Reset();
    }

    void WaypointReached(uint32 i) override
    {
        Player* pPlayer = GetPlayerForEscort();

        if (!pPlayer)
            return;

        switch (i)
        {
        case 0:
            DoScriptText(SAY_ELF_START, m_creature, pPlayer);
            break;
        case 9:
            DoScriptText(SAY_ELF_SUMMON1, m_creature, pPlayer);
            // Spawn two Haal'eshi Talonguard
            DoSpawnCreature(16967, -15, -15, 0, 0,
                TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 5000);
            DoSpawnCreature(16967, -17, -17, 0, 0,
                TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 5000);
            break;
        case 13:
            DoScriptText(SAY_ELF_RESTING, m_creature, pPlayer);
            break;
        case 14:
            DoScriptText(SAY_ELF_SUMMON2, m_creature, pPlayer);
            // Spawn two Haal'eshi Windwalker
            DoSpawnCreature(16966, -15, -15, 0, 0,
                TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 5000);
            DoSpawnCreature(16966, -17, -17, 0, 0,
                TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 5000);
            break;
        case 27:
            DoScriptText(SAY_ELF_COMPLETE, m_creature, pPlayer);
            // Award quest credit
            pPlayer->GroupEventHappens(QUEST_ROAD_TO_FALCON_WATCH, m_creature);
            break;
        }
    }

    void Reset() override {}

    void Aggro(Unit* /*who*/) override
    {
        if (HasEscortState(STATE_ESCORT_ESCORTING))
            DoScriptText(SAY_ELF_AGGRO, m_creature);
    }

    void JustSummoned(Creature* summoned) override
    {
        summoned->AI()->AttackStart(m_creature);
    }
};

CreatureAI* GetAI_npc_wounded_blood_elf(Creature* pCreature)
{
    return new npc_wounded_blood_elfAI(pCreature);
}

bool QuestAccept_npc_wounded_blood_elf(
    Player* pPlayer, Creature* pCreature, const Quest* pQuest)
{
    if (pQuest->GetQuestId() == QUEST_ROAD_TO_FALCON_WATCH)
    {
        // Change faction so mobs attack
        pCreature->setFaction(FACTION_ESCORT_H_PASSIVE);

        if (npc_wounded_blood_elfAI* pEscortAI =
                dynamic_cast<npc_wounded_blood_elfAI*>(pCreature->AI()))
            pEscortAI->Start(false, pPlayer, pQuest);
    }

    return true;
}

void AddSC_hellfire_peninsula()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "npc_aeranas";
    pNewScript->GetAI = &GetAI_npc_aeranas;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_ancestral_wolf";
    pNewScript->GetAI = &GetAI_npc_ancestral_wolf;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_wounded_blood_elf";
    pNewScript->GetAI = &GetAI_npc_wounded_blood_elf;
    pNewScript->pQuestAcceptNPC = &QuestAccept_npc_wounded_blood_elf;
    pNewScript->RegisterSelf();
}
