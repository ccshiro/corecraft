/* Copyright (C) 2013, 2015 Corecraft */

/* ScriptData
SDName: boss_soccothrates
SD%Complete: 100
SDComment:
SDCategory: Tempest Keep, The Arcatraz
EndScriptData */

#include "arcatraz.h"
#include "precompiled.h"

enum
{
    SAY_AGGRO = -1530023,
    SAY_KILL_1 = -1530024,
    SAY_KILL_2 = -1530025,
    SAY_CHARGE_1 = -1530026,
    SAY_CHARGE_2 = -1530027,
    SAY_DEATH = -1530028,

    SPELL_FELFIRE_SHOCK_N = 35759,
    SPELL_FELFIRE_SHOCK_H = 39006,

    SPELL_FEL_IMMOLATION = 36051, // Added to creature_template_addon
    SPELL_FEL_IMMOLATION_TICK = 35959,

    // Charge rotation works like this:
    // a) He casts SPELL_KNOCK_AWAY
    // b) 3 seconds later he casts SPELL_CHARGE_TARGETING on a random enemy
    //    This selects a target, makes him point and spawns NPC_CHARGE_TARGET
    // c) He spawns 6 NPC_FELFIRE, that will be placed like: first one on the
    //    NPC_CHARGE_TARGET, then towards him, each patch will not be further
    //    away each other than it resulting in a connected trail of fire (can be
    //    closer though).
    // e) When knock away root fades, he casts SPELL_FELFIRE_LINE_UP, this lets
    //    each fellfire patch know in how long they should cast SPELL_FELFIRE
    //    (based on distance to him).
    // e) Then, he casts SPELL_CHARGE
    // f) When his charge is done, he casts SPELL_CHARGE_DMG (AoE damage)
    SPELL_KNOCK_AWAY = 36512,
    SPELL_CHARGE_TARGETING = 36038,
    SPELL_CHARGE = 35754,
    SPELL_CHARGE_DMG = 36058,

    NPC_FELFIRE = 20978,
    NPC_CHARGE_TARGET = 21030,
    SPELL_FELFIRE_LINE_UP = 35770,
    // SPELL_FELFIRE = 35769, // Felfire used by Charge Target
};

enum class ChargeStages
{
    none,
    knock,
    target,
    felfire_spawn,
    charge,
    mid_charge,
    charge_dmg
};

struct MANGOS_DLL_DECL boss_soccothratesAI : public ScriptedAI
{
    boss_soccothratesAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_pInstance = (instance_arcatraz*)m_creature->GetInstanceData();
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();

        m_uiRpTimer = 1000;
        m_uiRpPhase = 0;

        Reset();
    }

    instance_arcatraz* m_pInstance;
    uint32 m_uiFelfireShockTimer;
    uint32 m_uiKnockAwayTimer;
    uint32 charge_stage_timer;
    ChargeStages charge_stage;
    // RP STUFF:
    uint32 m_uiRpTimer;
    uint32 m_uiRpPhase;
    int32 m_nextSayPct;

    bool m_bIsRegularMode;

    void MovementInform(movement::gen uiMoveType, uint32 uiPointId) override
    {
        if (uiMoveType == movement::gen::point && uiPointId == 100)
        {
            m_creature->SetFacingTo(5.2f);
            m_creature->SetOrientation(5.2f);
            m_creature->movement_gens.remove_all(movement::gen::idle);
            m_creature->movement_gens.push(
                new movement::IdleMovementGenerator());
        }
        else if (uiMoveType == movement::gen::charge &&
                 charge_stage == ChargeStages::mid_charge)
        {
            charge_stage = ChargeStages::charge_dmg;
        }
    }

    void Aggro(Unit* /*pWho*/) override
    {
        if (m_pInstance)
            m_pInstance->SetData(TYPE_SOCCOTHRATES, IN_PROGRESS);

        DoScriptText(SAY_AGGRO, m_creature);
    }

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_KILL_1, SAY_KILL_2);
    }

    void JustReachedHome() override
    {
        if (m_pInstance)
            m_pInstance->SetData(TYPE_SOCCOTHRATES, FAIL);
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        if (m_pInstance)
            m_pInstance->SetData(TYPE_SOCCOTHRATES, DONE);

        DoScriptText(SAY_DEATH, m_creature);
    }

    void SpellDamageCalculation(const Unit* /*pDoneTo*/, int32& iDamage,
        const SpellEntry* pSpell, SpellEffectIndex effectIndex) override
    {
        if (pSpell->Id == SPELL_FEL_IMMOLATION_TICK)
        {
            iDamage = urand(832, 918); // From 2.0.3 spell dbc data
        }
        else if (pSpell->Id == SPELL_KNOCK_AWAY &&
                 effectIndex == EFFECT_INDEX_0)
        {
            iDamage = 100; // Reduced to 50% weapon damage in patch 2.0.10
        }
    }

    void Reset() override
    {
        m_uiFelfireShockTimer = 15000;
        m_uiKnockAwayTimer = 10000;
        m_nextSayPct = 75;
        charge_stage_timer = 0;
        charge_stage = ChargeStages::none;
    }

    void DoRp(const uint32 uiDiff);

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
        {
            DoRp(uiDiff);
            return;
        }

        // Charge logic (see above how it works)
        if (charge_stage != ChargeStages::none)
        {
            if (charge_stage_timer && charge_stage_timer <= uiDiff)
                charge_stage_timer = 0;
            else if (charge_stage_timer)
                charge_stage_timer -= uiDiff;

            if (!charge_stage_timer)
            {
                switch (charge_stage)
                {
                case ChargeStages::none: // Prevent compiler warning
                    break;
                case ChargeStages::knock:
                    if (DoCastSpellIfCan(m_creature, SPELL_KNOCK_AWAY) ==
                        CAST_OK)
                    {
                        DoScriptText(urand(0, 1) ? SAY_CHARGE_1 : SAY_CHARGE_2,
                            m_creature);
                        charge_stage_timer = 3000;
                        charge_stage = ChargeStages::target;
                    }
                    break;
                case ChargeStages::target:
                {
                    Unit* target = m_creature->SelectAttackingTarget(
                        ATTACKING_TARGET_RANDOM, 0, SPELL_CHARGE_TARGETING);
                    if (!target)
                        charge_stage = ChargeStages::none;
                    else
                    {
                        DoCast(target, SPELL_CHARGE_TARGETING);
                        charge_stage = ChargeStages::felfire_spawn;
                        charge_stage_timer = 500;
                    }
                    break;
                }
                case ChargeStages::felfire_spawn:
                {
                    charge_stage = ChargeStages::charge;
                    charge_stage_timer = 500;
                    auto target = GetClosestCreatureWithEntry(
                        m_creature, NPC_CHARGE_TARGET, 100.0f);
                    if (!target)
                    {
                        charge_stage = ChargeStages::none;
                        break;
                    }

                    float max_dist =
                        G3D::distance(m_creature->GetX() - target->GetX(),
                            m_creature->GetY() - target->GetY(),
                            m_creature->GetZ() - target->GetZ());
                    static const float max_gap = 4.0f;
                    float gap = max_dist / 6.0f;
                    if (gap > max_gap)
                        gap = max_gap;

                    for (int i = 0; i < 6; ++i)
                    {
                        float dist = gap * (i - 1) -
                                     m_creature->GetObjectBoundingRadius();
                        G3D::Vector3 pos;
                        if (i == 0)
                            target->GetPosition(pos.x, pos.y, pos.z);
                        else
                            pos = target->GetPoint(m_creature, dist, true);
                        m_creature->SummonCreature(NPC_FELFIRE, pos.x, pos.y,
                            pos.z, m_creature->GetO(), TEMPSUMMON_TIMED_DESPAWN,
                            15000, SUMMON_OPT_NO_LOOT | SUMMON_OPT_ACTIVE);
                    }
                    break;
                }
                case ChargeStages::charge:
                    if (!m_creature->has_aura(SPELL_KNOCK_AWAY))
                    {
                        auto target = GetClosestCreatureWithEntry(
                            m_creature, NPC_CHARGE_TARGET, 100.0f);
                        if (!target)
                        {
                            charge_stage = ChargeStages::none;
                            break;
                        }

                        DoCast(m_creature, SPELL_FELFIRE_LINE_UP, true);
                        DoCast(target, SPELL_CHARGE, true);
                        charge_stage = ChargeStages::mid_charge;
                    }
                    break;
                case ChargeStages::mid_charge:
                    break;
                case ChargeStages::charge_dmg:
                    if (DoCastSpellIfCan(m_creature, SPELL_CHARGE_DMG) ==
                        CAST_OK)
                        charge_stage = ChargeStages::none;
                    break;
                }
            }
        }

        if (m_uiFelfireShockTimer <= uiDiff)
        {
            if (Unit* pTarget = m_creature->SelectAttackingTarget(
                    ATTACKING_TARGET_RANDOM, 0, SPELL_FELFIRE_SHOCK_N))
            {
                if (charge_stage == ChargeStages::none &&
                    DoCastSpellIfCan(pTarget,
                        m_bIsRegularMode ? SPELL_FELFIRE_SHOCK_N :
                                           SPELL_FELFIRE_SHOCK_H) == CAST_OK)
                {
                    m_uiFelfireShockTimer = urand(10000, 20000);
                }
            }
        }
        else
            m_uiFelfireShockTimer -= uiDiff;

        if (m_uiKnockAwayTimer <= uiDiff)
        {
            // Timer is 20 secs after knock away rotation ends, 5 seconds added
            // because it takes ~about that time
            m_uiKnockAwayTimer = 25000;
            charge_stage = ChargeStages::knock;
            charge_stage_timer = 0;
        }
        else
            m_uiKnockAwayTimer -= uiDiff;

        if (m_creature->GetHealthPercent() <= m_nextSayPct)
        {
            Creature* dalliah = m_pInstance ? m_pInstance->GetDalliah() : NULL;
            if (dalliah)
            {
                if (m_nextSayPct == 75)
                    DoScriptText(SAY_DH_SOC_TAUNT_1, dalliah);
                else if (m_nextSayPct == 50)
                    DoScriptText(SAY_DH_SOC_TAUNT_2, dalliah);
                else if (m_nextSayPct == 25)
                    DoScriptText(SAY_DH_SOC_TAUNT_3, dalliah);
            }
            m_nextSayPct -= 25;
        }

        DoMeleeAttackIfReady();
    }
};

void boss_soccothratesAI::DoRp(const uint32 uiDiff)
{
    if (!m_uiRpTimer)
        return;
    if (m_uiRpTimer > uiDiff)
    {
        m_uiRpTimer -= uiDiff;
        return;
    }

    Creature* dalliah = m_pInstance ? m_pInstance->GetDalliah() : NULL;
    if (!dalliah)
        return;

    switch (m_uiRpPhase)
    {
    case 0:
        // We cannot use Move LoS callback since it limits range to a too low
        // amount
        if (m_creature->FindNearestPlayer(70.0f))
            m_uiRpPhase = 1; // No timer change
        else
            m_uiRpTimer = 1000;
        return;

    // ARGUE DIALOG:
    case 1:
        DoScriptText(SAY_SOC_ARGUE_1, m_creature);
        m_uiRpTimer = 2000;
        break;
    case 2:
        DoScriptText(SAY_DAH_ARGUE_1, dalliah);
        m_uiRpTimer = 2000;
        break;
    case 3:
        DoScriptText(SAY_SOC_ARGUE_2, m_creature);
        m_uiRpTimer = 4000;
        break;
    case 4:
        DoScriptText(SAY_DAH_ARGUE_2, dalliah);
        m_uiRpTimer = 5000;
        break;
    case 5:
        DoScriptText(SAY_SOC_ARGUE_3, m_creature);
        m_uiRpTimer = 3000;
        break;
    case 6:
        DoScriptText(SAY_DAH_ARGUE_3, dalliah);
        m_uiRpTimer = 3000;
        break;
    case 7:
        DoScriptText(SAY_SOC_ARGUE_4, m_creature);
        m_uiRpTimer = 5000;
        break;
    // Walk away
    case 8:
        dalliah->movement_gens.push(new movement::PointMovementGenerator(
            100, 118.4f, 95.3f, 22.44f, false, false));
        m_creature->movement_gens.push(new movement::PointMovementGenerator(
            100, 118.0f, 198.1f, 22.44f, false, false));
        m_uiRpTimer = 0; // No more RP to be had
        return;
        ;
    }

    ++m_uiRpPhase;
}

CreatureAI* GetAI_boss_soccothrates(Creature* pCreature)
{
    return new boss_soccothratesAI(pCreature);
}

void AddSC_boss_soccothrates()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_soccothrates";
    pNewScript->GetAI = &GetAI_boss_soccothrates;
    pNewScript->RegisterSelf();
}
