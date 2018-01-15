-- world
CREATE TABLE `waypoints_group`
(
    id INT UNSIGNED NOT NULL DEFAULT 0,
    point INT UNSIGNED NOT NULL DEFAULT 0,
    x FLOAT NOT NULL DEFAULT 0,
    y FLOAT NOT NULL DEFAULT 0,
    z FLOAT NOT NULL DEFAULT 0,
    o FLOAT NOT NULL DEFAULT 0,
    delay INT UNSIGNED NOT NULL DEFAULT 0,
    run TINYINT UNSIGNED NOT NULL DEFAULT 0,
    mmap TINYINT UNSIGNED NOT NULL DEFAULT 0,
    comment TEXT,
    PRIMARY KEY (id, point)
);

INSERT INTO `command` (name, security, help) VALUES
("smartai addgwp", 3, "Usage:
.smartai addgwp id ""comment"" [delay] [run] [mmap]");
