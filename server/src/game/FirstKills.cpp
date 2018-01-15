#include "FirstKills.h"
#include "Group.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "ObjectMgr.h"
#include "ProgressBar.h"
#include "Unit.h"
#include "World.h"
#include "Policies/Singleton.h"

void FirstKills::ProcessKilledBoss(Unit* pBoss, Group* pGroup)
{
    return; // XXX

    PublicBossData d = _GetPublicDataOfBoss(pBoss);
    if (d.boss_name.length() == 0)
        return;

    if (std::find(m_alreadyKiledBossess.begin(), m_alreadyKiledBossess.end(),
            d.boss_name) != m_alreadyKiledBossess.end())
        return;

    std::pair<uint32, Team> guild_pair = _GetGroupGuildIdAndFaction(pGroup);
    Guild* guild = sGuildMgr::Instance()->GetGuildById(guild_pair.first);
    Team faction = guild_pair.second;
    if (!guild || faction == TEAM_NONE)
        return;

    // Send a message to the guild, through the highest ranked player online,
    // informing them about their first kill
    std::string msg;
    if (d.instance_name.length() > 0)
        msg = "Congratulations, <" + guild->GetName() +
              ">, on the server's first kill of " + d.instance_name + " - " +
              d.boss_name + ".";
    else
        msg = "Congratulations, <" + guild->GetName() +
              ">, on the server's first kill of " + d.boss_name + ".";
    sWorld::Instance()->SendServerMessage(
        SERVER_MSG_CUSTOM, msg.c_str(), nullptr);

    RegisterFirstKill(pBoss->GetEntry(), guild->GetId(), guild->GetName(),
        faction, d.instance_name, d.boss_name, d.boss_order_in_instance);
}

void FirstKills::ProcessSpecialEvent(FirstKillEvents fkEvent, Group* pGroup)
{
    return; // XXX

    PublicBossData d = _GetPublicDataOfEvent(fkEvent);
    if (d.boss_name.length() == 0)
        return;

    if (std::find(m_alreadyKiledBossess.begin(), m_alreadyKiledBossess.end(),
            d.boss_name) != m_alreadyKiledBossess.end())
        return;

    std::pair<uint32, Team> guild_pair = _GetGroupGuildIdAndFaction(pGroup);
    Guild* guild = sGuildMgr::Instance()->GetGuildById(guild_pair.first);
    Team faction = guild_pair.second;
    if (!guild || faction == TEAM_NONE)
        return;

    // Send a message to the guild, through the highest ranked player online,
    // informing them about their first kill
    std::string msg;
    if (d.instance_name.length() > 0)
        msg = "Congratulations, <" + guild->GetName() +
              ">, on the server's first kill of " + d.instance_name + " - " +
              d.boss_name + ".";
    else
        msg = "Congratulations, <" + guild->GetName() +
              ">, on the server's first kill of " + d.boss_name + ".";
    sWorld::Instance()->SendServerMessage(
        SERVER_MSG_CUSTOM, msg.c_str(), nullptr);

    RegisterFirstKill(0, guild->GetId(), guild->GetName(), faction,
        d.instance_name, d.boss_name, d.boss_order_in_instance);
}

void FirstKills::RegisterFirstKill(uint32 bossEntry, uint32 guildId,
    std::string guildName, Team guild_faction, std::string instanceName,
    std::string bossName, uint32 orderInInstance)
{
    std::string faction;
    if (guild_faction == HORDE)
        faction = "horde";
    else
        faction = "alliance";
    CharacterDatabase.escape_string(guildName);
    CharacterDatabase.escape_string(bossName);
    CharacterDatabase.escape_string(instanceName);
    CharacterDatabase.PExecute(
        "INSERT INTO first_kills VALUES(%i, \"%s\", \"%s\", \"%s\", %i, "
        "\"%s\", %i, %i)",
        guildId, guildName.c_str(), faction.c_str(), instanceName.c_str(),
        orderInInstance, bossName.c_str(), bossEntry,
        (uint32)WorldTimer::time_no_syscall());
    m_alreadyKiledBossess.push_back(bossName);
}

void FirstKills::LoadAllFirstKillsFromDB()
{
    std::unique_ptr<QueryResult> result(
        CharacterDatabase.PQuery("SELECT bossName FROM first_kills"));
    uint32 count = 0;
    if (result)
    {
        BarGoLink bar(result->GetRowCount());
        do
        {
            bar.step();
            Field* field = result->Fetch();
            m_alreadyKiledBossess.push_back(field[0].GetCppString());
        } while (result->NextRow());
    }
    logging.info("Loaded %u first kills\n", count);
}

std::pair<uint32, Team> FirstKills::_GetGroupGuildIdAndFaction(Group* pGroup)
{
    // Count all guilds' members
    std::map<uint32, std::pair<uint8 /* Online Players */, Team /* Faction */>>
        guild_count;
    std::list<uint32> listed_guilds;
    for (const auto& elem : pGroup->GetMemberSlots())
    {
        // We process online players only
        if (Player* pl = sObjectMgr::Instance()->GetPlayer(elem.guid, true))
        {
            if (pl->GetGuildId() == 0)
                continue;

            if (std::find(listed_guilds.begin(), listed_guilds.end(),
                    pl->GetGuildId()) == listed_guilds.end())
            {
                guild_count[pl->GetGuildId()].first = 1;
                guild_count[pl->GetGuildId()].second = pl->GetTeam();
                listed_guilds.push_back(pl->GetGuildId());
            }
            else
                guild_count[pl->GetGuildId()].first += 1;
        }
    }
    // Choose the guild with the most counted members
    uint32 max_count = 0;
    uint32 main_guild_id = 0;
    Team faction = TEAM_NONE;
    for (auto& listed_guild : listed_guilds)
    {
        if (guild_count[listed_guild].first > max_count)
        {
            main_guild_id = listed_guild;
            faction = guild_count[listed_guild].second;
            max_count = guild_count[listed_guild].first;
        }
    }

    return std::pair<uint32, Team>(main_guild_id, faction);
}

/*
    The public name of bossess differ sometimes from their actual name,
    so we return it for everyone, just to make sure it's all good.
*/
PublicBossData FirstKills::_GetPublicDataOfBoss(Unit* pBoss)
{
    switch (pBoss->GetEntry())
    {
    /* Karazhan */
    case 15550:
        return PublicBossData("Attumen the Huntsman", "Karazhan", 1);
    case 15687:
        return PublicBossData("Moroes", "Karazhan", 2);
    case 16457:
        return PublicBossData("Maiden of Virtue", "Karazhan", 3);
    case 18168:
        return PublicBossData("Opera - Wizard of Oz", "Karazhan", 4);
    case 17521:
        return PublicBossData("Opera - Red Riding Hood", "Karazhan", 4);
    case 17533:
        return PublicBossData("Opera - Romulo and Julianne", "Karazhan", 4);
    case 15691:
        return PublicBossData("The Curator", "Karazhan", 5);
    /*case : -- Chess Event
        return PublicBossData("", "Karazhan", 6);*/
    case 15688:
        return PublicBossData("Terestian Illhoof", "Karazhan", 7);
    case 16524:
        return PublicBossData("Shade of Aran", "Karazhan", 8);
    case 15689:
        return PublicBossData("Netherspite", "Karazhan", 9);
    case 17225:
        return PublicBossData("Nightbane", "Karazhan", 10);
    case 15690:
        return PublicBossData("Prince Malchezaar", "Karazhan", 11);

    /* Gruul's Lair */
    case 18831:
        return PublicBossData("High King Maulgar", "Gruul's Lair", 1);
    case 19044:
        return PublicBossData("Gruul the Dragonkiller", "Gruul's Lair", 2);

    /* Magtheridon's Lair */
    case 17257:
        return PublicBossData("Magtheridon", "Magtheridon's Lair", 1);

    /* Serpentshrine Cavern */
    case 21216:
        return PublicBossData(
            "Hydross the Unstable", "Serpentshrine Cavern", 1);
    case 21217:
        return PublicBossData("The Lurker Below", "Serpentshrine Cavern", 2);
    case 21215:
        return PublicBossData("Leotheras the Blind", "Serpentshrine Cavern", 3);
    case 21214:
        return PublicBossData(
            "Fathom-Lord Karathress", "Serpentshrine Cavern", 4);
    case 21213:
        return PublicBossData("Morogrim Tidewalker", "Serpentshrine Cavern", 5);
    case 21212:
        return PublicBossData("Lady Vashj", "Serpentshrine Cavern", 6);

    /* The Eye */
    case 19516:
        return PublicBossData("Void Reaver", "The Eye", 1);
    case 19514:
        return PublicBossData("Al'ar", "The Eye", 2);
    case 18805:
        return PublicBossData("High Astromancer Solarian", "The Eye", 3);
    case 19622:
        return PublicBossData("Kael'thas Sunstrider", "The Eye", 4);

    /* Hyjal Summit */
    case 17767:
        return PublicBossData("Rage Winterchill", "Hyjal Summit", 1);
    case 17808:
        return PublicBossData("Anetheron", "Hyjal Summit", 2);
    case 17888:
        return PublicBossData("Kaz'rogal", "Hyjal Summit", 3);
    case 17842:
        return PublicBossData("Azgalor", "Hyjal Summit", 4);
    case 17968:
        return PublicBossData("Archimonde", "Hyjal Summit", 5);

    /* Black Temple */
    case 22887:
        return PublicBossData("High Warlord Naj'entus", "Black Temple", 1);
    case 22898:
        return PublicBossData("Supremus", "Black Temple", 2);
    case 22841:
        return PublicBossData("Shade of Akama", "Black Temple", 3);
    case 22871:
        return PublicBossData("Teron Gorefiend", "Black Temple", 4);
    case 22948:
        return PublicBossData("Gurtogg Bloodboil", "Black Temple", 5);
    case 22856:
        return PublicBossData("Reliquary of the Lost", "Black Temple", 6);
    case 22947:
        return PublicBossData("Mother Shahraz", "Black Temple", 7);
    case 23426:
        return PublicBossData("The Illidari Council", "Black Temple", 8);
    case 22917:
        return PublicBossData("Illidan Stormrage", "Black Temple", 9);

    /* Zul'Aman */
    case 23574:
        return PublicBossData("Akil'zon", "Zul'Aman", 1);
    case 23576:
        return PublicBossData("Nalorakk", "Zul'Aman", 2);
    case 23578:
        return PublicBossData("Jan'alai", "Zul'Aman", 3);
    case 23577:
        return PublicBossData("Halazzi", "Zul'Aman", 4);
    case 24239:
        return PublicBossData("Hex Lord Malacrass", "Zul'Aman", 5);
    case 23863:
        return PublicBossData("Daakara", "Zul'Aman", 6);

    /* Sunwell Plateau */
    case 24892:
        return PublicBossData(
            "Kalecgos & Sathrovarr the Corruptor", "Sunwell Plateau", 1);
    case 24882:
        return PublicBossData("Brutallus", "Sunwell Plateau", 2);
    case 25038:
        return PublicBossData("Felmyst", "Sunwell Plateau", 3);
    case 25166: // TODO: PROBABLY WRONG, since they merge
    case 25165:
        return PublicBossData("Eredar Twins", "Sunwell Plateau", 4);
    case 25840:
        return PublicBossData("M'uru", "Sunwell Plateau", 5);
    case 25315:
        return PublicBossData("Kil'jaeden", "Sunwell Plateau", 6);

    default:
        return PublicBossData("", "", 0);
    }
}

/* Boss Encounters and Events that do not reward loot through the boss you slay
 */
PublicBossData FirstKills::_GetPublicDataOfEvent(FirstKillEvents fkEvent)
{
    switch (fkEvent)
    {
    case FKE_CHESS_EVENT:
        return PublicBossData("Chess Event", "Karazhan", 6);
    default:
        return PublicBossData("", "", 0);
    }
}
