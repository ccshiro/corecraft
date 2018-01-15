-- characters

ALTER TABLE characters
ADD COLUMN pvp_flagged TINYINT UNSIGNED NOT NULL DEFAULT 0;
