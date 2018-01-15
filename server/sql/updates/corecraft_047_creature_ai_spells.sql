CREATE TABLE creature_ai_spells
(
    `creature_id` INT UNSIGNED NOT NULL DEFAULT 0,
    `spell_id`  INT UNSIGNED NOT NULL DEFAULT 0,
    `heroic_spell_id` INT UNSIGNED NOT NULL DEFAULT 0,
    `type` INT UNSIGNED NOT NULL DEFAULT 0,
    `priority` INT UNSIGNED NOT NULL DEFAULT 0,
    `cooldown_min` INT UNSIGNED NOT NULL DEFAULT 0,
    `cooldown_max` INT UNSIGNED NOT NULL DEFAULT 0,
    `target_settings` INT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (creature_id, spell_id)
)
