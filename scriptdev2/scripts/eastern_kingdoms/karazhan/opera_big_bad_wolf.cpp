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
SDName: Big Bad Wolf
SD%Complete: 100
SDComment:
SDCategory: Karazhan
EndScriptData */

#include "karazhan.h"
#include "precompiled.h"

/**************************************/
/**** Opera Red Riding Hood Event ****/
/************************************/

/**** Yells for the Wolf ****/
enum
{
    SAY_WOLF_AGGRO = -1532043,
    SAY_WOLF_KILL = -1532044,
    SAY_WOLF_HOOD = -1532045,
    SOUND_WOLF_DEATH = 9275, // Only sound on death, no text.

    /**** Spells For The Wolf ****/
    SPELL_LITTLE_RED_RIDING_HOOD = 30768,
    SPELL_TERRIFYING_HOWL = 30752,
    SPELL_WIDE_SWIPE = 30761,

    TEXT_GRANDMA_1 = 15001,
    TEXT_GRANDMA_2 = 15002,
    TEXT_GRANDMA_3 = 15003,

    /**** The Wolf's Entry ****/
    NPC_BIG_BAD_WOLF = 17521
};

#define GOSSIP_GRANDMA_1 "Oh, grandmother, what big ears you have."
#define GOSSIP_GRANDMA_2 "Oh, grandmother, what big eyes you have."
#define GOSSIP_GRANDMA_3 "Oh, grandmother, what phat lewts you have."

bool GossipHello_npc_grandmother(Player* pPlayer, Creature* pCreature)
{
    pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_GRANDMA_1,
        GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 10);
    pPlayer->SEND_GOSSIP_MENU(TEXT_GRANDMA_1, pCreature->GetObjectGuid());

    return true;
}

bool GossipSelect_npc_grandmother(
    Player* pPlayer, Creature* pCreature, uint32 /*uiSender*/, uint32 uiAction)
{
    switch (uiAction)
    {
    case GOSSIP_ACTION_INFO_DEF + 10:
        pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_GRANDMA_2,
            GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 20);
        pPlayer->SEND_GOSSIP_MENU(TEXT_GRANDMA_2, pCreature->GetObjectGuid());
        break;

    case GOSSIP_ACTION_INFO_DEF + 20:
        pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_GRANDMA_3,
            GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 30);
        pPlayer->SEND_GOSSIP_MENU(TEXT_GRANDMA_3, pCreature->GetObjectGuid());
        break;

    case GOSSIP_ACTION_INFO_DEF + 30:
    {
        if (Creature* bigBadWolf = pCreature->SummonCreature(NPC_BIG_BAD_WOLF,
                pCreature->GetX(), pCreature->GetY(), pCreature->GetZ(), 4.6f,
                TEMPSUMMON_CORPSE_TIMED_DESPAWN,
                sWorld::Instance()->getConfig(
                    CONFIG_UINT32_CORPSE_DECAY_WORLDBOSS) *
                    1000))
        {
            if (bigBadWolf->AI())
                bigBadWolf->AI()->AttackStart(pPlayer);

            pCreature->ForcedDespawn();
        }
        break;
    }
    }

    return true;
}

struct MANGOS_DLL_DECL boss_bigbadwolfAI : public ScriptedAI
{
    boss_bigbadwolfAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_instance = pCreature->GetMapId() == KARAZHAN_MAP_ID ?
                         (instance_karazhan*)pCreature->GetInstanceData() :
                         NULL;
        Reset();
    }

    instance_karazhan* m_instance;
    ObjectGuid m_ridingHood;
    uint32 m_chaseTimer;
    uint32 m_fearTimer;
    uint32 m_swipeTimer;
    bool m_isChasing;

    void Reset() override
    {
        m_ridingHood.Clear();
        m_chaseTimer = urand(1000, 5000);
        m_fearTimer = urand(10000, 15000);
        m_swipeTimer = urand(25000, 35000);
        m_isChasing = false;
    }

    void Aggro(Unit* /*who*/) override
    {
        DoScriptText(SAY_WOLF_AGGRO, m_creature);
    }

    void JustReachedHome() override
    {
        if (m_instance)
            m_instance->SetData(TYPE_OPERA, FAIL);
        m_creature->ForcedDespawn();
    }

    void JustDied(Unit* /*killer*/) override
    {
        DoPlaySoundToSet(m_creature, SOUND_WOLF_DEATH);
        if (m_instance)
            m_instance->SetData(TYPE_OPERA, DONE);
    }

    void SpellHitTarget(Unit* pTarget, const SpellEntry* pSpellEntry) override
    {
        // FIXME: WTB Cleaner solution
        static const uint32 spell_daze = 1604;
        if (pSpellEntry->Id == spell_daze)
            pTarget->remove_auras(spell_daze);
    }

    void KilledUnit(Unit* victim) override
    {
        if (victim->GetObjectGuid() == m_ridingHood)
            m_creature->SetFocusTarget(nullptr);

        DoKillSay(m_creature, victim, SAY_WOLF_KILL);
    }

    void UpdateAI(const uint32 diff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        // Chasing
        if (m_chaseTimer <= diff)
        {
            if (!m_isChasing)
            {
                if (Unit* target = m_creature->SelectAttackingTarget(
                        ATTACKING_TARGET_RANDOM, 0,
                        SPELL_LITTLE_RED_RIDING_HOOD))
                {
                    DoScriptText(SAY_WOLF_HOOD, m_creature);
                    DoCastSpellIfCan(
                        target, SPELL_LITTLE_RED_RIDING_HOOD, CAST_TRIGGERED);
                    m_creature->SetCanRun(false);
                    m_creature->SetFocusTarget(target);

                    m_chaseTimer = 20000;
                    m_isChasing = true;
                    m_ridingHood = target->GetObjectGuid();
                }
            }
            else
            {
                m_ridingHood.Clear();
                m_creature->SetFocusTarget(NULL);
                m_isChasing = false;
                m_chaseTimer = 10000;
                m_creature->SetCanRun(true);
                m_creature->SetFocusTarget(nullptr);
            }
        }
        else
            m_chaseTimer -= diff;

        if (m_swipeTimer <= diff)
        {
            // Cannot swipe while chasing
            if (!m_isChasing &&
                DoCastSpellIfCan(m_creature->getVictim(), SPELL_WIDE_SWIPE) ==
                    CAST_OK)
                m_swipeTimer = urand(25000, 35000);
        }
        else
            m_swipeTimer -= diff;

        if (m_fearTimer <= diff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_TERRIFYING_HOWL) == CAST_OK)
                m_fearTimer = urand(20000, 30000);
        }
        else
            m_fearTimer -= diff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_bigbadwolf(Creature* pCreature)
{
    return new boss_bigbadwolfAI(pCreature);
}

void Opera_BidBadWolf()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->pGossipHello = &GossipHello_npc_grandmother;
    pNewScript->pGossipSelect = &GossipSelect_npc_grandmother;
    pNewScript->Name = "npc_grandmother";
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->GetAI = &GetAI_boss_bigbadwolf;
    pNewScript->Name = "boss_bigbadwolf";
    pNewScript->RegisterSelf();
}
