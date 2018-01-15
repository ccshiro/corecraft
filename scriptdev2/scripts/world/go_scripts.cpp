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
SDName: GO_Scripts
SD%Complete: 100
SDComment: Quest support: 5088, 5097, 5098, 5381, 6481, 10990, 10991, 10992,
Barov_journal->Teaches spell 26089
SDCategory: Game Objects
EndScriptData */

/* ContentData
go_cat_figurine (the "trap" version of GO, two different exist)
go_barov_journal
go_ethereum_prison
go_ethereum_stasis
go_resonite_cask
go_sacred_fire_of_life
go_shrine_of_the_birds
go_andorhal_tower
go_refreshtable
go_soulwell
go_hand_of_iruxos_crystal
EndContentData */

#include "precompiled.h"

/*######
## go_cat_figurine
######*/

enum
{
    SPELL_SUMMON_GHOST_SABER = 5968,
};

bool GOUse_go_cat_figurine(Player* pPlayer, GameObject* /*pGo*/)
{
    pPlayer->CastSpell(pPlayer, SPELL_SUMMON_GHOST_SABER, true);
    return false;
}

/*######
## go_barov_journal
######*/

enum
{
    SPELL_TAILOR_FELCLOTH_BAG = 26086,
    SPELL_LEARN_FELCLOTH_BAG = 26095
};

bool GOUse_go_barov_journal(Player* pPlayer, GameObject* /*pGo*/)
{
    if (pPlayer->HasSkill(SKILL_TAILORING) &&
        pPlayer->GetBaseSkillValue(SKILL_TAILORING) >= 280 &&
        !pPlayer->HasSpell(SPELL_TAILOR_FELCLOTH_BAG))
    {
        pPlayer->CastSpell(pPlayer, SPELL_LEARN_FELCLOTH_BAG, false);
    }
    return true;
}

/*######
## go_ethereum_prison
######*/

enum
{
    FACTION_LC = 1011,
    FACTION_SHAT = 935,
    FACTION_CE = 942,
    FACTION_CON = 933,
    FACTION_KT = 989,
    FACTION_SPOR = 970,

    SPELL_REP_LC = 39456,
    SPELL_REP_SHAT = 39457,
    SPELL_REP_CE = 39460,
    SPELL_REP_CON = 39474,
    SPELL_REP_KT = 39475,
    SPELL_REP_SPOR = 39476
};

const uint32 uiNpcPrisonEntry[] = {
    22810, 22811, 22812, 22813, 22814, 22815,       // good guys
    20783, 20784, 20785, 20786, 20788, 20789, 20790 // bad guys
};

bool GOUse_go_ethereum_prison(Player* pPlayer, GameObject* pGo)
{
    uint8 uiRandom = urand(0, (sizeof(uiNpcPrisonEntry) / sizeof(uint32)) - 1);

    if (Creature* pCreature =
            pPlayer->SummonCreature(uiNpcPrisonEntry[uiRandom], pGo->GetX(),
                pGo->GetY(), pGo->GetZ(), pGo->GetAngle(pPlayer),
                TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 30000))
    {
        if (!pCreature->IsHostileTo(pPlayer))
        {
            uint32 uiSpell = 0;

            if (FactionTemplateEntry const* pFaction =
                    pCreature->getFactionTemplateEntry())
            {
                switch (pFaction->faction)
                {
                case FACTION_LC:
                    uiSpell = SPELL_REP_LC;
                    break;
                case FACTION_SHAT:
                    uiSpell = SPELL_REP_SHAT;
                    break;
                case FACTION_CE:
                    uiSpell = SPELL_REP_CE;
                    break;
                case FACTION_CON:
                    uiSpell = SPELL_REP_CON;
                    break;
                case FACTION_KT:
                    uiSpell = SPELL_REP_KT;
                    break;
                case FACTION_SPOR:
                    uiSpell = SPELL_REP_SPOR;
                    break;
                }

                if (uiSpell)
                    pCreature->CastSpell(pPlayer, uiSpell, false);
                else
                    logging.error(
                        "SD2: go_ethereum_prison summoned creature (entry %u) "
                        "but faction (%u) are not expected by script.",
                        pCreature->GetEntry(), pCreature->getFaction());
            }
        }
    }

    return false;
}

/*######
## go_ethereum_stasis
######*/

const uint32 uiNpcStasisEntry[] = {22825, 20888, 22827, 22826, 22828};

bool GOUse_go_ethereum_stasis(Player* pPlayer, GameObject* pGo)
{
    uint8 uiRandom = urand(0, (sizeof(uiNpcStasisEntry) / sizeof(uint32)) - 1);

    pPlayer->SummonCreature(uiNpcStasisEntry[uiRandom], pGo->GetX(),
        pGo->GetY(), pGo->GetZ(), pGo->GetAngle(pPlayer),
        TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 30000);

    return false;
}

/*######
## go_gilded_brazier
######*/

enum
{
    NPC_STILLBLADE = 17716,
};

bool GOUse_go_gilded_brazier(Player* pPlayer, GameObject* pGO)
{
    if (pGO->GetGoType() == GAMEOBJECT_TYPE_GOOBER)
    {
        if (Creature* pCreature = pPlayer->SummonCreature(NPC_STILLBLADE,
                8087.632f, -7542.740f, 151.568f, 0.122f,
                TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 5000))
            pCreature->AI()->AttackStart(pPlayer);
    }

    return true;
}

/*######
## go_jump_a_tron
######*/

enum
{
    SPELL_JUMP_A_TRON = 33382,
    NPC_JUMP_A_TRON = 19041
};

bool GOUse_go_jump_a_tron(Player* pPlayer, GameObject* pGo)
{
    if (Creature* pCreature = GetClosestCreatureWithEntry(
            pGo, NPC_JUMP_A_TRON, INTERACTION_DISTANCE))
        pCreature->CastSpell(pPlayer, SPELL_JUMP_A_TRON, false);

    return false;
}

/*######
## go_resonite_cask
######*/

enum
{
    NPC_GOGGEROC = 11920
};

bool GOUse_go_resonite_cask(Player* /*pPlayer*/, GameObject* pGO)
{
    if (pGO->GetGoType() == GAMEOBJECT_TYPE_GOOBER)
        pGO->SummonCreature(NPC_GOGGEROC, 0.0f, 0.0f, 0.0f, 0.0f,
            TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 300000);

    return false;
}

/*######
## go_sacred_fire_of_life
######*/

enum
{
    NPC_ARIKARA = 10882,
};

bool GOUse_go_sacred_fire_of_life(Player* pPlayer, GameObject* pGO)
{
    if (pGO->GetGoType() == GAMEOBJECT_TYPE_GOOBER)
        pPlayer->SummonCreature(NPC_ARIKARA, -5008.338f, -2118.894f, 83.657f,
            0.874f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 30000);

    return true;
}

/*######
## go_shrine_of_the_birds
######*/

enum
{
    NPC_HAWK_GUARD = 22992,
    NPC_EAGLE_GUARD = 22993,
    NPC_FALCON_GUARD = 22994,
    GO_SHRINE_HAWK = 185551,
    GO_SHRINE_EAGLE = 185547,
    GO_SHRINE_FALCON = 185553
};

bool GOUse_go_shrine_of_the_birds(Player* pPlayer, GameObject* pGo)
{
    uint32 uiBirdEntry = 0;

    auto pos = pGo->GetPoint(0.0f, INTERACTION_DISTANCE);

    switch (pGo->GetEntry())
    {
    case GO_SHRINE_HAWK:
        uiBirdEntry = NPC_HAWK_GUARD;
        break;
    case GO_SHRINE_EAGLE:
        uiBirdEntry = NPC_EAGLE_GUARD;
        break;
    case GO_SHRINE_FALCON:
        uiBirdEntry = NPC_FALCON_GUARD;
        break;
    }

    if (uiBirdEntry)
        pPlayer->SummonCreature(uiBirdEntry, pos.x, pos.y, pos.z, pGo->GetO(),
            TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 60000);

    return false;
}

enum
{
    NPC_ZELEMAR_THE_WRATHFULL = 17830,
    SAY_AGGRO = -1000579
};

float Position[4] = {-327.99f, 221.74f, -20.31f, 3.87f};

bool GOUse_go_blood_filled_orb(Player* pPlayer, GameObject* pGo)
{
    if (Creature* pZelemar = pGo->SummonCreature(NPC_ZELEMAR_THE_WRATHFULL,
            Position[0], Position[1], Position[2], Position[3],
            TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 30000))
    {
        DoScriptText(SAY_AGGRO, pZelemar);
        pZelemar->AI()->AttackStart(pPlayer);
    }
    return false;
}

/*######
## go_andorhal_tower
######*/

enum
{
    QUEST_ALL_ALONG_THE_WATCHTOWERS_ALLIANCE = 5097,
    QUEST_ALL_ALONG_THE_WATCHTOWERS_HORDE = 5098,
    NPC_ANDORHAL_TOWER_1 = 10902,
    NPC_ANDORHAL_TOWER_2 = 10903,
    NPC_ANDORHAL_TOWER_3 = 10904,
    NPC_ANDORHAL_TOWER_4 = 10905,
    GO_ANDORHAL_TOWER_1 = 176094,
    GO_ANDORHAL_TOWER_2 = 176095,
    GO_ANDORHAL_TOWER_3 = 176096,
    GO_ANDORHAL_TOWER_4 = 176097
};

bool GOUse_go_andorhal_tower(Player* pPlayer, GameObject* pGo)
{
    if (pPlayer->GetQuestStatus(QUEST_ALL_ALONG_THE_WATCHTOWERS_ALLIANCE) ==
            QUEST_STATUS_INCOMPLETE ||
        pPlayer->GetQuestStatus(QUEST_ALL_ALONG_THE_WATCHTOWERS_HORDE) ==
            QUEST_STATUS_INCOMPLETE)
    {
        uint32 uiKillCredit = 0;
        switch (pGo->GetEntry())
        {
        case GO_ANDORHAL_TOWER_1:
            uiKillCredit = NPC_ANDORHAL_TOWER_1;
            break;
        case GO_ANDORHAL_TOWER_2:
            uiKillCredit = NPC_ANDORHAL_TOWER_2;
            break;
        case GO_ANDORHAL_TOWER_3:
            uiKillCredit = NPC_ANDORHAL_TOWER_3;
            break;
        case GO_ANDORHAL_TOWER_4:
            uiKillCredit = NPC_ANDORHAL_TOWER_4;
            break;
        }
        if (uiKillCredit)
            pPlayer->KilledMonsterCredit(uiKillCredit);
    }
    return true;
}

enum
{
    GO_SOULWELL = 181621, // Soulwell's Gameobject_template Entry Id.
    SPELL_CREATE_HEALTHSTONE_0 = 34130, // Healthstone Spell Id for having none
                                        // of the Improved Healthstones Talents
                                        // learned.
    SPELL_CREATE_HEALTHSTONE_1 = 34149, // Healthstone Spell Id for having the
                                        // Improved Healthstones (Rank 1) Talent
                                        // learned.
    SPELL_CREATE_HEALTHSTONE_2 = 34150, // Healthstone Spell Id for having the
                                        // Improved Healthstones (Rank 2) Talent
                                        // learned.
    SPELL_IMPROVED_HEALTHSTONES_1 =
        18692, // Improved Healthstones (Rank 1)'s Spell Id.
    SPELL_IMPROVED_HEALTHSTONES_2 =
        18693, // Improved Healthstones (Rank 2)'s Spell Id.
    ITEM_HEALTHSTONE_0 =
        22103, // Healthstone Item Id from SPELL_CREATE_HEALTHSTONE_0.
    ITEM_HEALTHSTONE_1 =
        22104, // Healthstone Item Id from SPELL_CREATE_HEALTHSTONE_1.
    ITEM_HEALTHSTONE_2 =
        22105 // Healthstone Item Id from SPELL_CREATE_HEALTHSTONE_2.
};
/*######
## go_soulwell
######*/

bool GOHello_go_soulwell(Player* pPlayer, GameObject* pGO)
{
    Unit* caster = pGO->GetOwner();
    if (!caster || caster->GetTypeId() != TYPEID_PLAYER)
        return true;

    if (!pPlayer->IsInSameRaidWith(static_cast<Player*>(caster)))
        return true;

    // Repeating this at every use is ugly and inefficient. But as long as we
    // don't have proper
    // GO scripting with at least On Create and On Update events, the other
    // options are no less
    // ugly and hacky.
    uint32 newSpell = 0;
    uint32 newItem = 0;
    if (pGO->GetEntry() == GO_SOULWELL)
    {
        if (caster->has_aura(SPELL_IMPROVED_HEALTHSTONES_2))
        {
            newSpell = SPELL_CREATE_HEALTHSTONE_2;
            newItem = ITEM_HEALTHSTONE_2;
        }
        else if (caster->has_aura(SPELL_IMPROVED_HEALTHSTONES_1))
        {
            newSpell = SPELL_CREATE_HEALTHSTONE_1;
            newItem = ITEM_HEALTHSTONE_1;
        }
        else
        {
            newSpell = SPELL_CREATE_HEALTHSTONE_0;
            newItem = ITEM_HEALTHSTONE_0;
        }
    }

    // Sanity null check
    if (newItem && newSpell)
    {
        if (pPlayer->can_add_item(newItem, 1))
            pGO->AddUse();
        pPlayer->CastSpell(pPlayer, newSpell, true);
    }
    return true;
}

/*######
## go_refreshtable
######*/

enum
{
    GO_REFRESHTABLE = 186812, // Refreshment's Gameobject_template Entry Id.
    SPELL_CONJURE_REFRESHMENT = 43988, // Conjure Refreshmet Spell Id
    ITEM_CONJURED_MANA_BISCUIT =
        34062, // Conjured Mana Biscuit Item Id from SPELL_CONJURE_REFRESHMENT
    CONJURED_MANA_BISCUIT_CAST_COUNT = 20, // The amount of Conjured Mana
                                           // Biscuits per cast from
                                           // SPELL_CONJURE_REFRESHMENT
    CONJURED_MANA_BISCUIT_MAX_COUNT =
        80 // The max acomunt of Conjured Mana Biscuits one player can have
           // (Possibly check db instead to not have this hardcoded?)
};

bool GOHello_go_refreshtable(Player* pPlayer, GameObject* pGO)
{
    Unit* caster = pGO->GetOwner();
    if (!caster || caster->GetTypeId() != TYPEID_PLAYER)
        return true;

    if (!pPlayer->IsInSameRaidWith(static_cast<Player*>(caster)))
        return true;

    if (pGO->GetEntry() != GO_REFRESHTABLE)
        return true;

    if (pPlayer->can_add_item(
            ITEM_CONJURED_MANA_BISCUIT, CONJURED_MANA_BISCUIT_CAST_COUNT))
        pGO->AddUse();
    pPlayer->CastSpell(pPlayer, SPELL_CONJURE_REFRESHMENT, true);

    return true;
}

/*######
## go_hand_of_iruxos_crystal
######*/

/* TODO
 * Actually this script is extremely vague, but as long as there is no valid
 * information
 * hidden in some dark places, this will be the best we can do here :(
 * Do not consider this a well proven script.
 */

enum
{
    // QUEST_HAND_OF_IRUXOS     = 5381,
    NPC_IRUXOS = 11876,
};

bool GOUse_go_hand_of_iruxos_crystal(Player* pPlayer, GameObject* pGo)
{
    if (Creature* pIruxos = pGo->SummonCreature(NPC_IRUXOS, 0.0f, 0.0f, 0.0f,
            pPlayer->GetO() + M_PI_F, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,
            20000))
        pIruxos->AI()->AttackStart(pPlayer);

    return false;
}

void AddSC_go_scripts()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "go_cat_figurine";
    pNewScript->pGOUse = &GOUse_go_cat_figurine;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "go_barov_journal";
    pNewScript->pGOUse = &GOUse_go_barov_journal;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "go_ethereum_prison";
    pNewScript->pGOUse = &GOUse_go_ethereum_prison;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "go_ethereum_stasis";
    pNewScript->pGOUse = &GOUse_go_ethereum_stasis;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "go_gilded_brazier";
    pNewScript->pGOUse = &GOUse_go_gilded_brazier;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "go_jump_a_tron";
    pNewScript->pGOUse = &GOUse_go_jump_a_tron;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "go_resonite_cask";
    pNewScript->pGOUse = &GOUse_go_resonite_cask;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "go_sacred_fire_of_life";
    pNewScript->pGOUse = &GOUse_go_sacred_fire_of_life;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "go_shrine_of_the_birds";
    pNewScript->pGOUse = &GOUse_go_shrine_of_the_birds;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "go_blood_filled_orb";
    pNewScript->pGOUse = &GOUse_go_blood_filled_orb;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "go_andorhal_tower";
    pNewScript->pGOUse = &GOUse_go_andorhal_tower;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "go_refreshtable";
    pNewScript->pGOUse = &GOHello_go_refreshtable;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "go_soulwell";
    pNewScript->pGOUse = &GOHello_go_soulwell;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "go_hand_of_iruxos_crystal";
    pNewScript->pGOUse = &GOUse_go_hand_of_iruxos_crystal;
    pNewScript->RegisterSelf();
}
