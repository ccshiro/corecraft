-- World

INSERT INTO command VALUES
("debug pathing adt", 3, "Usage: .debug pathing adt n [dist] [no]. Where n is number of points and dist is between them. Type no at the end to not be teleported."),
("debug pathing wmo", 3, "Usage: .debug pathing wmo n [dist] [no]. Where n is number of points and dist is between them. Type no at the end to not be teleported."),
("debug pathing path", 3, "Usage: .debug pathing path [reverse]. Draws a path between you and your target. Reverses direction if reverse specified."),
("debug pathing position", 3, "Usage: .debug pathing position [ori] dist [norm]. Draw an arbitrary position in [ori], or if target is selected, angle between you and target. Specifiy norm to normalize Z value.");

INSERT INTO mangos_string(entry, content_default) VALUES
(1194, "   Random Water/Air"),
(1195, "   Charge"),
(1196, "   Group Waypoint");

UPDATE mangos_string SET content_default="Liquid level: %f, ground: %f, entry: %u, type flags %u, status: %d, checked vmap liquids: %s." WHERE entry=175;
