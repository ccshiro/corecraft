/*
 * Copyright (C) 2005-2012 MaNGOS <http://getmangos.com/>
 *
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

#include "TemporarySummon.h"
#include "CreatureAI.h"
#include "logging.h"

TemporarySummon::TemporarySummon(ObjectGuid summoner, uint32 summon_opts)
  : Creature(CREATURE_SUBTYPE_TEMPORARY_SUMMON),
    m_type(TEMPSUMMON_TIMED_OR_CORPSE_DESPAWN), m_timer(0), m_lifetime(0),
    m_summoner(std::move(summoner)), m_summonOpts(summon_opts),
    m_forcedDespawn(false)
{
}

TemporarySummon::~TemporarySummon()
{
}

void TemporarySummon::Update(uint32 update_diff, uint32 diff)
{
    if (unlikely(has_queued_actions()))
        update_queued_actions(update_diff);

    if (m_summonOpts & SUMMON_OPT_DESPAWN_ON_SUMMONER_DEATH)
    {
        auto summoner = GetSummoner();
        if (!summoner || summoner->isDead())
        {
            UnSummon();
            return;
        }
    }

    if (m_forcedDespawn)
    {
        UnSummon();
        return;
    }

    switch (m_type)
    {
    case TEMPSUMMON_MANUAL_DESPAWN:
    {
        break;
    }
    case TEMPSUMMON_TIMED_DESPAWN:
    {
        if (m_timer <= update_diff)
        {
            UnSummon();
            return;
        }

        m_timer -= update_diff;
        break;
    }
    case TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT:
    {
        if (!isInCombat())
        {
            if (m_timer <= update_diff)
            {
                UnSummon();
                return;
            }

            m_timer -= update_diff;
        }
        else if (m_timer != m_lifetime)
            m_timer = m_lifetime;

        break;
    }
    case TEMPSUMMON_CORPSE_TIMED_DESPAWN:
    {
        if (getDeathState() == CORPSE || getDeathState() == DEAD)
        {
            if (m_timer <= update_diff)
            {
                UnSummon();
                return;
            }

            m_timer -= update_diff;
        }
        break;
    }
    case TEMPSUMMON_CORPSE_DESPAWN:
    {
        // if m_deathState is DEAD, CORPSE was skipped
        if (isDead())
        {
            UnSummon();
            return;
        }

        break;
    }
    case TEMPSUMMON_DEAD_DESPAWN:
    {
        if (IsDespawned())
        {
            UnSummon();
            return;
        }
        break;
    }
    case TEMPSUMMON_TIMED_OR_CORPSE_DESPAWN:
    {
        // if m_deathState is DEAD, CORPSE was skipped
        if (isDead())
        {
            // let two seconds pass between death and desummon, for proper death
            // animation display
            if (m_lifetime)
            {
                m_lifetime = 0;
                m_timer = 2000;
                return;
            }
            if (m_timer <= update_diff)
                UnSummon();
            else
                m_timer -= update_diff;
            return;
        }

        if (!isInCombat())
        {
            if (m_timer <= update_diff)
            {
                UnSummon();
                return;
            }
            else
                m_timer -= update_diff;
        }
        else if (m_timer != m_lifetime)
            m_timer = m_lifetime;
        break;
    }
    case TEMPSUMMON_TIMED_OR_DEAD_DESPAWN:
    {
        // if m_deathState is DEAD, CORPSE was skipped
        if (IsDespawned())
        {
            UnSummon();
            return;
        }

        if (!isInCombat() && isAlive())
        {
            if (m_timer <= update_diff)
            {
                UnSummon();
                return;
            }
            else
                m_timer -= update_diff;
        }
        else if (m_timer != m_lifetime)
            m_timer = m_lifetime;
        break;
    }
    case TEMPSUMMON_TIMED_DEATH:
    {
        if (isAlive())
        {
            if (m_timer <= update_diff)
            {
                Kill(this);
                m_timer = 5000; // Remove corpse in 5
            }
            else
                m_timer -= update_diff;
        }
        else
        {
            if (m_timer <= update_diff)
                UnSummon();
            else
                m_timer -= update_diff;
        }
        break;
    }
    default:
        UnSummon();
        logging.error(
            "Temporary summoned creature (entry: %u) have unknown type %u of ",
            GetEntry(), m_type);
        break;
    }

    Creature::Update(update_diff, diff);
}

bool TemporarySummon::Summon(TempSummonType type, uint32 lifetime)
{
    m_type = type;
    m_timer = lifetime;
    m_lifetime = lifetime;

    if (!GetMap()->insert(this))
        return false;

    AIM_Initialize();

    // Summoned creatures should not respawn:
    m_canRespawn = false;

    return true;
}

void TemporarySummon::UnSummon()
{
    CombatStop();

    if (GetSummonerGuid().IsCreature())
        if (Creature* sum = GetMap()->GetCreature(GetSummonerGuid()))
            if (sum->AI())
                sum->AI()->SummonedCreatureDespawn(this);

    AddObjectToRemoveList();
}

void TemporarySummon::SaveToDB()
{
}
