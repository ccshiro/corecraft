#ifndef GAME__ACTION_LIMIT_H
#define GAME__ACTION_LIMIT_H

#include "Common.h"
#include "Platform/Define.h"
#include "Policies/Singleton.h"
#include <string>

class WorldSession;

// action_limit: allows you to put global limits, local (session based) limits
// and ip limits to certain actions. actions are defined by a string of your
// choosing

class action_limit
{
public:
    enum type
    {
        local = 0x1,
        global = 0x2,
        ip = 0x4
    };

    // returns true if player can perform action,
    // limit:    how many times this action can be performed per minute
    // cooldown: flat cooldown in seconds after performing the action once,
    // cannot exceed a minute
    bool attempt(std::string action, uint32 type_mask, WorldSession* session,
        uint32 limit, time_t cooldown);

    // removes entries that are no longer important time-wise, should be called
    // fairly regularly to keep data small and easy to look-up in.
    void clean();

private:
    typedef std::vector<time_t> entry;

    template <typename T>
    struct action_map_t
    {
        typedef std::map<std::string, std::map<T, entry>> type;
    };
    struct action_map
    {
        typedef std::map<std::string, entry> type;
    };

    bool check_local(std::string action, WorldSession* session, uint32 limit,
        time_t cooldown);
    bool check_global(std::string action, uint32 limit, time_t cooldown);
    bool check_ip(std::string action, WorldSession* session, uint32 limit,
        time_t cooldown);
    bool below_limit(entry& e, uint32 limit) const;

    bool useless_entry(entry& e) const;
    void comb_entry(entry& e) const;

    action_map_t<WorldSession*>::type local_;
    action_map::type global_;
    action_map_t<std::string>::type ip_;
};

// locked singleton, access is fully thread-safe
#define sActionLimit MaNGOS::Singleton<action_limit>

#endif
