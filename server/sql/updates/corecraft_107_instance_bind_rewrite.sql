-- characters
DROP TABLE group_instance;

CREATE TABLE group_instance
(
	gid INT UNSIGNED NOT NULL DEFAULT 0,
	instance INT UNSIGNED NOT NULL DEFAULT 0,
	perm TINYINT UNSIGNED NOT NULL DEFAULT 0,
	PRIMARY KEY(gid, instance),
	INDEX(gid)
);
