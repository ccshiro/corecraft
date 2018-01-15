CREATE TABLE creature_group_waypoints
(
    group_id INT NOT NULL DEFAULT 0,
    point INT UNSIGNED NOT NULL DEFAULT 0,
    position_x FLOAT NOT NULL DEFAULT 0,
    position_y FLOAT NOT NULL DEFAULT 0,
    position_z FLOAT NOT NULL DEFAULT 0,
    orientation FLOAT NOT NULL NOT NULL DEFAULT 0,
    delay INT UNSIGNED DEFAULT 0,
    run BOOL DEFAULT FALSE
);

-- Wp Add
INSERT INTO command (name, security, help) VALUES("npc group gwp add", 2, "Syntax: .npc group gwp add #id (#delay).\n\nAdds a group waypoint for group with id. Delay is optional and makes the group pause at that point.");
-- Wp Move
INSERT INTO command (name, security, help) VALUES("npc group gwp move", 2, "Syntax: .npc group gwp move #id.\n\nTarget one of the npc's spawned by .npc group wp show and use this command to move that waypoint for the group with id.");
-- Wp Remove
INSERT INTO command (name, security, help) VALUES("npc group gwp remove", 2, "Syntax: .npc group gwp remove #id.\n\nTarget one of the npc's spawned by .npc group wp show and use this command to remove that waypoint from the group with id.");
-- Wp Show
INSERT INTO command (name, security, help) VALUES("npc group gwp show", 2, "Syntax: .npc group gwp show #id.\n\nShows waypoints for the group with that id.");
-- Wp Unshow
INSERT INTO command (name, security, help) VALUES("npc group gwp unshow", 2, "Syntax: .npc group gwp unshow #id.\n\nHides the waypoints for the group with that id.");
-- Wp Edit
INSERT INTO command (name, security, help) VALUES("npc group gwp edit", 2, "Syntax: .npc group gwp edit $edit_str #/$value.\n\nEdits npc (run and delay). Valid ways: .n g gw edit run on, .n g gw edit delay 4000");

-- Waypoint has entry 1 and 2 is free, so 2 seems pretty fitting
INSERT INTO creature_template(entry, modelid_1, name, subname, minlevel, maxlevel, minhealth, maxhealth, faction_A, faction_H, unit_flags,
    family, type_flags, MovementType, InhabitType, flags_extra)
    VALUES(2, 15332, "Group Waypoint", "Visual", 1, 1, 8, 8, 35, 35, 4096, 8, 5242886, 0, 7, 130);
