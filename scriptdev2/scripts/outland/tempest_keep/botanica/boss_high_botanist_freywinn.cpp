/* Copyright (C) 2012 CoreCraft */

/* ScriptData
SDName: Boss_High_Botanist_Freywinn
SD%Complete: 100
SDComment:
SDCategory: Tempest Keep, The Botanica
EndScriptData */

#include "precompiled.h"
#include <vector>

const float GHETTO_TRANQ_RADIUS =
    15.0f; // is 6.0 in the dbc, but we make it a bit harder

enum
{
    SAY_AGGRO = -1553000,
    SAY_KILL_1 = -1553001,
    SAY_KILL_2 = -1553002,
    SAY_TREE_1 = -1553003,
    SAY_TREE_2 = -1553004,
    SAY_DEATH = -1553005,

    SPELL_WHITE_SEEDLING = 34759,
    SPELL_GREEN_SEEDLING = 34761,
    SPELL_BLUE_SEEDLING = 34762,
    SPELL_RED_SEEDLING = 34763,
    SPELL_FRAYER_PROTECTOR = 34557,
    SPELL_TREEFORM = 34551,
    SPELL_TRANQUILITY = 34550,

    NPC_FRAYER_PROTECTOR = 19953,
    NPC_RESEARCHER = 18421,
};

enum FreywinnPhase
{
    CASTER_PHASE = 1,
    TREE_PHASE
};

struct boss_high_botanist_freywinnAI : public ScriptedAI
{
    bool m_bIsRegularMode;
    boss_high_botanist_freywinnAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_bIsRegularMode = m_creature->GetMap()->IsRegularDifficulty();
        m_uiResearchStage = 0;
        m_uiCurrentFreySay = 0;
        m_researchGuid = ObjectGuid();
        m_home = true;
        Reset();
    }

    void Aggro(Unit* /*pWho*/) override
    {
        DoScriptText(SAY_AGGRO, m_creature);
        m_home = false;
    }

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_KILL_1, SAY_KILL_2);
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        DoScriptText(SAY_DEATH, m_creature);

        DesummonAdds();
    }

    // RP FUNCTIONS (Implemented below)
    bool m_home;
    void DoIntroRP(const uint32 uiDiff);
    Creature* GetResearcher();
    uint32 m_uiResearchTimer;
    uint32 m_uiFreyTimer;
    uint32 m_uiResearchStage;
    uint32 m_uiFreyStage;
    uint32 m_uiCurrentFreySay;
    ObjectGuid m_researchGuid;

    FreywinnPhase m_currentPhase;

    // Phase One
    uint32 m_uiTreePhaseTimer;
    uint32 m_uiSeedlingTimer;

    // Phase Two
    uint32 m_uiFrayerCheckTimer;
    std::vector<ObjectGuid> m_Frayers;
    uint32 m_uiDeadProtectors;
    uint32 m_uiPhaseTwoTimeout;
    bool m_bHasCastedTranquility;

    std::vector<ObjectGuid> m_summonedAdds;

    void Reset() override
    {
        m_currentPhase = CASTER_PHASE;
        m_uiSeedlingTimer = 5000;
        m_uiTreePhaseTimer = 15000;
        m_bHasCastedTranquility = false;

        m_uiResearchTimer = 0;
        m_uiFreyTimer = 0;
        m_uiFreyStage = 0;

        DesummonAdds();

        m_creature->movement_gens.remove_all(movement::gen::idle);
    }

    void JustReachedHome() override { m_home = true; }

    void DesummonAdds()
    {
        for (auto& elem : m_summonedAdds)
        {
            if (Creature* pCreature = m_creature->GetMap()->GetCreature(elem))
            {
                pCreature->SetVisibility(VISIBILITY_OFF);
                pCreature->DealDamage(pCreature, pCreature->GetHealth(), NULL,
                    DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, NULL, false,
                    false);
            }
        }
        m_summonedAdds.clear();
    }

    void JustSummoned(Creature* pCreature) override
    {
        m_summonedAdds.push_back(pCreature->GetObjectGuid());
    }

    // FIXME: This is a hack to implement tranquility until
    // either we make a generalization that area party means nearby
    // friendly for mobs, or we decide it means creatures part of the new
    // "CreatureGroup" system.
    void HACK_TRANQ()
    {
        if (!m_creature->has_aura(SPELL_TRANQUILITY))
            return;

        for (std::vector<ObjectGuid>::iterator itr = m_summonedAdds.begin();
             itr != m_summonedAdds.end();)
        {
            bool del = true;
            if (Creature* pCreature = m_creature->GetMap()->GetCreature(*itr))
            {
                if (pCreature->isAlive())
                {
                    del = false;
                    if (m_creature->GetDistance(pCreature) >
                        GHETTO_TRANQ_RADIUS)
                    {
                        if (pCreature->has_aura(SPELL_TRANQUILITY))
                            pCreature->remove_auras(SPELL_TRANQUILITY);
                    }
                    else
                    {
                        if (!pCreature->has_aura(SPELL_TRANQUILITY))
                            pCreature->AddAuraThroughNewHolder(
                                SPELL_TRANQUILITY, pCreature);
                    }
                }
            }
            // Might as well do some house-keeping:
            if (del)
                itr = m_summonedAdds.erase(itr);
            else
                ++itr;
        }
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
        {
            if (m_home)
                DoIntroRP(uiDiff);
            return;
        }

        if (m_currentPhase == CASTER_PHASE)
        {
            if (m_uiSeedlingTimer <= uiDiff)
            {
                uint32 spellid = 0;
                switch (urand(0, 3))
                {
                case 0:
                    spellid = SPELL_WHITE_SEEDLING;
                    break;
                case 1:
                    spellid = SPELL_GREEN_SEEDLING;
                    break;
                case 2:
                    spellid = SPELL_BLUE_SEEDLING;
                    break;
                case 3:
                    spellid = SPELL_RED_SEEDLING;
                    break;
                }

                if (DoCastSpellIfCan(m_creature, spellid) == CAST_OK)
                    m_uiSeedlingTimer = 5000;
            }
            else
                m_uiSeedlingTimer -= uiDiff;

            if (m_uiTreePhaseTimer <= uiDiff)
            {
                if (DoCastSpellIfCan(m_creature, SPELL_TREEFORM) == CAST_OK)
                {
                    DoScriptText(
                        urand(0, 1) ? SAY_TREE_1 : SAY_TREE_2, m_creature);

                    m_bHasCastedTranquility = false;
                    m_currentPhase = TREE_PHASE;
                    // m_creature->CastSpell(m_creature, SPELL_FRAYER_PROTECTOR,
                    // true);
                    for (uint8 i = 0; i < 3; ++i)
                    {
                        if (Creature* pSummon =
                                m_creature->SummonCreature(NPC_FRAYER_PROTECTOR,
                                    m_creature->GetX(), m_creature->GetY(),
                                    m_creature->GetZ(), m_creature->GetO(),
                                    TEMPSUMMON_TIMED_DESPAWN, 55000))
                        {
                            m_Frayers.push_back(pSummon->GetObjectGuid());
                        }
                    }
                    m_uiFrayerCheckTimer = 800;
                    m_uiPhaseTwoTimeout = 45000;
                    m_uiTreePhaseTimer = urand(46000, 60000);
                }
            }
            else
                m_uiTreePhaseTimer -= uiDiff;
        }
        else if (m_currentPhase == TREE_PHASE)
        {
            if (!m_bHasCastedTranquility)
            {
                if (DoCastSpellIfCan(m_creature, SPELL_TRANQUILITY) == CAST_OK)
                    m_bHasCastedTranquility = true;
            }

            HACK_TRANQ();

            if (m_uiFrayerCheckTimer <= uiDiff)
            {
                for (std::vector<ObjectGuid>::iterator itr = m_Frayers.begin();
                     itr != m_Frayers.end();)
                {
                    bool del = false;
                    if (Creature* pSummon =
                            m_creature->GetMap()->GetCreature(*itr))
                    {
                        if (!pSummon->isAlive())
                            del = true;
                    }
                    else
                        del = true;

                    if (del)
                        itr = m_Frayers.erase(itr);
                    else
                        ++itr;
                }
                m_uiFrayerCheckTimer = 800;
            }
            else
                m_uiFrayerCheckTimer -= uiDiff;

            if (m_Frayers.size() == 0)
            {
                m_creature->InterruptNonMeleeSpells(false);
                m_creature->remove_auras(SPELL_TREEFORM);
                m_currentPhase = CASTER_PHASE;
            }

            if (m_uiPhaseTwoTimeout <= uiDiff)
            {
                m_creature->InterruptNonMeleeSpells(false);
                m_creature->remove_auras(SPELL_TREEFORM);
                m_currentPhase = CASTER_PHASE;
                for (auto& elem : m_Frayers)
                {
                    if (Creature* pSummon =
                            m_creature->GetMap()->GetCreature(elem))
                        pSummon->ForcedDespawn();
                }
            }
            else
                m_uiPhaseTwoTimeout -= uiDiff;

            // Keep Updating Tree Phase Timer in Tree Phase
            if (m_uiTreePhaseTimer > uiDiff)
                m_uiTreePhaseTimer -= uiDiff;
        }

        if (m_currentPhase == CASTER_PHASE)
            DoMeleeAttackIfReady();
    }
};
CreatureAI* GetAI_boss_high_botanist_freywinn(Creature* pCreature)
{
    return new boss_high_botanist_freywinnAI(pCreature);
}

Creature* boss_high_botanist_freywinnAI::GetResearcher()
{
    if (m_researchGuid)
    {
        if (Creature* pResearcher =
                m_creature->GetMap()->GetCreature(m_researchGuid))
        {
            if (pResearcher->isAlive() && !pResearcher->isInCombat())
                return pResearcher;
        }
    }
    else
    {
        if (Creature* pCreature =
                GetClosestCreatureWithEntry(m_creature, NPC_RESEARCHER, 15.0f))
        {
            m_researchGuid = pCreature->GetObjectGuid();
            return pCreature;
        }
    }

    return NULL;
}

#define SAY_HIGH_BOTA "High Botanist?"
#define EMOTE_RESE_SIGH "%s sighs."
#define SAY_FREY_ONE "...mumble...Petals of Fire...mumble..."
#define SAY_FREY_TWO "...thorny vines...mumble...ouch!"
#define SAY_FREY_THREE "...with the right mixture, perhaps..."
#define SAY_FREY_FOUR "...mumble mumble..."

void boss_high_botanist_freywinnAI::DoIntroRP(const uint32 uiDiff)
{
    if (m_uiFreyTimer <= uiDiff)
    {
        switch (m_uiFreyStage)
        {
        case 0:
            m_creature->movement_gens.push(
                new movement::PointMovementGenerator(
                    0, 120.5f, 450.3f, -4.9f, false, false),
                movement::EVENT_ENTER_COMBAT);
            m_uiFreyTimer = 2500;
            m_uiFreyStage = 1;
            break;
        case 1:
            m_creature->SetFacingTo(4.4f);
            m_creature->HandleEmote(EMOTE_STATE_USESTANDING);
            m_uiFreyTimer = 5000;
            m_uiFreyStage = 2;
            break;
        case 2:
            m_creature->movement_gens.push(
                new movement::PointMovementGenerator(
                    0, 115.6f, 455.7f, -4.9f, false, false),
                movement::EVENT_ENTER_COMBAT);
            m_uiFreyTimer = 2500;
            m_uiFreyStage = 3;
            break;
        case 3:
            m_creature->SetFacingTo(3.3f);
            m_creature->HandleEmote(EMOTE_STATE_USESTANDING);
            m_uiFreyTimer = 5000;
            m_uiFreyStage = 0;
            break;
        }
    }
    else
        m_uiFreyTimer -= uiDiff;

    Unit* pResearcher = GetResearcher();
    if (!pResearcher)
        return;

    if (m_uiResearchTimer <= uiDiff)
    {
        switch (m_uiResearchStage)
        {
        case 0:
            pResearcher->movement_gens.remove_all(movement::gen::idle);
            pResearcher->movement_gens.push(
                new movement::IdleMovementGenerator(162.8, 503.1, -2.4, 0.9));
            pResearcher->NearTeleportTo(
                162.8, 503.1, -2.4, 0.9); // Only at spawn
            m_uiResearchStage = 1;
            m_uiResearchTimer = 3000;
            break;
        case 1:
            pResearcher->movement_gens.remove_all(movement::gen::idle);
            pResearcher->movement_gens.push(new movement::IdleMovementGenerator(
                121.1f, 454.9f, -4.9f, 3.9f));
            pResearcher->movement_gens.push(
                new movement::PointMovementGenerator(
                    0, 121.1f, 454.9f, -4.9f, false, false),
                movement::EVENT_ENTER_COMBAT);
            m_uiResearchStage = 2;
            m_uiResearchTimer = 31500;
            break;
        case 2:
            pResearcher->SetFacingToObject(m_creature);
            pResearcher->MonsterSay(SAY_HIGH_BOTA, 0);
            m_uiResearchStage = 3;
            m_uiResearchTimer = 2400;
            break;
        case 3:
            pResearcher->SetFacingToObject(m_creature);
            pResearcher->MonsterSay(SAY_HIGH_BOTA, 0);
            m_uiResearchStage = 4;
            m_uiResearchTimer = 2400;
            break;
        case 4:
            switch (m_uiCurrentFreySay)
            {
            case 0:
                m_creature->MonsterSay(SAY_FREY_ONE, 0);
                m_uiCurrentFreySay++;
                break;
            case 1:
                m_creature->MonsterSay(SAY_FREY_TWO, 0);
                m_uiCurrentFreySay++;
                break;
            case 2:
                m_creature->MonsterSay(SAY_FREY_THREE, 0);
                m_uiCurrentFreySay++;
                break;
            case 3:
                m_creature->MonsterSay(SAY_FREY_FOUR, 0);
                m_uiCurrentFreySay = 0;
                break;
            default:
                m_uiCurrentFreySay = 0;
                break;
            }
            m_uiResearchStage = 5;
            m_uiResearchTimer = 2400;
            break;
        case 5:
            pResearcher->SetFacingToObject(m_creature);
            pResearcher->MonsterTextEmote(EMOTE_RESE_SIGH, m_creature);
            m_uiResearchStage = 6;
            m_uiResearchTimer = 2400;
            break;
        case 6:
            pResearcher->movement_gens.remove_all(movement::gen::idle);
            pResearcher->movement_gens.push(
                new movement::IdleMovementGenerator(162.8, 503.1, -2.4, 0.9));
            pResearcher->movement_gens.push(
                new movement::PointMovementGenerator(
                    0, 162.8, 503.1, -2.4, false, false),
                movement::EVENT_ENTER_COMBAT);
            m_uiResearchStage = 1;
            m_uiResearchTimer = 34000;
            break;
        }
    }
    else
        m_uiResearchTimer -= uiDiff;
}

void AddSC_boss_high_botanist_freywinn()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_high_botanist_freywinn";
    pNewScript->GetAI = &GetAI_boss_high_botanist_freywinn;
    pNewScript->RegisterSelf();
}
