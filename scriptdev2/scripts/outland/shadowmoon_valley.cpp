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
SDName: Shadowmoon_Valley
SD%Complete: 100
SDComment: Quest support: 10804, 11020. Vendor Drake Dealer Hurlunk.
SDCategory: Shadowmoon Valley
EndScriptData */

/* ContentData
npc_dragonmaw_peon
npc_drake_dealer_hurlunk
npc_spawned_oronok_tornheart
EndContentData */

#include "escort_ai.h"
#include "pet_ai.h"
#include "precompiled.h"

/*#####
# npc_dragonmaw_peon
#####*/

enum
{
    SAY_PEON_1 = -1000652,
    SAY_PEON_2 = -1000653,
    SAY_PEON_3 = -1000654,
    SAY_PEON_4 = -1000655,
    SAY_PEON_5 = -1000656,

    SPELL_SERVING_MUTTON = 40468,
    NPC_DRAGONMAW_KILL_CREDIT = 23209,
    EQUIP_ID_MUTTON = 2202,
    POINT_DEST = 1
};

struct MANGOS_DLL_DECL npc_dragonmaw_peonAI : public ScriptedAI
{
    npc_dragonmaw_peonAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        Reset();
    }

    ObjectGuid m_playerGuid;
    uint32 m_uiPoisonTimer;
    uint32 m_uiMoveTimer;
    uint32 m_uiEatTimer;

    void Reset() override
    {
        m_playerGuid.Clear();
        m_uiPoisonTimer = 0;
        m_uiMoveTimer = 0;
        m_uiEatTimer = 0;

        SetEquipmentSlots(true);
    }

    bool SetPlayerTarget(ObjectGuid playerGuid)
    {
        // Check if event already started
        if (m_playerGuid)
            return false;

        m_playerGuid = playerGuid;
        m_uiMoveTimer = 500;
        return true;
    }

    void MovementInform(movement::gen uiType, uint32 uiPointId) override
    {
        if (uiType != movement::gen::point)
            return;

        if (uiPointId == POINT_DEST)
        {
            m_uiEatTimer = 2000;
            m_uiPoisonTimer = 3000;

            switch (urand(0, 4))
            {
            case 0:
                DoScriptText(SAY_PEON_1, m_creature);
                break;
            case 1:
                DoScriptText(SAY_PEON_2, m_creature);
                break;
            case 2:
                DoScriptText(SAY_PEON_3, m_creature);
                break;
            case 3:
                DoScriptText(SAY_PEON_4, m_creature);
                break;
            case 4:
                DoScriptText(SAY_PEON_5, m_creature);
                break;
            }
        }
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (m_uiMoveTimer)
        {
            if (m_uiMoveTimer <= uiDiff)
            {
                if (Player* pPlayer =
                        m_creature->GetMap()->GetPlayer(m_playerGuid))
                {
                    GameObject* pMutton =
                        pPlayer->GetGameObject(SPELL_SERVING_MUTTON);

                    // Workaround for broken function GetGameObject
                    if (!pMutton)
                    {
                        const SpellEntry* pSpell =
                            GetSpellStore()->LookupEntry(SPELL_SERVING_MUTTON);

                        uint32 uiGameobjectEntry =
                            pSpell->EffectMiscValue[EFFECT_INDEX_0];

                        // this can fail, but very low chance
                        pMutton = GetClosestGameObjectWithEntry(pPlayer,
                            uiGameobjectEntry, 2 * INTERACTION_DISTANCE);
                    }

                    if (pMutton)
                    {
                        auto pos =
                            pMutton->GetPoint(m_creature, CONTACT_DISTANCE);
                        m_creature->movement_gens.push(
                            new movement::PointMovementGenerator(
                                POINT_DEST, pos.x, pos.y, pos.z, true, true),
                            movement::EVENT_LEAVE_COMBAT);
                    }
                }

                m_uiMoveTimer = 0;
            }
            else
                m_uiMoveTimer -= uiDiff;
        }
        else if (m_uiEatTimer)
        {
            if (m_uiEatTimer <= uiDiff)
            {
                SetEquipmentSlots(false, EQUIP_ID_MUTTON, EQUIP_UNEQUIP);
                m_creature->HandleEmote(EMOTE_ONESHOT_EAT_NOSHEATHE);
                m_uiEatTimer = 0;
            }
            else
                m_uiEatTimer -= uiDiff;
        }
        else if (m_uiPoisonTimer)
        {
            if (m_uiPoisonTimer <= uiDiff)
            {
                if (Player* pPlayer =
                        m_creature->GetMap()->GetPlayer(m_playerGuid))
                    pPlayer->KilledMonsterCredit(
                        NPC_DRAGONMAW_KILL_CREDIT, m_creature->GetObjectGuid());

                m_uiPoisonTimer = 0;

                // dies
                m_creature->SetDeathState(JUST_DIED);
                m_creature->SetHealth(0);
            }
            else
                m_uiPoisonTimer -= uiDiff;
        }
    }
};

CreatureAI* GetAI_npc_dragonmaw_peon(Creature* pCreature)
{
    return new npc_dragonmaw_peonAI(pCreature);
}

bool EffectDummyCreature_npc_dragonmaw_peon(Unit* pCaster, uint32 uiSpellId,
    SpellEffectIndex uiEffIndex, Creature* pCreatureTarget)
{
    if (uiEffIndex != EFFECT_INDEX_1 || uiSpellId != SPELL_SERVING_MUTTON ||
        pCaster->GetTypeId() != TYPEID_PLAYER)
        return false;

    npc_dragonmaw_peonAI* pPeonAI =
        dynamic_cast<npc_dragonmaw_peonAI*>(pCreatureTarget->AI());

    if (!pPeonAI)
        return false;

    if (pPeonAI->SetPlayerTarget(pCaster->GetObjectGuid()))
    {
        pCreatureTarget->HandleEmote(EMOTE_ONESHOT_NONE);
        return true;
    }

    return false;
}

/*######
## npc_drake_dealer_hurlunk
######*/

bool GossipHello_npc_drake_dealer_hurlunk(Player* pPlayer, Creature* pCreature)
{
    if (pCreature->isVendor() &&
        pPlayer->GetReputationRank(1015) == REP_EXALTED)
        pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_VENDOR, GOSSIP_TEXT_BROWSE_GOODS,
            GOSSIP_SENDER_MAIN, GOSSIP_ACTION_TRADE);

    pPlayer->SEND_GOSSIP_MENU(
        pPlayer->GetGossipTextId(pCreature), pCreature->GetObjectGuid());

    return true;
}

bool GossipSelect_npc_drake_dealer_hurlunk(
    Player* pPlayer, Creature* pCreature, uint32 /*uiSender*/, uint32 uiAction)
{
    if (uiAction == GOSSIP_ACTION_TRADE)
        pPlayer->SEND_VENDORLIST(pCreature->GetObjectGuid());

    return true;
}

void AddSC_shadowmoon_valley()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "npc_dragonmaw_peon";
    pNewScript->GetAI = &GetAI_npc_dragonmaw_peon;
    pNewScript->pEffectDummyNPC = &EffectDummyCreature_npc_dragonmaw_peon;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_drake_dealer_hurlunk";
    pNewScript->pGossipHello = &GossipHello_npc_drake_dealer_hurlunk;
    pNewScript->pGossipSelect = &GossipSelect_npc_drake_dealer_hurlunk;
    pNewScript->RegisterSelf();
}
