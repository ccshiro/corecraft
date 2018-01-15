-- characters

CREATE TABLE character_category_cooldown
(
guid INT(11) UNSIGNED NOT NULL DEFAULT 0,
category INT(11) UNSIGNED NOT NULL DEFAULT 0,
`time` INT(11) UNSIGNED NOT NULL DEFAULT 0,
INDEX guid_index (guid)
);
