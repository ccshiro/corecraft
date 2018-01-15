#include "lfg_tool_container.h"
#include "ChannelMgr.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "World.h"
#include "WorldSession.h"

void lfg_tool_container::insert(WorldSession* session)
{
    if (std::find(users_.begin(), users_.end(), session) != users_.end())
        return;
    users_.push_back(session);

    // attempt adding to LookingForGroup channel
    if (Player* plr = session->GetPlayer())
        plr->JoinLFGChannel();
}

void lfg_tool_container::remove(WorldSession* session)
{
    if (Player* player = session->GetPlayer())
    {
        if (sWorld::Instance()->getConfig(CONFIG_BOOL_RESTRICTED_LFG_CHANNEL) &&
            session->GetSecurity() == SEC_PLAYER)
            player->LeaveLFGChannel();

        player->m_lookingForGroup.more.Clear();
        for (int i = 0; i < MAX_LOOKING_FOR_GROUP_SLOT; ++i)
            player->m_lookingForGroup.slots[i].Clear();
    }

    users_.erase(
        std::remove(users_.begin(), users_.end(), session), users_.end());
}

void lfg_tool_container::attempt_join(Player* player)
{
    // don't attempt join if the player is not queued
    if (std::find(users_.begin(), users_.end(), player->GetSession()) ==
        users_.end())
        return;

    // cannot join if not queued for an auto-join type, or is already in a group
    if (!player->m_lookingForGroup.canAutoJoin() || player->GetGroup())
        return;

    std::vector<WorldSession*> remove_sessions;

    for (WorldSession* session : users_)
    {
        Player* other = session->GetPlayer();
        if (!other || other == player || !other->IsInWorld() ||
            other->GetTeam() != player->GetTeam())
            continue;

        // need to have auto-invite on, and to be in an auto-joinable type
        if (!session->lfg_auto_invite ||
            !other->m_lookingForGroup.more.canAutoJoin())
            continue;

        // need to have a type we're searching for
        if (!player->m_lookingForGroup.HaveInSlot(
                other->m_lookingForGroup.more))
            continue;

        // needs to be leader of his group, or group-less
        if (other->GetGroup() &&
            !other->GetGroup()->IsLeader(other->GetObjectGuid()))
            continue;

        // We've found a potential match

        // Try creating a group if one doesn't exist
        if (!other->GetGroup())
        {
            auto group = new Group;
            if (!group->Create(other->GetObjectGuid(), other->GetName()))
            {
                delete group;
                continue;
            }

            sObjectMgr::Instance()->AddGroup(group);
        }

        if (other->GetGroup()->AddMember(
                player->GetObjectGuid(), player->GetName()))
        {
            remove_sessions.push_back(player->GetSession());
            break; // we found a group
        }
        else
        {
            // group was full, remove session after attempted join is completed
            remove_sessions.push_back(session);
        }
    }

    // remove any session from the LFG tool that had a full group
    for (auto session : remove_sessions)
        remove(session);
}

void lfg_tool_container::attempt_invite(Player* player)
{
    // don't attempt invite if the player is not in the tool
    if (std::find(users_.begin(), users_.end(), player->GetSession()) ==
        users_.end())
        return;

    // must be leader or group-less to invite people
    if (player->GetGroup() &&
        player->GetGroup()->GetLeaderGuid() != player->GetObjectGuid())
        return;

    // must be LFM with auto-invite for an auto-joinable type
    if (!player->GetSession()->lfg_auto_invite ||
        !player->m_lookingForGroup.more.canAutoJoin())
        return;

    // cannot invite if group is full (leave LFM)
    if (player->GetGroup() && player->GetGroup()->IsFull())
    {
        remove(player->GetSession());
        return;
    }

    std::vector<WorldSession*> removed_sessions;

    for (WorldSession* session : users_)
    {
        Player* other = session->GetPlayer();
        if (!other || other == player || !other->IsInWorld() ||
            other->GetTeam() != player->GetTeam())
            continue;

        // skip people that don't have auto-join on or are in a group
        if (!session->lfg_auto_join || other->GetGroup())
            continue;

        // skip people not queued for the same type
        if (!other->m_lookingForGroup.HaveInSlot(
                player->m_lookingForGroup.more))
            continue;

        // Try creating a group if one doesn't exist
        if (!player->GetGroup())
        {
            auto group = new Group;
            if (!group->Create(player->GetObjectGuid(), player->GetName()))
            {
                delete group;
                continue;
            }

            sObjectMgr::Instance()->AddGroup(group);
        }

        if (player->GetGroup()->AddMember(
                other->GetObjectGuid(), other->GetName()))
            removed_sessions.push_back(
                session); // remove from queue, he got invited

        if (player->GetGroup()->IsFull())
        {
            removed_sessions.push_back(player->GetSession());
            break; // stop searching, group is full
        }
    }

    for (auto session : removed_sessions)
        remove(session);
}

void lfg_tool_container::send_tool_state(
    Player* player, uint32 entry, uint32 type)
{
    uint32 count = 0;

    WorldPacket data(MSG_LOOKING_FOR_GROUP);
    data << uint32(type);  // type
    data << uint32(entry); // entry from LFGDungeons.dbc
    size_t count_pos1 = data.wpos();
    data << uint32(0); // count, placeholder
    size_t count_pos2 = data.wpos();
    data << uint32(0); // count again, strange, placeholder

    std::vector<WorldSession*> removed_sessions;

    for (WorldSession* session : users_)
    {
        Player* other = session->GetPlayer();

        if (!other || !other->IsInWorld() ||
            other->GetTeam() != player->GetTeam())
            continue;

        if (!other->m_lookingForGroup.HaveInSlot(entry, type) &&
            !other->m_lookingForGroup.more.Is(entry, type))
            continue;

        LfgMode mode;
        if (other->m_lookingForGroup.HaveInSlot(entry, type))
            mode = LFG_MODE;
        else
            mode = LFM_MODE;

        // cannot be LFG if already in group (XXX: Is this case even possible?)
        if (mode == LFG_MODE && other->GetGroup())
        {
            removed_sessions.push_back(session);
            continue;
        }

        ++count;

        data << other->GetPackGUID();       // packed guid
        data << uint32(other->getLevel());  // level
        data << uint32(other->GetZoneId()); // current zone
        data << uint8(mode);                // 0x00 - LFG, 0x01 - LFM

        // packed entry, type pair
        if (mode == LFG_MODE)
        {
            for (int i = 0; i < MAX_LOOKING_FOR_GROUP_SLOT; ++i)
                data << uint32(other->m_lookingForGroup.slots[i].entry |
                               (other->m_lookingForGroup.slots[i].type << 24));
        }
        else
        {
            data << uint32(other->m_lookingForGroup.more.entry |
                           (other->m_lookingForGroup.more.type << 24));
            data << uint32(0);
            data << uint32(0);
        }

        data << other->m_lookingForGroup.comment; // comment

        if (Group* group = other->GetGroup())
        {
            data << uint32(group->GetMembersCount() -
                           1); // count of group members without group leader
            for (auto member : group->members(false))
            {
                if (member->GetObjectGuid() != other->GetObjectGuid())
                {
                    data << member->GetPackGUID();      // packed guid
                    data << uint32(member->getLevel()); // player level
                }
            }
        }
        else
        {
            data << uint32(0);
        }
    }

    // fill count placeholders
    data.put<uint32>(count_pos1, count);
    data.put<uint32>(count_pos2, count);

    player->GetSession()->send_packet(std::move(data));

    for (WorldSession* session : removed_sessions)
        remove(session);
}

void lfg_tool_container::group_leader_switch(
    WorldSession* prev, WorldSession* now)
{
    if (std::find(users_.begin(), users_.end(), prev) == users_.end())
        return;

    // Copy LFG data from prev to now
    Player* prev_player = prev->GetPlayer();
    Player* now_player = now->GetPlayer();
    if (prev_player && now_player)
        now_player->m_lookingForGroup = prev_player->m_lookingForGroup;

    // remove prev and insert now
    remove(prev);

    insert(now);
}

bool lfg_tool_container::in_tool(WorldSession* session) const
{
    return std::find(users_.begin(), users_.end(), session) != users_.end();
}
