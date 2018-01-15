-- world

ALTER TABLE creature_text
ADD COLUMN text_range INT(11) UNSIGNED NOT NULL DEFAULT 0;
