-- Run sql on mangos database

-- Remove the linking manager
DROP TABLE IF EXISTS creature_linking_template;
-- And the npc link command
DELETE FROM command WHERE name="npc_link";

-- Add the creature_groups table
CREATE TABLE creature_groups
(
    id SERIAL NOT NULL COMMENT 'Id identifying the group',
    map INT NOT NULL DEFAULT 0 COMMENT 'Id of the map the group is in',
    special_flags INT NOT NULL DEFAULT 0 COMMENT 'check enum CREATURE_GROUP_SPECIAL_FLAGS @ GroupPullMgr.h',
    leader_guid INT NOT NULL DEFAULT 0 COMMENT 'Allows for an npc to be the leader of the group, 0 if everyone is equal',
    name VARCHAR(127) NOT NULL DEFAULT '' COMMENT 'Name of the group, only used to make it easier for editing',
    PRIMARY KEY(id),
    INDEX(map)
) CHARSET=utf8 COMMENT='Creature Groups that aggro and behave together';

CREATE TABLE creature_group_members
(
    group_id INT NOT NULL DEFAULT 0 COMMENT 'creature_groups.id that the member belongs to',
    creature_guid INT NOT NULL DEFAULT 0 COMMENT 'creature.guid of this member',
    PRIMARY KEY(creature_guid),
    INDEX(group_id)
);

-- Add commands for controlling the Group's in-game:

-- Group creation
-- Create
INSERT INTO command (name, security, help) VALUES("npc group create", 2, "Syntax: .npc group create $GroupName.\n\nCreates a group with the given name.");
-- Delete
INSERT INTO command (name, security, help) VALUES("npc group delete", 2, "Syntax: .npc group delete #id.\n\nDeletes group with ID. Requires restart to take global effect.");
-- List
INSERT INTO command (name, security, help) VALUES("npc group list", 2, "Syntax: .npc group list.\n\nLists all groups in map.");
-- Rename
INSERT INTO command (name, security, help) VALUES("npc group rename", 2, "Syntax: .npc group rename #id $GroupName.\n\nRenames the group with the given ID to the specified Name.");

-- Member managing
-- Add
INSERT INTO command (name, security, help) VALUES("npc group add", 2, "Syntax: .npc group add #id.\n\nAdds your target NPC to the group with the specified ID.");
-- Remove
INSERT INTO command (name, security, help) VALUES("npc group remove", 2, "Syntax: .npc group remove #id.\n\nRemoves your target NPC from the group with the specified ID.");
-- Leader
INSERT INTO command (name, security, help) VALUES("npc group leader", 2, "Syntax: .npc group leader #id.\n\nMakes your target leader of the group, or if he already is, makes the group leader-less.");

-- Info flags
-- Info
INSERT INTO command (name, security, help) VALUES("npc group info", 2, "Syntax: .npc group info {#id}.\n\nShows info about your target's group -- or if an ID is specified, the group with that ID.");
-- Flag List
INSERT INTO command (name, security, help) VALUES("npc group flag list", 2, "Syntax: .npc group flag list.\n\nShows all available flags.");
-- Flag Add
INSERT INTO command (name, security, help) VALUES("npc group flag add", 2, "Syntax: .npc group flag add #id $Flag.\n\nAdds the specified flag to the group with ID. Write \".npc group flag list\" to see all available flags.");
-- Flag Remove
INSERT INTO command (name, security, help) VALUES("npc group flag remove", 2, "Syntax: .npc group flag remove #id $Flag.\n\nRemoves the specified flag from the group with ID. Write \".npc group flag list\" to see all available flags.");
-- Show
INSERT INTO command (name, security, help) VALUES("npc group show", 2, "Syntax: .npc group show #id.\n\nDisplays a red arrow above the head of all group members. The leader (if one exists) will be having floating hearts above his head.");
-- Unshow
INSERT INTO command (name, security, help) VALUES("npc group unshow", 2, "Syntax: .npc group show #id.\n\nHides the effect of show for that group.");
-- Go
INSERT INTO command (name, security, help) VALUES("npc group go", 2, "Syntax: .npc group go #id.\n\nTeleports you to the closest member of the group.");
