-- world
ALTER TABLE creature_ai_spells
ADD COLUMN phase_mask INT UNSIGNED NOT NULL DEFAULT 0;
