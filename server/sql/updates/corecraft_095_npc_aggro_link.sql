-- World

DROP TABLE IF EXISTS npc_aggro_link;
CREATE TABLE npc_aggro_link
(
    boss_entry INT(11) UNSIGNED NOT NULL DEFAULT '0',
    boss_guid INT(11) UNSIGNED NOT NULL DEFAULT '0',
    defender_entry INT(11) UNSIGNED NOT NULL DEFAULT '0',
    defender_guid INT(11) UNSIGNED NOT NULL DEFAULT '0',
    PRIMARY KEY(boss_entry, boss_guid, defender_entry, defender_guid),
    UNIQUE (defender_entry, defender_guid)
);

DELETE FROM command WHERE name="npc aggrolink";
INSERT INTO command (name, security, help)
VALUES ("npc aggrolink", 3, "Syntax: .npc aggrolink boss_entry boss_guid. Links your target to that entry & guid.");
