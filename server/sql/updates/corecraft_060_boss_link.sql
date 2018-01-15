ALTER TABLE `creature`
ADD COLUMN `boss_link_entry` INT(10) UNSIGNED  NOT NULL DEFAULT 0;
ALTER TABLE `creature`
ADD COLUMN `boss_link_guid` INT(10) UNSIGNED NOT NULL DEFAULT 0;

INSERT INTO `command` (`name`, `security`, `help`)
    VALUES("npc bosslink", 2, "Syntax: .npc bosslink [entry] [guid]. Both parameters optional. Target creature to be linked, use entry and guid of boss.");
