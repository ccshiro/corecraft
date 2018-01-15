-- Characters

-- Add a map column to creature_respawn, needed for non-instance maps (continents)
ALTER TABLE `creature_respawn`
ADD COLUMN `map` INT UNSIGNED NOT NULL DEFAULT '0';
