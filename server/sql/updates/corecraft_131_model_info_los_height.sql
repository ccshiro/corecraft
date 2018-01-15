-- world

ALTER TABLE creature_model_info
ADD COLUMN los_height FLOAT NOT NULL DEFAULT 0 AFTER combat_reach;
