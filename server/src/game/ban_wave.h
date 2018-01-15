#ifndef GAME__BAN_WAVE_H
#define GAME__BAN_WAVE_H

#include "Common.h"
#include "Policies/Singleton.h"
#include <boost/thread/mutex.hpp>
#include <sstream>
#include <string>
#include <vector>

class Player;

class ban_wave
{
public:
    void load_from_db();
    void save_to_db();

    void add_ban(Player* player, const std::string& reason,
        const std::string& banned_by);
    void add_ban(const std::string& char_name, const std::string& reason,
        const std::string& banned_by);
    void remove_ban(Player* player, const std::string& removed_by);
    void remove_ban(
        const std::string& char_name, const std::string& removed_by);
    bool has_ban_on(Player* player);
    bool has_ban_on(const std::string& char_name);

    uint32 do_ban_wave();

    std::string print();
    std::string search(const std::string& char_name);

    static std::string fix_name(const std::string& str);

private:
    struct entry
    {
        uint32 account_id;
        std::string char_name;
        std::string reason;
        std::string banned_by;
        std::string removed_by; // empty if still in effect

        bool operator<(const entry& rhs) const
        {
            return char_name.compare(rhs.char_name) < 0;
        }
    };

    void print_entries(
        const std::vector<entry>& entries, std::stringstream& ss);

    std::mutex mutex_;
    std::vector<entry> pending_bans_;
};

#define sBanWave MaNGOS::Singleton<ban_wave>

#endif
