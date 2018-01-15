-- world
DELETE FROM `mangos_string` WHERE `entry`=175;
INSERT INTO `mangos_string` (`entry`,`content_default`) VALUES
(175, 'Liquid level: %f, ground: %f, entry: %u, type flags %u, status: %d.');
