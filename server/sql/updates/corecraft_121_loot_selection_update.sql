-- WORLD
RENAME TABLE loot_selection TO loot_slot_items;

CREATE TABLE loot_slots
(
id INT UNSIGNED NOT NULL,
name VARCHAR(127) NOT NULL,
has_quality TINYINT NOT NULL DEFAULT 0,
PRIMARY KEY(id)
);

CREATE TABLE loot_slot_rules
(
id INT UNSIGNED NOT NULL AUTO_INCREMENT,
description TEXT NOT NULL DEFAULT "",
drop_type INT UNSIGNED NOT NULL DEFAULT 0,
slot INT UNSIGNED NOT NULL DEFAULT 0,
chance FLOAT NOT NULL DEFAULT 0,
creature_types TEXT DEFAULT NULL,
creature_families TEXT DEFAULT NULL,
common_chance FLOAT NOT NULL DEFAULT 0,
uncommon_chance FLOAT NOT NULL DEFAULT 0,
rare_chance FLOAT NOT NULL DEFAULT 0,
epic_chance FLOAT NOT NULL DEFAULT 0,
common_chance_elite FLOAT NOT NULL DEFAULT 0,
uncommon_chance_elite FLOAT NOT NULL DEFAULT 0,
rare_chance_elite FLOAT NOT NULL DEFAULT 0,
epic_chance_elite FLOAT NOT NULL DEFAULT 0,
PRIMARY KEY(id)
);

ALTER TABLE gameobject_template
ADD COLUMN loot_selection_level INT UNSIGNED NOT NULL DEFAULT 0 AFTER maxgold;

ALTER TABLE item_template
ADD COLUMN loot_selection_level INT UNSIGNED NOT NULL DEFAULT 0 AFTER maxMoneyLoot;

ALTER TABLE loot_slot_items
ADD COLUMN min_count INT UNSIGNED NOT NULL DEFAULT 1,
ADD COLUMN max_count INT UNSIGNED NOT NULL DEFAULT 1,
ADD COLUMN min_level INT UNSIGNED NOT NULL DEFAULT 0,
ADD COLUMN max_level INT UNSIGNED NOT NULL DEFAULT 0,
DROP PRIMARY KEY,
ADD PRIMARY KEY(item_id, slot);

UPDATE loot_slot_items SET slot = 2 WHERE slot = 1;
UPDATE loot_slot_items SET slot = 1 WHERE slot = 0;

INSERT INTO loot_slots VALUES
(1, "Armor", 1),
(2, "Weapons", 1);
