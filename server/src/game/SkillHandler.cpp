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

#include "Common.h"
#include "logging.h"
#include "Opcodes.h"
#include "Player.h"
#include "SpellMgr.h"
#include "UpdateMask.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Database/DatabaseEnv.h"

void WorldSession::HandleLearnTalentOpcode(WorldPacket& recv_data)
{
    uint32 talent_id, requested_rank;
    recv_data >> talent_id >> requested_rank;

    _player->LearnTalent(talent_id, requested_rank);
}

void WorldSession::HandleTalentWipeConfirmOpcode(WorldPacket& recv_data)
{
    LOG_DEBUG(logging, "MSG_TALENT_WIPE_CONFIRM");
    ObjectGuid guid;
    recv_data >> guid;

    Creature* unit =
        GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_TRAINER);
    if (!unit)
    {
        LOG_DEBUG(logging,
            "WORLD: HandleTalentWipeConfirmOpcode - %s not found or you can't "
            "interact with him.",
            guid.GetString().c_str());
        return;
    }

    // remove fake death
    if (GetPlayer()->hasUnitState(UNIT_STAT_DIED))
        GetPlayer()->remove_auras(SPELL_AURA_FEIGN_DEATH);

    if (!(_player->resetTalents()))
    {
        WorldPacket data(
            MSG_TALENT_WIPE_CONFIRM, 8 + 4); // you have not any talent
        data << uint64(0);
        data << uint32(0);
        send_packet(std::move(data));
        return;
    }

    unit->CastSpell(_player, 14867, true); // spell: "Untalent Visual Effect"
}

void WorldSession::HandleUnlearnSkillOpcode(WorldPacket& recv_data)
{
    uint32 skill_id;
    recv_data >> skill_id;

    const SkillRaceClassInfoEntry* entry = nullptr;

    auto bounds =
        sSpellMgr::Instance()->GetSkillRaceClassInfoMapBounds(skill_id);
    for (auto itr = bounds.first; itr != bounds.second; ++itr)
    {
        entry = itr->second;
        break;
    }

    if (!entry || !(entry->flags & ABILITY_SKILL_ABANDONABLE))
        return;

    GetPlayer()->SetSkill(skill_id, 0, 0);
}
