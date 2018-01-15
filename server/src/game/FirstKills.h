#ifndef _FIRSTKILLS_H
#define _FIRSTKILLS_H

#include "SharedDefines.h"
#include <list>
#include <string>
#include <utility>

class Group;
class Unit;

enum FirstKillEvents
{
    FKE_CHESS_EVENT = 0,
    FKE_ZUL_AMAN_TIMED_EVENT
};

struct PublicBossData
{
    PublicBossData(std::string const& s1, std::string const& s2, uint32 i1)
    {
        boss_name = s1;
        instance_name = s2;
        boss_order_in_instance = i1;
    }

    std::string boss_name;
    std::string instance_name;
    uint32 boss_order_in_instance; // From 1 to MAX_BOSS of instance
};

class FirstKills
{
    // Gets name and instance name
    PublicBossData _GetPublicDataOfBoss(Unit* pBoss);
    PublicBossData _GetPublicDataOfEvent(FirstKillEvents fkEvent);
    std::list<std::string> m_alreadyKiledBossess;

    std::pair<uint32, Team> _GetGroupGuildIdAndFaction(Group* pGroup);

public:
    void ProcessKilledBoss(Unit* pBoss, Group* pGroup);
    void ProcessSpecialEvent(FirstKillEvents fkEvent, Group* pGroup);

    void LoadAllFirstKillsFromDB();
    void RegisterFirstKill(uint32 bossEntry, uint32 guildId,
        std::string guildName, Team guild_faction, std::string instanceName,
        std::string bossName, uint32 orderInInstance);
};

#define sFirstKills MaNGOS::UnlockedSingleton<FirstKills>

#endif
