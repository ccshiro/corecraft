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

#include "CreatureTextMgr.h"
#include "Common.h"
#include "ObjectMgr.h"
#include "ProgressBar.h"
#include "World.h"
#include "Database/DatabaseEnv.h"

class CreatureTextBuilder
{
public:
    CreatureTextBuilder(WorldObject* obj, ChatMsg msgtype, uint8 textGroup,
        uint32 id, uint32 language, uint64 targetGUID, const char* targetName)
      : _source(obj), _msgType(msgtype), _textGroup(textGroup), _textId(id),
        _language(language), _targetGUID(targetGUID), _targetName(targetName)
    {
    }

    size_t operator()(WorldPacket* data, LocaleConstant locale) const
    {
        std::string text = sCreatureTextMgr::Instance()->GetLocalizedChatString(
            _source->GetEntry(), _textGroup, _textId, locale);
        char const* localizedName = _source->GetNameForLocaleIdx(locale);

        *data << uint8(_msgType);
        *data << uint32(_language);
        *data << uint64(_source->GetObjectGuid().GetRawValue());
        *data << uint32(1); // 2.1.0
        *data << uint32(strlen(localizedName) + 1);
        *data << localizedName;
        size_t whisperGUIDpos = data->wpos();
        *data << uint64(_targetGUID); // Unit Target
        if (_targetGUID && !ObjectGuid(_targetGUID).IsPlayer())
        {
            *data << uint32(strlen(_targetName) + 1); // target name length
            *data << _targetName;                     // target name
        }
        *data << uint32(text.length() + 1);
        *data << text;
        *data << uint8(0); // ChatTag

        return whisperGUIDpos;
    }

    WorldObject* _source;
    ChatMsg _msgType;
    uint8 _textGroup;
    uint32 _textId;
    uint32 _language;
    uint64 _targetGUID;
    const char* _targetName;
};

void CreatureTextMgr::LoadCreatureTexts()
{
    mTextMap.clear();       // for reload case
    mTextRepeatMap.clear(); // reset all currently used temp texts

    std::unique_ptr<QueryResult> result(WorldDatabase.PQuery(
        "SELECT entry, groupid, id, text, type, language, probability, emote, "
        "duration, sound, text_range FROM creature_text"));
    if (!result)
    {
        logging.info(
            "Loaded 0 ceature texts. DB table `creature_texts` is empty.\n");
        return;
    }

    uint32 textCount = 0;
    uint32 creatureCount = 0;
    BarGoLink bar(result->GetRowCount());

    do
    {
        bar.step();
        Field* fields = result->Fetch();
        CreatureTextEntry temp;

        temp.entry = fields[0].GetUInt32();
        temp.group = fields[1].GetUInt8();
        temp.id = fields[2].GetUInt8();
        temp.text = fields[3].GetCppString();
        temp.type = ChatMsg(fields[4].GetUInt8());
        temp.lang = Language(fields[5].GetUInt8());
        temp.probability = fields[6].GetFloat();
        temp.emote = Emote(fields[7].GetUInt32());
        temp.duration = fields[8].GetUInt32();
        temp.sound = fields[9].GetUInt32();
        temp.text_range = (TextRange)fields[10].GetUInt32();

        if (temp.sound)
        {
            if (!sSoundEntriesStore.LookupEntry(temp.sound))
            {
                logging.error(
                    "CreatureTextMgr:  Entry %u, Group %u in table "
                    "`creature_texts` has Sound %u but sound does not exist.",
                    temp.entry, temp.group, temp.sound);
                temp.sound = 0;
            }
        }
        if (!GetLanguageDescByID(temp.lang))
        {
            logging.error(
                "CreatureTextMgr:  Entry %u, Group %u in table "
                "`creature_texts` using Language %u but Language does not "
                "exist.",
                temp.entry, temp.group, uint32(temp.lang));
            temp.lang = LANG_UNIVERSAL;
        }
        if (temp.type >= MAX_CHAT_MSG_TYPE)
        {
            logging.error(
                "CreatureTextMgr:  Entry %u, Group %u in table "
                "`creature_texts` has Type %u but this Chat Type does not "
                "exist.",
                temp.entry, temp.group, uint32(temp.type));
            temp.type = CHAT_MSG_SAY;
        }
        if (temp.emote)
        {
            if (!sEmotesStore.LookupEntry(temp.emote))
            {
                logging.error(
                    "CreatureTextMgr:  Entry %u, Group %u in table "
                    "`creature_texts` has Emote %u but emote does not exist.",
                    temp.entry, temp.group, uint32(temp.emote));
                temp.emote = EMOTE_ONESHOT_NONE;
            }
        }
        if (temp.text_range > TEXT_RANGE_WORLD)
        {
            logging.error(
                "CreatureTextMgr:  Entry %u, Group %u in table "
                "`creature_texts` has text_range out of range (%u).",
                temp.entry, temp.group, temp.text_range);
            temp.text_range = TEXT_RANGE_NORMAL;
        }

        // entry not yet added, add empty TextHolder (list of groups)
        if (mTextMap.find(temp.entry) == mTextMap.end())
            ++creatureCount;

        // add the text into our entry's group
        mTextMap[temp.entry][temp.group].push_back(temp);

        ++textCount;
    } while (result->NextRow());

    logging.info("Loaded %u creature texts for %u creatures\n", textCount,
        creatureCount);
}

void CreatureTextMgr::LoadCreatureTextLocales()
{
    mLocaleTextMap.clear(); // for reload case

    std::unique_ptr<QueryResult> result(WorldDatabase.PQuery(
        "SELECT entry, textGroup, id, text_loc1, text_loc2, text_loc3, "
        "text_loc4, text_loc5, text_loc6, text_loc7, text_loc8 FROM "
        "locales_creature_text"));
    if (!result)
        return;

    uint32 textCount = 0;
    BarGoLink bar(result->GetRowCount());

    do
    {
        bar.step();
        Field* fields = result->Fetch();
        CreatureTextLocale& loc =
            mLocaleTextMap[CreatureTextId(fields[0].GetUInt32(),
                uint32(fields[1].GetUInt8()), fields[2].GetUInt32())];
        for (uint8 i = 1; i < MAX_LOCALE; ++i)
        {
            LocaleConstant locale = LocaleConstant(i);
            sObjectMgr::Instance()->AddLocaleString(
                fields[3 + i - 1].GetString(), locale, loc.Text);
        }

        ++textCount;
    } while (result->NextRow());

    logging.info("Loaded %u creature localized texts\n", textCount);
}

uint32 CreatureTextMgr::SendChat(Creature* source, uint8 textGroup,
    uint64 whisperGuid /*= 0*/, const char* whisperName /*= nullptr*/,
    ChatMsg msgType /*= CHAT_MSG_ADDON*/, Language language /*= LANG_ADDON*/,
    uint32 sound /*= 0*/, Team team /*= TEAM_OTHER*/, bool gmOnly /*= false*/,
    Player* srcPlr /*= NULL*/)
{
    if (!source)
        return 0;

    CreatureTextMap::const_iterator sList = mTextMap.find(source->GetEntry());
    if (sList == mTextMap.end())
    {
        logging.error(
            "CreatureTextMgr: Could not find Text for Creature(%s) Entry %u in "
            "'creature_text' table. Ignoring.",
            source->GetName(), source->GetEntry());
        return 0;
    }

    CreatureTextHolder const& textHolder = sList->second;
    auto itr = textHolder.find(textGroup);
    if (itr == textHolder.end())
    {
        logging.error(
            "CreatureTextMgr: Could not find TextGroup %u for Creature(%s) "
            "GuidLow %u Entry %u. Ignoring.",
            uint32(textGroup), source->GetName(), source->GetGUIDLow(),
            source->GetEntry());
        return 0;
    }

    CreatureTextGroup const& textGroupContainer =
        itr->second; // has all texts in the group
    CreatureTextRepeatIds repeatGroup = GetRepeatGroup(source,
        textGroup); // has all textIDs from the group that were already said
    CreatureTextGroup
        tempGroup; // will use this to talk after sorting repeatGroup

    for (const auto& elem : textGroupContainer)
        if (std::find(repeatGroup.begin(), repeatGroup.end(), elem.id) ==
            repeatGroup.end())
            tempGroup.push_back(elem);

    if (tempGroup.empty())
    {
        auto mapItr = mTextRepeatMap.find(source->GetObjectGuid());
        if (mapItr != mTextRepeatMap.end())
        {
            auto groupItr = mapItr->second.find(textGroup);
            groupItr->second.clear();
        }

        tempGroup = textGroupContainer;
    }

    uint8 count = 0;
    float lastChance = -1;
    bool isEqualChanced = true;

    float totalChance = 0;

    for (CreatureTextGroup::const_iterator iter = tempGroup.begin();
         iter != tempGroup.end(); ++iter)
    {
        if (lastChance >= 0 && lastChance != iter->probability)
            isEqualChanced = false;

        lastChance = iter->probability;
        totalChance += iter->probability;
        ++count;
    }

    int32 offset = -1;
    if (!isEqualChanced)
    {
        for (CreatureTextGroup::const_iterator iter = tempGroup.begin();
             iter != tempGroup.end(); ++iter)
        {
            uint32 chance = uint32(iter->probability);
            uint32 r = urand(0, 100);
            ++offset;
            if (r <= chance)
                break;
        }
    }

    uint32 pos = 0;
    if (isEqualChanced || offset < 0)
        pos = urand(0, count - 1);
    else if (offset >= 0)
        pos = offset;

    CreatureTextGroup::const_iterator iter = tempGroup.begin() + pos;

    ChatMsg finalType = (msgType == CHAT_MSG_ADDON) ? iter->type : msgType;
    Language finalLang = (language == LANG_ADDON) ? iter->lang : language;
    uint32 finalSound = sound ? sound : iter->sound;
    TextRange range = iter->text_range;

    if (finalSound)
        SendSound(
            source, finalSound, finalType, whisperGuid, range, team, gmOnly);

    Unit* finalSource = source;
    if (srcPlr)
        finalSource = srcPlr;

    if (iter->emote)
        SendEmote(finalSource, iter->emote);

    if (!iter->text.empty())
    {
        CreatureTextBuilder builder(finalSource, finalType, iter->group,
            iter->id, finalLang, whisperGuid, whisperName);
        SendChatPacket(
            finalSource, builder, finalType, whisperGuid, range, team, gmOnly);
    }
    if (isEqualChanced || (!isEqualChanced && totalChance == 100.0f))
        SetRepeatId(source, textGroup, iter->id);

    return iter->duration;
}

float CreatureTextMgr::GetRangeForChatType(ChatMsg msgType) const
{
    float dist = sWorld::Instance()->getConfig(CONFIG_FLOAT_LISTEN_RANGE_SAY);
    switch (msgType)
    {
    case CHAT_MSG_MONSTER_YELL:
        dist = sWorld::Instance()->getConfig(CONFIG_FLOAT_LISTEN_RANGE_YELL);
        break;
    case CHAT_MSG_MONSTER_EMOTE:
    case CHAT_MSG_RAID_BOSS_EMOTE:
        dist =
            sWorld::Instance()->getConfig(CONFIG_FLOAT_LISTEN_RANGE_TEXTEMOTE);
        break;
    default:
        break;
    }

    return dist;
}

void CreatureTextMgr::SendSound(Creature* source, uint32 sound, ChatMsg msgType,
    uint64 whisperGuid, TextRange range, Team team, bool gmOnly)
{
    if (!sound || !source)
        return;

    WorldPacket data(SMSG_PLAY_SOUND, 4);
    data << uint32(sound);
    SendNonChatPacket(source, &data, msgType, whisperGuid, range, team, gmOnly);
}

void CreatureTextMgr::SendNonChatPacket(WorldObject* source, WorldPacket* data,
    ChatMsg msgType, uint64 whisperGuid, TextRange range, Team team,
    bool gmOnly) const
{
    float dist = GetRangeForChatType(msgType);

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
            player->GetSession()->send_packet(data);
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
                player.getSource()->GetSession()->send_packet(data);
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
                player.getSource()->GetSession()->send_packet(data);
        return;
    }
    case TEXT_RANGE_MAP:
    {
        Map::PlayerList const& players = source->GetMap()->GetPlayers();
        for (const auto& player : players)
            if ((!team || Team(player.getSource()->GetTeam()) == team) &&
                (!gmOnly || player.getSource()->isGameMaster()))
                player.getSource()->GetSession()->send_packet(data);
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
                    player->GetSession()->send_packet(data);
        return;
    }
    case TEXT_RANGE_NORMAL:
    default:
        break;
    }

    source->SendMessageToSetInRange(data, dist, true);
}

void CreatureTextMgr::SendEmote(Unit* source, uint32 emote)
{
    if (!source)
        return;

    source->HandleEmoteCommand(emote);
}

void CreatureTextMgr::SetRepeatId(Creature* source, uint8 textGroup, uint8 id)
{
    if (!source)
        return;

    CreatureTextRepeatIds& repeats =
        mTextRepeatMap[source->GetObjectGuid()][textGroup];
    if (std::find(repeats.begin(), repeats.end(), id) == repeats.end())
        repeats.push_back(id);
    else
        logging.error(
            "CreatureTextMgr: TextGroup %u for Creature(%s) GuidLow %u Entry "
            "%u, id %u already added",
            uint32(textGroup), source->GetName(), source->GetGUIDLow(),
            source->GetEntry(), uint32(id));
}

CreatureTextRepeatIds CreatureTextMgr::GetRepeatGroup(
    Creature* source, uint8 textGroup)
{
    assert(source); // should never happen
    CreatureTextRepeatIds ids;

    CreatureTextRepeatMap::const_iterator mapItr =
        mTextRepeatMap.find(source->GetObjectGuid());
    if (mapItr != mTextRepeatMap.end())
    {
        auto groupItr = (*mapItr).second.find(textGroup);
        if (groupItr != mapItr->second.end())
            ids = groupItr->second;
    }
    return ids;
}

bool CreatureTextMgr::TextExist(uint32 sourceEntry, uint8 textGroup)
{
    if (!sourceEntry)
        return false;

    CreatureTextMap::const_iterator sList = mTextMap.find(sourceEntry);
    if (sList == mTextMap.end())
    {
        LOG_DEBUG(logging,
            "CreatureTextMgr::TextExist: Could not find Text for Creature "
            "(entry %u) in 'creature_text' table.",
            sourceEntry);
        return false;
    }

    CreatureTextHolder const& textHolder = sList->second;
    auto itr = textHolder.find(textGroup);
    if (itr == textHolder.end())
    {
        LOG_DEBUG(logging,
            "CreatureTextMgr::TextExist: Could not find TextGroup %u for "
            "Creature (entry %u).",
            uint32(textGroup), sourceEntry);
        return false;
    }

    return true;
}

std::string CreatureTextMgr::GetLocalizedChatString(
    uint32 entry, uint8 textGroup, uint32 id, LocaleConstant locale) const
{
    auto mapitr = mTextMap.find(entry);
    if (mapitr == mTextMap.end())
        return "";

    auto holderItr = mapitr->second.find(textGroup);
    if (holderItr == mapitr->second.end())
        return "";

    auto groupItr = holderItr->second.begin();
    for (; groupItr != holderItr->second.end(); ++groupItr)
        if (groupItr->id == id)
            break;

    if (groupItr == holderItr->second.end())
        return "";

    std::string baseText = groupItr->text;
    if (locale == DEFAULT_LOCALE)
        return baseText;

    if (locale > MAX_LOCALE)
        return baseText;

    auto locItr =
        mLocaleTextMap.find(CreatureTextId(entry, uint32(textGroup), id));
    if (locItr == mLocaleTextMap.end())
        return baseText;

    sObjectMgr::Instance()->GetLocaleString(
        locItr->second.Text, locale, baseText);

    return baseText;
}
