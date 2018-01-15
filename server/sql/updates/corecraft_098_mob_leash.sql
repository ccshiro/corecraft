-- world

ALTER TABLE creature
ADD COLUMN leash_x FLOAT DEFAULT 0 AFTER boss_link_guid,
ADD COLUMN leash_y FLOAT DEFAULT 0 AFTER leash_x,
ADD COLUMN leash_z FLOAT DEFAULT 0 AFTER leash_y,
ADD COLUMN leash_radius FLOAT DEFAULT 0 AFTER leash_z;
