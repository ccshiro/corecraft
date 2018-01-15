#include "action_limit.h"
#include "WorldSession.h"

bool action_limit::attempt(std::string action, uint32 type_mask,
    WorldSession* session, uint32 limit, time_t cooldown)
{
    // only players have their actions limited
    if (session->GetSecurity() > SEC_PLAYER)
        return true;

    if (type_mask & type::local)
    {
        if (!check_local(action, session, limit, cooldown))
            return false;
    }

    if (type_mask & type::global)
    {
        if (!check_global(action, limit, cooldown))
            return false;
    }

    if (type_mask & type::ip)
    {
        if (!check_ip(action, session, limit, cooldown))
            return false;
    }

    return true;
}

void action_limit::clean()
{
    // local
    for (auto& elem : local_)
    {
        for (auto j_itr = elem.second.begin(); j_itr != elem.second.end();)
        {
            if (useless_entry(j_itr->second))
            {
                j_itr = elem.second.erase(j_itr);
            }
            else
            {
                comb_entry(j_itr->second);
                ++j_itr;
            }
        }
    }
    // global
    for (auto itr = global_.begin(); itr != global_.end();)
    {
        if (useless_entry(itr->second))
        {
            itr = global_.erase(itr);
        }
        else
        {
            comb_entry(itr->second);
            ++itr;
        }
    }
    // ip
    for (auto& elem : ip_)
    {
        for (auto j_itr = elem.second.begin(); j_itr != elem.second.end();)
        {
            if (useless_entry(j_itr->second))
            {
                j_itr = elem.second.erase(j_itr);
            }
            else
            {
                comb_entry(j_itr->second);
                ++j_itr;
            }
        }
    }
}

bool action_limit::check_local(
    std::string action, WorldSession* session, uint32 limit, time_t cooldown)
{
    auto& local = local_[action][session];
    if (local.empty())
    {
        local.push_back(WorldTimer::time_no_syscall());
        return true;
    }
    // cooldown
    if (cooldown &&
        local[local.size() - 1] + cooldown > WorldTimer::time_no_syscall())
        return false;
    // limit
    if (limit)
    {
        if (!below_limit(local, limit))
            return false;
    }
    local.push_back(WorldTimer::time_no_syscall());
    return true;
}

bool action_limit::check_global(
    std::string action, uint32 limit, time_t cooldown)
{
    auto& global = global_[action];
    if (global.empty())
    {
        global.push_back(WorldTimer::time_no_syscall());
        return true;
    }
    // cooldown
    if (cooldown &&
        global[global.size() - 1] + cooldown > WorldTimer::time_no_syscall())
        return false;
    // limit
    if (limit)
    {
        if (!below_limit(global, limit))
            return false;
    }
    global.push_back(WorldTimer::time_no_syscall());
    return true;
}

bool action_limit::check_ip(
    std::string action, WorldSession* session, uint32 limit, time_t cooldown)
{
    auto& ip = ip_[action][session->GetRemoteAddress()];
    if (ip.empty())
    {
        ip.push_back(WorldTimer::time_no_syscall());
        return true;
    }
    // cooldown
    if (cooldown &&
        ip[ip.size() - 1] + cooldown > WorldTimer::time_no_syscall())
        return false;
    // limit
    if (limit)
    {
        if (!below_limit(ip, limit))
            return false;
    }
    ip.push_back(WorldTimer::time_no_syscall());
    return true;
}

bool action_limit::below_limit(entry& e, uint32 limit) const
{
    uint32 accumulated = 0;
    for (auto t : e)
    {
        if (t + 60 >= WorldTimer::time_no_syscall())
            ++accumulated;
    }
    return accumulated < limit;
}

bool action_limit::useless_entry(entry& e) const
{
    if (e.empty() || e[e.size() - 1] + 60 < WorldTimer::time_no_syscall())
        return true;

    return false;
}

void action_limit::comb_entry(entry& e) const
{
    for (auto itr = e.begin(); itr != e.end();)
    {
        if (*itr + 60 < WorldTimer::time_no_syscall())
            itr = e.erase(itr);
        else
            ++itr;
    }
}
