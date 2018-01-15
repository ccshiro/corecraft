/*
 * Copyright (C) 2008-2012 TrinityCore <http://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TRINITY_CREATURE_TEXT_MGR_H
#define TRINITY_CREATURE_TEXT_MGR_H

#include "Creature.h"
#include "ObjectAccessor.h"
#include "SharedDefines.h"
#include "SpecialVisCreature.h"
#include "World.h"
#include "maps/visitors.h"
#include "Policies/Singleton.h"
#include <unordered_map>

enum TextRange
{
    TEXT_RANGE_NORMAL = 0,
    TEXT_RANGE_AREA = 1,
    TEXT_RANGE_ZONE = 2,
    TEXT_RANGE_MAP = 3,
    TEXT_RANGE_WORLD = 4
};

struct CreatureTextEntry
{
    uint32 entry;
    uint8 group;
    uint8 id;
    std::string text;
    ChatMsg type;
    Language lang;
    float probability;
    Emote emote;
    uint32 duration;
    uint32 sound;
    TextRange text_range;
};

struct CreatureTextLocale
{
    std::vector<std::string> Text;
};

struct CreatureTextId
{
    CreatureTextId(uint32 e, uint32 g, uint32 i)
      : entry(e), textGroup(g), textId(i)
    {
    }

    bool operator<(CreatureTextId const& right) const
    {
        return memcmp(this, &right, sizeof(CreatureTextId)) < 0;
    }

    uint32 entry;
    uint32 textGroup;
    uint32 textId;
};

typedef std::vector<CreatureTextEntry> CreatureTextGroup; // texts in a group
typedef std::unordered_map<uint8, CreatureTextGroup>
    CreatureTextHolder; // groups for a creature by groupid
typedef std::unordered_map<uint32, CreatureTextHolder>
    CreatureTextMap; // all creatures by entry

typedef std::map<CreatureTextId, CreatureTextLocale> LocaleCreatureTextMap;

// used for handling non-repeatable random texts
typedef std::vector<uint8> CreatureTextRepeatIds;
typedef std::unordered_map<uint8, CreatureTextRepeatIds>
    CreatureTextRepeatGroup;
typedef std::unordered_map<uint64, CreatureTextRepeatGroup>
    CreatureTextRepeatMap; // guid based

class CreatureTextMgr
{
    friend class MaNGOS::UnlockedSingleton<CreatureTextMgr>; // Private
                                                             // constructor
    CreatureTextMgr(){};

public:
    ~CreatureTextMgr(){};
    void LoadCreatureTexts();
    void LoadCreatureTextLocales();
    CreatureTextMap const& GetTextMap() const { return mTextMap; }

    void SendSound(Creature* source, uint32 sound, ChatMsg msgType,
        uint64 whisperGuid, TextRange range, Team team, bool gmOnly);
    void SendEmote(Unit* source, uint32 emote);

    // if sent, returns the 'duration' of the text else 0 if error
    uint32 SendChat(Creature* source, uint8 textGroup, uint64 whisperGuid = 0,
        const char* whisperName = nullptr, ChatMsg msgType = CHAT_MSG_ADDON,
        Language language = LANG_ADDON, uint32 sound = 0, Team team = TEAM_NONE,
        bool gmOnly = false, Player* srcPlr = nullptr);
    bool TextExist(uint32 sourceEntry, uint8 textGroup);
    std::string GetLocalizedChatString(
        uint32 entry, uint8 textGroup, uint32 id, LocaleConstant locale) const;

    template <class Builder>
    void SendChatPacket(WorldObject* source, Builder const& builder,
        ChatMsg msgType, uint64 whisperGuid = 0,
        TextRange range = TEXT_RANGE_NORMAL, Team team = TEAM_NONE,
        bool gmOnly = false) const;

private:
    CreatureTextRepeatIds GetRepeatGroup(Creature* source, uint8 textGroup);
    void SetRepeatId(Creature* source, uint8 textGroup, uint8 id);

    void SendNonChatPacket(WorldObject* source, WorldPacket* data,
        ChatMsg msgType, uint64 whisperGuid, TextRange range, Team team,
        bool gmOnly) const;
    float GetRangeForChatType(ChatMsg msgType) const;

    CreatureTextMap mTextMap;
    CreatureTextRepeatMap mTextRepeatMap;
    LocaleCreatureTextMap mLocaleTextMap;
};

template <class Builder>
class CreatureTextLocalizer
{
public:
    CreatureTextLocalizer(Builder const& builder, ChatMsg msgType)
      : _builder(builder), _msgType(msgType)
    {
        _packetCache.resize(MAX_LOCALE, nullptr);
    }

    ~CreatureTextLocalizer()
    {
        for (auto& elem : _packetCache)
        {
            if (elem)
                delete elem->first;
            delete elem;
        }
    }

    void operator()(Player* player)
    {
        LocaleConstant loc_idx =
            (LocaleConstant)player->GetSession()->GetSessionDbLocaleIndex();
        if (loc_idx < (LocaleConstant)0 || loc_idx >= MAX_LOCALE)
            loc_idx = DEFAULT_LOCALE;

        WorldPacket* messageTemplate;
        size_t whisperGUIDpos;

        // create if not cached yet
        if (!_packetCache[loc_idx])
        {
            messageTemplate = new WorldPacket(SMSG_MESSAGECHAT, 200);
            whisperGUIDpos = _builder(messageTemplate, loc_idx);
            _packetCache[loc_idx] = new std::pair<WorldPacket*, size_t>(
                messageTemplate, whisperGUIDpos);
        }
        else
        {
            messageTemplate = _packetCache[loc_idx]->first;
            whisperGUIDpos = _packetCache[loc_idx]->second;
        }

        WorldPacket data(*messageTemplate);
        switch (_msgType)
        {
        case CHAT_MSG_MONSTER_WHISPER:
        case CHAT_MSG_RAID_BOSS_WHISPER:
            data.put<uint64>(whisperGUIDpos, player->GetObjectGuid());
            break;
        default:
            break;
        }

        player->SendDirectMessage(std::move(data));
    }

private:
    std::vector<std::pair<WorldPacket*, size_t>*> _packetCache;
    Builder const& _builder;
    ChatMsg _msgType;
};

template <class Builder>
void CreatureTextMgr::SendChatPacket(WorldObject* source,
    Builder const& builder, ChatMsg msgType, uint64 whisperGuid,
    TextRange range, Team team, bool gmOnly) const
{
    if (!source)
        return;

    CreatureTextLocalizer<Builder> localizer(builder, msgType);

    switch (msgType)
    {
    case CHAT_MSG_MONSTER_WHISPER:
    case CHAT_MSG_RAID_BOSS_WHISPER:
    {
        if (range == TEXT_RANGE_NORMAL) // ignores team and gmOnly
        {
            Player* player =
                ObjectAccessor::FindPlayer(ObjectGuid(whisperGuid));
            if (!player || !player->GetSession())
                return;

            localizer(player);
            return;
        }
        break;
    }
    default:
        break;
    }

    switch (range)
    {
    case TEXT_RANGE_AREA:
    {
        uint32 areaId = source->GetAreaId();
        Map::PlayerList const& players = source->GetMap()->GetPlayers();
        for (const auto& player : players)
            if (player.getSource()->GetAreaId() == areaId &&
                (!team || Team(player.getSource()->GetTeam()) == team) &&
                (!gmOnly || player.getSource()->isGameMaster()))
                localizer(player.getSource());
        return;
    }
    case TEXT_RANGE_ZONE:
    {
        uint32 zoneId = source->GetZoneId();
        Map::PlayerList const& players = source->GetMap()->GetPlayers();
        for (const auto& player : players)
            if (player.getSource()->GetZoneId() == zoneId &&
                (!team || Team(player.getSource()->GetTeam()) == team) &&
                (!gmOnly || player.getSource()->isGameMaster()))
                localizer(player.getSource());
        return;
    }
    case TEXT_RANGE_MAP:
    {
        Map::PlayerList const& players = source->GetMap()->GetPlayers();
        for (const auto& player : players)
            if ((!team || Team(player.getSource()->GetTeam()) == team) &&
                (!gmOnly || player.getSource()->isGameMaster()))
                localizer(player.getSource());
        return;
    }
    case TEXT_RANGE_WORLD:
    {
        const auto& smap = sWorld::Instance()->GetAllSessions();
        for (const auto& elem : smap)
            if (Player* player = elem.second->GetPlayer())
                if (player->GetSession() &&
                    (!team || Team(player->GetTeam()) == team) &&
                    (!gmOnly || player->isGameMaster()))
                    localizer(player);
        return;
    }
    case TEXT_RANGE_NORMAL:
    default:
        break;
    }

    float dist = GetRangeForChatType(msgType);
    maps::visitors::simple<Player>{}(source, dist, localizer);
}

#define sCreatureTextMgr MaNGOS::UnlockedSingleton<CreatureTextMgr>

#endif
