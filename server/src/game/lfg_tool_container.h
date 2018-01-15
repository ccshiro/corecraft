#ifndef GAME__LFG_TOOL_CONTAINER_H
#define GAME__LFG_TOOL_CONTAINER_H

#include "ObjectGuid.h"
#include "SharedDefines.h"
#include "Policies/Singleton.h"
#include <vector>

class Player;
class WorldSession;

enum LfgMode
{
    LFG_MODE = 0,
    LFM_MODE = 1,
};

class lfg_tool_container
{
public:
    void insert(WorldSession* session);
    void remove(WorldSession* session);

    void attempt_join(Player* player);
    void attempt_invite(Player* player);

    void send_tool_state(Player* player, uint32 entry, uint32 type);

    void group_leader_switch(WorldSession* prev, WorldSession* now);

    bool in_tool(WorldSession* session) const;

private:
    std::vector<WorldSession*>
        users_; // users currently registered with the tool,
                // session does not have to have a valid player attached
};

#define sLfgToolContainer MaNGOS::Singleton<lfg_tool_container>

#endif
