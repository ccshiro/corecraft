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

#include "ChannelMgr.h"
#include "Chat.h"
#include "Common.h"
#include "GMTicketMgr.h"
#include "Group.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Language.h"
#include "logging.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "Player.h"
#include "SpecialVisCreature.h"
#include "Totem.h"
#include "TemporarySummon.h"
#include "SpellAuras.h"
#include "Util.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Database/DatabaseEnv.h"
#include "maps/callbacks.h"
#include "maps/visitors.h"

bool WorldSession::processChatmessageFurtherAfterSecurityChecks(
    std::string& msg, uint32 lang)
{
    // Messages longer than 255 characters are automatically truncated
    // (see: http://wowprogramming.com/docs/api/SendAddonMessage)
    if (msg.size() > 255)
        msg.resize(255);

    if (lang != LANG_ADDON)
    {
        // strip invisible characters for non-addon messages
        if (sWorld::Instance()->getConfig(
                CONFIG_BOOL_CHAT_FAKE_MESSAGE_PREVENTING))
            stripLineInvisibleChars(msg);

        if (sWorld::Instance()->getConfig(
                CONFIG_UINT32_CHAT_STRICT_LINK_CHECKING_SEVERITY) &&
            GetSecurity() < SEC_TICKET_GM &&
            !ChatHandler(this).isValidChatMessage(msg.c_str()))
        {
            logging.error(
                "Player %s (GUID: %u) sent a chatmessage with an invalid link: "
                "%s",
                GetPlayer()->GetName(), GetPlayer()->GetGUIDLow(), msg.c_str());
            if (sWorld::Instance()->getConfig(
                    CONFIG_UINT32_CHAT_STRICT_LINK_CHECKING_KICK))
                KickPlayer();
            return false;
        }
    }

    return true;
}

void WorldSession::HandleMessagechatOpcode(WorldPacket& recv_data)
{
    uint32 type;
    uint32 lang;

    recv_data >> type;
    recv_data >> lang;

    if (type >= MAX_CHAT_MSG_TYPE)
    {
        logging.error("CHAT: Wrong message type received: %u", type);
        return;
    }

    LOG_DEBUG(logging, "CHAT: packet received. type %u, lang %u", type, lang);

    // prevent talking at unknown language (cheating)
    LanguageDesc const* langDesc = GetLanguageDescByID(lang);
    if (!langDesc)
    {
        SendNotification(LANG_UNKNOWN_LANGUAGE);
        return;
    }
    if (langDesc->skill_id != 0 && !_player->HasSkill(langDesc->skill_id))
    {
        // also check SPELL_AURA_COMPREHEND_LANGUAGE (client offers option to
        // speak in that language)
        auto& langAuras =
            _player->GetAurasByType(SPELL_AURA_COMPREHEND_LANGUAGE);
        bool foundAura = false;
        for (const auto& langAura : langAuras)
        {
            if ((langAura)->GetModifier()->m_miscvalue == int32(lang))
            {
                foundAura = true;
                break;
            }
        }
        if (!foundAura)
        {
            SendNotification(LANG_NOT_LEARNED_LANGUAGE);
            return;
        }
    }

    if (lang == LANG_ADDON)
    {
        // Disabled addon channel?
        if (!sWorld::Instance()->getConfig(CONFIG_BOOL_ADDON_CHANNEL))
            return;
    }
    // LANG_ADDON should not be changed nor be affected by flood control
    else
    {
        // send in universal language if player in .gmon mode (ignore spell
        // effects)
        if (_player->isGameMaster())
            lang = LANG_UNIVERSAL;
        else
        {
            // send in universal language in two side iteration allowed mode
            if (sWorld::Instance()->getConfig(
                    CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CHAT))
                lang = LANG_UNIVERSAL;
            else
            {
                switch (type)
                {
                case CHAT_MSG_PARTY:
                case CHAT_MSG_RAID:
                case CHAT_MSG_RAID_LEADER:
                case CHAT_MSG_RAID_WARNING:
                    // allow two side chat at group channel if two side group
                    // allowed
                    if (sWorld::Instance()->getConfig(
                            CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GROUP))
                        lang = LANG_UNIVERSAL;
                    break;
                case CHAT_MSG_GUILD:
                case CHAT_MSG_OFFICER:
                    // allow two side chat at guild channel if two side guild
                    // allowed
                    if (sWorld::Instance()->getConfig(
                            CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GUILD))
                        lang = LANG_UNIVERSAL;
                    break;
                }
            }

            // but overwrite it by SPELL_AURA_MOD_LANGUAGE auras (only single
            // case used)
            auto& ModLangAuras =
                _player->GetAurasByType(SPELL_AURA_MOD_LANGUAGE);
            if (!ModLangAuras.empty())
                lang = ModLangAuras.front()->GetModifier()->m_miscvalue;
        }

        if (type != CHAT_MSG_AFK && type != CHAT_MSG_DND)
        {
            if (!_player->CanSpeak())
            {
                std::string timeStr = secsToTimeString(
                    m_muteTime - WorldTimer::time_no_syscall());
                SendNotification(GetMangosString(LANG_WAIT_BEFORE_SPEAKING),
                    timeStr.c_str());
                return;
            }

            GetPlayer()->UpdateSpeakTime();
        }
    }

    switch (type)
    {
    case CHAT_MSG_SAY:
    case CHAT_MSG_EMOTE:
    case CHAT_MSG_YELL:
    {
        std::string msg;
        recv_data >> msg;

        if (msg.empty())
            break;

        if (ChatHandler(this).ParseCommands(msg.c_str()))
            break;

        if (!processChatmessageFurtherAfterSecurityChecks(msg, lang))
            return;

        if (msg.empty())
            break;

        if (type == CHAT_MSG_SAY)
            GetPlayer()->Say(msg, lang);
        else if (type == CHAT_MSG_EMOTE)
            GetPlayer()->TextEmote(msg);
        else if (type == CHAT_MSG_YELL)
            GetPlayer()->Yell(msg, lang);
    }
    break;

    case CHAT_MSG_WHISPER:
    {
        std::string to, msg;
        recv_data >> to;
        recv_data >> msg;

        if (!processChatmessageFurtherAfterSecurityChecks(msg, lang))
            return;

        if (msg.empty())
            break;

        if (!normalizePlayerName(to))
        {
            SendPlayerNotFoundNotice(to);
            break;
        }

        Player* player = sObjectMgr::Instance()->GetPlayer(to.c_str(), false);
        uint32 tSecurity = GetSecurity();
        uint32 pSecurity =
            player ? player->GetSession()->GetSecurity() : SEC_PLAYER;

        if (!player)
        {
            SendPlayerNotFoundNotice(to);
            return;
        }

        // Players can only whisper GMs that are expediting their ticket.
        if (tSecurity == SEC_PLAYER && pSecurity > SEC_PLAYER)
        {
            if (player->checked_out_ticket == nullptr ||
                player->checked_out_ticket->account_id != GetAccountId())
            {
                SendPlayerNotFoundNotice(to);
                return;
            }

            // GM is expediting the ticket, hand it off to the ticket_mgr
            ticket_mgr::instance().player_whisper(msg, GetPlayer(), player);
        }

        // A GM below Level 3 cannot whisper a non-GM.
        if ((tSecurity == SEC_TICKET_GM || tSecurity == SEC_POWER_GM) &&
            pSecurity == SEC_PLAYER)
        {
            SendPlayerNotFoundNotice(to);
            ChatHandler(this).SendSysMessage("You cannot whisper non-GMs.");
            return;
        }

        if (!sWorld::Instance()->getConfig(
                CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CHAT) &&
            tSecurity == SEC_PLAYER && pSecurity == SEC_PLAYER)
        {
            if (GetPlayer()->GetTeam() != player->GetTeam())
            {
                SendWrongFactionNotice();
                return;
            }
        }

        GetPlayer()->Whisper(msg, lang, player);
    }
    break;

    case CHAT_MSG_PARTY:
    {
        std::string msg;
        recv_data >> msg;

        if (msg.empty())
            break;

        if (ChatHandler(this).ParseCommands(msg.c_str()))
            break;

        if (!processChatmessageFurtherAfterSecurityChecks(msg, lang))
            return;

        if (msg.empty())
            break;

        // if player is in battleground, he cannot say to battleground members
        // by /p
        Group* group = GetPlayer()->GetOriginalGroup();
        if (!group)
        {
            group = _player->GetGroup();
            if (!group || group->isBGGroup())
                return;
        }

        WorldPacket data;
        ChatHandler::FillMessageData(&data, this, type, lang, msg.c_str());
        group->BroadcastPacket(
            &data, false, group->GetMemberGroup(GetPlayer()->GetObjectGuid()));

        break;
    }
    case CHAT_MSG_GUILD:
    {
        std::string msg;
        recv_data >> msg;

        if (msg.empty())
            break;

        if (ChatHandler(this).ParseCommands(msg.c_str()))
            break;

        if (!processChatmessageFurtherAfterSecurityChecks(msg, lang))
            return;

        if (msg.empty())
            break;

        if (GetPlayer()->GetGuildId())
            if (Guild* guild = sGuildMgr::Instance()->GetGuildById(
                    GetPlayer()->GetGuildId()))
                guild->BroadcastToGuild(this, msg,
                    lang == LANG_ADDON ? LANG_ADDON : LANG_UNIVERSAL);

        break;
    }
    case CHAT_MSG_OFFICER:
    {
        std::string msg;
        recv_data >> msg;

        if (msg.empty())
            break;

        if (ChatHandler(this).ParseCommands(msg.c_str()))
            break;

        if (!processChatmessageFurtherAfterSecurityChecks(msg, lang))
            return;

        if (msg.empty())
            break;

        if (GetPlayer()->GetGuildId())
            if (Guild* guild = sGuildMgr::Instance()->GetGuildById(
                    GetPlayer()->GetGuildId()))
                guild->BroadcastToOfficers(this, msg,
                    lang == LANG_ADDON ? LANG_ADDON : LANG_UNIVERSAL);

        break;
    }
    case CHAT_MSG_RAID:
    {
        std::string msg;
        recv_data >> msg;

        if (msg.empty())
            break;

        if (ChatHandler(this).ParseCommands(msg.c_str()))
            break;

        if (!processChatmessageFurtherAfterSecurityChecks(msg, lang))
            return;

        if (msg.empty())
            break;

        // if player is in battleground, he cannot say to battleground members
        // by /ra
        Group* group = GetPlayer()->GetOriginalGroup();
        if (!group)
        {
            group = GetPlayer()->GetGroup();
            if (!group || group->isBGGroup() || !group->isRaidGroup())
                return;
        }

        WorldPacket data;
        ChatHandler::FillMessageData(
            &data, this, CHAT_MSG_RAID, lang, msg.c_str());
        group->BroadcastPacket(&data, false);
    }
    break;
    case CHAT_MSG_RAID_LEADER:
    {
        std::string msg;
        recv_data >> msg;

        if (msg.empty())
            break;

        if (ChatHandler(this).ParseCommands(msg.c_str()))
            break;

        if (!processChatmessageFurtherAfterSecurityChecks(msg, lang))
            return;

        if (msg.empty())
            break;

        // if player is in battleground, he cannot say to battleground members
        // by /ra
        Group* group = GetPlayer()->GetOriginalGroup();
        if (!group)
        {
            group = GetPlayer()->GetGroup();
            if (!group || group->isBGGroup() || !group->isRaidGroup() ||
                !group->IsLeader(_player->GetObjectGuid()))
                return;
        }

        WorldPacket data;
        ChatHandler::FillMessageData(
            &data, this, CHAT_MSG_RAID_LEADER, lang, msg.c_str());
        group->BroadcastPacket(&data, false);
    }
    break;

    case CHAT_MSG_RAID_WARNING:
    {
        std::string msg;
        recv_data >> msg;

        if (!processChatmessageFurtherAfterSecurityChecks(msg, lang))
            return;

        if (msg.empty())
            break;

        Group* group = GetPlayer()->GetGroup();
        if (!group || !group->isRaidGroup() ||
            !(group->IsLeader(GetPlayer()->GetObjectGuid()) ||
                group->IsAssistant(GetPlayer()->GetObjectGuid())))
            return;

        WorldPacket data;
        // in battleground, raid warning is sent only to players in battleground
        // - code is ok
        ChatHandler::FillMessageData(
            &data, this, CHAT_MSG_RAID_WARNING, lang, msg.c_str());
        group->BroadcastPacket(&data, false);
    }
    break;

    case CHAT_MSG_BATTLEGROUND:
    {
        std::string msg;
        recv_data >> msg;

        if (!processChatmessageFurtherAfterSecurityChecks(msg, lang))
            return;

        if (msg.empty())
            break;

        // battleground raid is always in Player->GetGroup(), never in
        // GetOriginalGroup()
        Group* group = GetPlayer()->GetGroup();
        if (!group || !group->isBGGroup())
            return;

        WorldPacket data;
        ChatHandler::FillMessageData(
            &data, this, CHAT_MSG_BATTLEGROUND, lang, msg.c_str());
        group->BroadcastPacket(&data, false);
    }
    break;

    case CHAT_MSG_BATTLEGROUND_LEADER:
    {
        std::string msg;
        recv_data >> msg;

        if (!processChatmessageFurtherAfterSecurityChecks(msg, lang))
            return;

        if (msg.empty())
            break;

        // battleground raid is always in Player->GetGroup(), never in
        // GetOriginalGroup()
        Group* group = GetPlayer()->GetGroup();
        if (!group || !group->isBGGroup() ||
            !group->IsLeader(GetPlayer()->GetObjectGuid()))
            return;

        WorldPacket data;
        ChatHandler::FillMessageData(
            &data, this, CHAT_MSG_BATTLEGROUND_LEADER, lang, msg.c_str());
        group->BroadcastPacket(&data, false);
    }
    break;

    case CHAT_MSG_CHANNEL:
    {
        std::string channel, msg;
        recv_data >> channel;
        recv_data >> msg;

        if (!processChatmessageFurtherAfterSecurityChecks(msg, lang))
            return;

        if (msg.empty())
            break;

        if (ChannelMgr* cMgr = channelMgr(_player->GetTeam()))
            if (Channel* chn = cMgr->GetChannel(channel, _player))
                chn->Say(_player->GetObjectGuid(), msg.c_str(), lang);
    }
    break;

    case CHAT_MSG_AFK:
    {
        std::string msg;
        recv_data >> msg;

        if (!processChatmessageFurtherAfterSecurityChecks(msg, lang))
            return;

        if (_player->isAFK()) // Already AFK
        {
            if (msg.empty())
                _player->ToggleAFK(); // Remove AFK
            else
                _player->autoReplyMsg = msg; // Update message
        }
        else if (!_player->isInCombat()) // New AFK mode
        {
            _player->autoReplyMsg =
                msg.empty() ? GetMangosString(LANG_PLAYER_AFK_DEFAULT) : msg;

            if (_player->isDND())
                _player->ToggleDND();

            _player->ToggleAFK();
        }
        break;
    }
    case CHAT_MSG_DND:
    {
        std::string msg;
        recv_data >> msg;

        if (!processChatmessageFurtherAfterSecurityChecks(msg, lang))
            return;

        if (_player->isDND()) // Already DND
        {
            if (msg.empty())
                _player->ToggleDND(); // Remove DND
            else
                _player->autoReplyMsg = msg; // Update message
        }
        else // New DND mode
        {
            _player->autoReplyMsg =
                msg.empty() ? GetMangosString(LANG_PLAYER_DND_DEFAULT) : msg;

            if (_player->isAFK())
                _player->ToggleAFK();

            _player->ToggleDND();
        }
        break;
    }

    default:
        logging.error("CHAT: unknown message type %u, lang: %u", type, lang);
        break;
    }
}

void WorldSession::HandleEmoteOpcode(WorldPacket& recv_data)
{
    if (!GetPlayer()->isAlive() || GetPlayer()->hasUnitState(UNIT_STAT_DIED))
        return;

    uint32 emote;
    recv_data >> emote;
    GetPlayer()->HandleEmoteCommand(emote);
}

namespace MaNGOS
{
class EmoteChatBuilder
{
public:
    EmoteChatBuilder(Player const& pl, uint32 text_emote, uint32 emote_num,
        Unit const* target)
      : i_player(pl), i_text_emote(text_emote), i_emote_num(emote_num),
        i_target(target)
    {
    }

    void operator()(WorldPacket& data, int32 loc_idx)
    {
        char const* nam =
            i_target ? i_target->GetNameForLocaleIdx(loc_idx) : nullptr;
        uint32 namlen = (nam ? strlen(nam) : 0) + 1;

        data.initialize(SMSG_TEXT_EMOTE, (20 + namlen));
        data << ObjectGuid(i_player.GetObjectGuid());
        data << uint32(i_text_emote);
        data << uint32(i_emote_num);
        data << uint32(namlen);
        if (namlen > 1)
            data.append(nam, namlen);
        else
            data << uint8(0x00);
    }

private:
    Player const& i_player;
    uint32 i_text_emote;
    uint32 i_emote_num;
    Unit const* i_target;
};
} // namespace MaNGOS

void WorldSession::HandleTextEmoteOpcode(WorldPacket& recv_data)
{
    if (!GetPlayer()->isAlive())
        return;

    if (!GetPlayer()->CanSpeak())
    {
        std::string timeStr =
            secsToTimeString(m_muteTime - WorldTimer::time_no_syscall());
        SendNotification(
            GetMangosString(LANG_WAIT_BEFORE_SPEAKING), timeStr.c_str());
        return;
    }

    uint32 text_emote, emoteNum;
    ObjectGuid guid;

    recv_data >> text_emote;
    recv_data >> emoteNum;
    recv_data >> guid;

    EmotesTextEntry const* em = sEmotesTextStore.LookupEntry(text_emote);
    if (!em)
        return;

    uint32 emote_id = em->textid;

    switch (emote_id)
    {
    case EMOTE_STATE_SLEEP:
    case EMOTE_STATE_SIT:
    case EMOTE_STATE_KNEEL:
    case EMOTE_ONESHOT_NONE:
        break;
    default:
    {
        // in feign death state allowed only text emotes.
        if (GetPlayer()->hasUnitState(UNIT_STAT_DIED))
            break;

        GetPlayer()->HandleEmoteCommand(emote_id);
        break;
    }
    }

    Unit* unit = GetPlayer()->GetMap()->GetUnit(guid);

    MaNGOS::EmoteChatBuilder emote_builder(
        *GetPlayer(), text_emote, emoteNum, unit);
    auto emote_do = maps::callbacks::make_localize_packet(emote_builder);
    maps::visitors::camera_owner{}(GetPlayer(),
        sWorld::Instance()->getConfig(CONFIG_FLOAT_LISTEN_RANGE_TEXTEMOTE),
        emote_do);

    // Send scripted event call
    if (unit && unit->GetTypeId() == TYPEID_UNIT && ((Creature*)unit)->AI())
        ((Creature*)unit)->AI()->ReceiveEmote(GetPlayer(), text_emote);
}

void WorldSession::HandleChatIgnoredOpcode(WorldPacket& recv_data)
{
    ObjectGuid iguid;
    uint8 unk;

    recv_data >> iguid;
    recv_data >> unk; // probably related to spam reporting

    Player* player = sObjectMgr::Instance()->GetPlayer(iguid);
    if (!player || !player->GetSession())
        return;

    WorldPacket data;
    ChatHandler::FillMessageData(&data, this, CHAT_MSG_IGNORED, LANG_UNIVERSAL,
        nullptr, GetPlayer()->GetObjectGuid(), GetPlayer()->GetName(), nullptr);
    player->GetSession()->send_packet(std::move(data));
}

void WorldSession::SendPlayerNotFoundNotice(std::string name)
{
    WorldPacket data(SMSG_CHAT_PLAYER_NOT_FOUND, name.size() + 1);
    data << name;
    send_packet(std::move(data));
}

void WorldSession::SendWrongFactionNotice()
{
    WorldPacket data(SMSG_CHAT_WRONG_FACTION, 0);
    send_packet(std::move(data));
}

void WorldSession::SendChatRestrictedNotice(ChatRestrictionType restriction)
{
    WorldPacket data(SMSG_CHAT_RESTRICTED, 1);
    data << uint8(restriction);
    send_packet(std::move(data));
}
