import argparse, re
import pymysql

parser = argparse.ArgumentParser()

# optional
parser.add_argument("--db", help="update database?", action='store_true')
parser.add_argument("--host", "-s", help="mysql hostname")
parser.add_argument("--database", "-d", help="mysql database")
parser.add_argument("--user", "-u", help="mysql user")
parser.add_argument("--pwd", "-p", help="mysql password")
parser.add_argument("--port", help="mysql port")

args = parser.parse_args()

if args.db:
    usr = args.user if args.user else ""
    pwd = args.pwd if args.pwd else ""
    prt = args.port if args.port else 3306
    conn = pymysql.connect(host=args.host, db=args.database, user=usr,
        passwd=pwd, port=prt)
    cur = conn.cursor()
    print("connected to mysql")

new_unit = ""
with open("../../src/game/Unit.cpp") as f:
    content = f.read()

    find = content.find("Unit::AddThreat")
    if find == -1:
        raise RuntimeError("Could not find Unit::AddThreat")

    find = content.find("{\n", find)
    if find == -1:
        raise RuntimeError("Could not find start of function")

    new_unit = content[:find+2]
    new_unit += "    if (GetTypeId() == TYPEID_UNIT && strcmp(GetName(), \"Target Dummy\") == 0 && GetEntry() != 2673)\n"
    new_unit += "    {\n"
    new_unit += "        SetInCombatState(true);\n"
    new_unit += "        pVictim->SetInCombatState(true);\n"
    new_unit += "        return;\n"
    new_unit += "    }\n\n"
    new_unit += content[find+2:]

if len(new_unit) == 0:
    raise RuntimeError("Unable to patch Unit::AddThreat")

with open("../../src/game/Unit.cpp", "w") as f:
    f.write(new_unit)

print("patched Unit::AddThreat")

new_player = ""
with open("../../src/game/Player.cpp") as f:
    content = f.read()

    content = re.sub("SetLocationMapId\(info->mapId\)",
        "SetLocationMapId(530)", content)
    content = re.sub(("Relocate\(info->positionX, info->positionY, "
        "info->positionZ\)"),
        "Relocate(-1848.5, 5399.8, -12.4278)", content)
    content = re.sub("SetOrientation\(info->orientation\)",
        "SetOrientation(2.04)", content)
    content = re.sub(("SetMap\(sMapMgr::Instance\(\)->CreateMap\("
        "info->mapId, this\)\)"),
        "SetMap(sMapMgr::Instance()->CreateMap(530, this))", content)
    content = re.sub(("SetUInt32Value\(PLAYER_FIELD_COINAGE,\n"
        "        sWorld::Instance\(\)->getConfig\("
        "CONFIG_UINT32_START_PLAYER_MONEY\)\)"),
        "storage().money(inventory::gold(10000));\n    "
        "SetUInt32Value(PLAYER_FIELD_COINAGE, storage().money().get())",
        content)
        
    # Add heroic keys when players login, if they don't have them already
    content = re.sub("Unit::AddToWorld\(\);",
        "Unit::AddToWorld();\n\n" +
        "    if (!HasItemCount(GetTeam() == ALLIANCE ? 30622 : 30637, 1, "
        "true)) add_item(GetTeam() == ALLIANCE ? 30622 : 30637, 1);\n" +
        "    if (!HasItemCount(30633, 1, true)) add_item(30633, 1);\n" +
        "    if (!HasItemCount(30634, 1, true)) add_item(30634, 1);\n" +
        "    if (!HasItemCount(30635, 1, true)) add_item(30635, 1);\n" +
        "    if (!HasItemCount(30623, 1, true)) add_item(30623, 1);\n" +
        "    if (!HasItemCount(27991, 1, true)) add_item(27991, 1);\n" +
        "    if (!HasItemCount(28395, 1, true)) add_item(28395, 1);\n" +
        "    if (!HasItemCount(31084, 1, true)) add_item(31084, 1);",
        content)

    new_player = content

if len(new_player) == 0:
    raise RuntimeError("Unable to patch Player.cpp")

with open("../../src/game/Player.cpp", "w") as f:
    f.write(new_player)

print("patched Player.cpp")

new_duelhandler = ""
with open("../../src/game/DuelHandler.cpp") as f:
    content = f.read()

    content = re.sub("    plTarget->SendDuelCountdown\(3000\);",
        ("    plTarget->SendDuelCountdown(3000);\n" +
            "    pl->RemoveArenaSpellCooldowns();\n" +
            "    plTarget->RemoveArenaSpellCooldowns();"), content)

    new_duelhandler = content

if len(new_duelhandler) == 0:
    raise RuntimeError("Unable to patch DuelHandler.cpp")

with open("../../src/game/DuelHandler.cpp", "w") as f:
    f.write(new_duelhandler)

print("patched DuelHandler.cpp")

if args.db:
    data = ""
    with open("data.sql") as f:
        data = f.read()

    if len(data) == 0:
        raise RuntimeError("data.sql corrupt")

    cur.execute("SELECT MAX(entry) FROM creature_template")
    entry = str(cur.fetchone()[0] + 1)
    cur.execute("SELECT MAX(guid) FROM creature")
    guid = str(cur.fetchone()[0] + 1)

    print("entry: " + entry)
    print("guid: " + guid)

    data = re.sub("@ctst@", entry, data)
    data = re.sub("@cst@", guid, data)

    cur.execute(data)

    print("imported SQL")

print("done")

