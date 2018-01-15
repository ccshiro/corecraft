-- world

ALTER TABLE areatrigger_teleport
DROP COLUMN required_item,
DROP COLUMN required_item2,
DROP COLUMN heroic_key,
DROP COLUMN heroic_key2,
DROP COLUMN required_quest_done_heroic,
DROP COLUMN required_failed_text;

CREATE TABLE map_entry_requirements
(
    map_id INT UNSIGNED NOT NULL DEFAULT 0,
    required_level INT UNSIGNED NOT NULL DEFAULT 0,
    required_items TEXT DEFAULT NULL,
    required_quests TEXT DEFAULT NULL,
    PRIMARY KEY(map_id)
);

DELETE FROM mangos_string WHERE entry=370 or entry=368;
