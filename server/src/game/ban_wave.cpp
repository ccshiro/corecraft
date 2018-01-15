#include "ban_wave.h"
#include "AccountMgr.h"
#include "logging.h"
#include "ObjectMgr.h"
#include "ProgressBar.h"
#include "World.h"
#include "Database/Database.h"
#include <cctype>

void ban_wave::load_from_db()
{
    std::lock_guard<std::mutex> guard(mutex_);

    std::unique_ptr<QueryResult> result(CharacterDatabase.PQuery(
        "SELECT account_id, char_name, reason, banned_by, removed_by FROM "
        "ban_wave"));

    if (!result)
    {
        logging.info("Loaded 0 pending bans for the Ban Wave system.\n");
        return;
    }

    uint32 count = 0;
    pending_bans_.clear();

    BarGoLink bar(result->GetRowCount());
    do
    {
        ++count;
        bar.step();

        Field* fields = result->Fetch();
        entry e;
        e.account_id = fields[0].GetUInt32();
        e.char_name = fields[1].GetCppString();
        e.reason = fields[2].GetCppString();
        e.banned_by = fields[3].GetCppString();
        e.removed_by = fields[4].GetCppString();

        pending_bans_.push_back(e);

    } while (result->NextRow());

    logging.info("Loaded %u pending bans for the Ban Wave system.\n", count);
}

void ban_wave::save_to_db()
{
    std::lock_guard<std::mutex> guard(mutex_);

    CharacterDatabase.BeginTransaction();

    CharacterDatabase.Execute("DELETE FROM ban_wave");

    std::stringstream ss;
    ss << "INSERT INTO ban_wave (account_id, char_name, reason, banned_by, "
          "removed_by) VALUES ";
    for (auto itr = pending_bans_.begin(); itr != pending_bans_.end(); ++itr)
    {
        std::string name = itr->char_name, reason = itr->reason,
                    ban = itr->banned_by, remove = itr->removed_by;
        CharacterDatabase.escape_string(name);
        CharacterDatabase.escape_string(reason);
        CharacterDatabase.escape_string(ban);
        CharacterDatabase.escape_string(remove);
        ss << "(" << itr->account_id << ", '" << name << "', '" << reason
           << "', '" << ban << "', '" << remove << "')";
        if (itr + 1 != pending_bans_.end())
            ss << ", ";
    }

    if (!pending_bans_.empty())
        CharacterDatabase.Execute(ss.str().c_str());

    CharacterDatabase.CommitTransaction();
}

std::string ban_wave::fix_name(const std::string& str)
{
    std::string new_str;
    for (std::size_t i = 0; i < str.size(); ++i)
        new_str += (i == 0 ? std::toupper(str[i]) : std::tolower(str[i]));
    return new_str;
}

void ban_wave::add_ban(
    Player* player, const std::string& reason, const std::string& banned_by)
{
    add_ban(player->GetName(), reason, banned_by);
}

void ban_wave::add_ban(const std::string& char_name, const std::string& reason,
    const std::string& banned_by)
{
    std::string name = fix_name(char_name);

    // If we already have a ban we don't allow to place another one, however if
    // that one
    // has been removed, we allow the overwriting of the removed ban.
    {
        std::lock_guard<std::mutex> guard(mutex_);

        for (auto itr = pending_bans_.begin(); itr != pending_bans_.end();
             ++itr)
        {
            if (itr->char_name.compare(name) == 0)
            {
                if (itr->removed_by.empty())
                {
                    return;
                }
                else
                {
                    pending_bans_.erase(itr);
                    break;
                }
            }
        }
    }

    uint32 acc_id =
        sObjectMgr::Instance()->GetPlayerAccountIdByPlayerName(char_name);
    if (!acc_id)
        return;

    entry e;
    e.account_id = acc_id;
    e.char_name = name;
    e.reason = reason;
    e.banned_by = banned_by;

    {
        std::lock_guard<std::mutex> guard(mutex_);
        pending_bans_.push_back(e);
    }

    save_to_db();
}

void ban_wave::remove_ban(Player* player, const std::string& removed_by)
{
    remove_ban(player->GetName(), removed_by);
}

void ban_wave::remove_ban(
    const std::string& char_name, const std::string& removed_by)
{
    if (char_name.empty())
        return;

    std::string name = fix_name(char_name);

    bool update = false;

    {
        std::lock_guard<std::mutex> guard(mutex_);
        for (auto& elem : pending_bans_)
        {
            if (elem.char_name.compare(name) == 0)
            {
                elem.removed_by = fix_name(removed_by);
                update = true;
                break;
            }
        }
    }

    if (update)
        save_to_db();
}

bool ban_wave::has_ban_on(Player* player)
{
    return has_ban_on(player->GetName());
}

bool ban_wave::has_ban_on(const std::string& char_name)
{
    std::string name = fix_name(char_name);

    std::lock_guard<std::mutex> guard(mutex_);

    for (auto& elem : pending_bans_)
    {
        if (elem.char_name.compare(name) == 0 && elem.removed_by.empty())
            return true;
    }
    return false;
}

uint32 ban_wave::do_ban_wave()
{
    uint32 count = 0;

    {
        std::lock_guard<std::mutex> guard(mutex_);

        for (auto& elem : pending_bans_)
        {
            // Skip removed bans
            if (elem.removed_by.size() > 0)
                continue;

            std::string acc_name;
            if (sAccountMgr::Instance()->GetName(elem.account_id, acc_name))
            {
                sWorld::Instance()->BanAccount(
                    BAN_ACCOUNT, acc_name, 0, elem.reason, elem.banned_by);
                ++count;
            }
        }

        pending_bans_.clear();
    }

    save_to_db(); // mutex_ lock released at this point
    return count;
}

void ban_wave::print_entries(
    const std::vector<entry>& entries, std::stringstream& ss)
{
    for (const auto& entrie : entries)
    {
        if (sObjectAccessor::Instance()->FindPlayerByName(
                entrie.char_name.c_str()))
            ss << "|cffffffff|Hplayer:" << entrie.char_name << "|h["
               << entrie.char_name << "]|h|r";
        else
            ss << entrie.char_name;
        ss << ": " << entrie.reason << " (by: " << entrie.banned_by;
        if (entrie.removed_by.size() > 0)
            ss << " removed by: " << entrie.removed_by;
        ss << ")\n";
    }
}

std::string ban_wave::print()
{
    std::lock_guard<std::mutex> guard(mutex_);

    std::vector<entry> still_in_effect;
    std::vector<entry> removed;

    for (auto& elem : pending_bans_)
    {
        if (elem.removed_by.empty())
            still_in_effect.push_back(elem);
        else
            removed.push_back(elem);
    }

    std::stringstream ss;
    ss << "Total bans: " << pending_bans_.size() << " (" << removed.size()
       << " of those cancelled)\n";
    ss << "Pending bans:\n";

    std::sort(still_in_effect.begin(), still_in_effect.end());
    print_entries(still_in_effect, ss);

    if (still_in_effect.empty())
        ss << "none\n";

    ss << "Cancelled bans:\n";

    std::sort(removed.begin(), removed.end());
    print_entries(removed, ss);

    if (removed.empty())
        ss << "none\n";

    return ss.str();
}

std::string ban_wave::search(const std::string& char_name)
{
    std::lock_guard<std::mutex> guard(mutex_);

    std::string search_lower = char_name;
    for (auto& elem : search_lower)
        elem = std::tolower(elem);

    std::vector<entry> found_entries;
    for (auto& elem : pending_bans_)
    {
        std::string lower_case = elem.char_name;
        for (auto& lower_case_i : lower_case)
            lower_case_i = std::tolower(lower_case_i);

        if (lower_case.find(search_lower) != std::string::npos)
            found_entries.push_back(elem);
    }

    std::sort(found_entries.begin(), found_entries.end());

    if (found_entries.empty())
    {
        return "The search gave no results.";
    }
    else
    {
        std::stringstream ss;
        ss << found_entries.size() << " results:\n";
        std::sort(found_entries.begin(), found_entries.end());
        print_entries(found_entries, ss);
        return ss.str();
    }
}
