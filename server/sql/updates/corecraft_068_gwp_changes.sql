ALTER TABLE creature_group_waypoints ADD PRIMARY KEY(group_id, point);
ALTER TABLE creature_group_waypoints ADD COLUMN mmap TINYINT UNSIGNED NOT NULL DEFAULT 0;
ALTER TABLE creature_groups ADD COLUMN movement_leader_guid INT(11) UNSIGNED NOT NULL DEFAULT 0 AFTER leader_guid;