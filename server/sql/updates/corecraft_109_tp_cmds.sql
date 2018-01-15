-- world
DELETE FROM command WHERE name="go";
DELETE FROM command WHERE name="groupgo";
DELETE FROM command WHERE name="namego";
DELETE FROM command WHERE name="goname";
DELETE FROM command WHERE name="recall";
DELETE FROM command WHERE name LIKE "go %";
DELETE FROM command WHERE name LIKE "tele%";
DELETE FROM command WHERE name="tp";
DELETE FROM command WHERE name="bring";
DELETE FROM command WHERE name="tploc add";
DELETE FROM command WHERE name="tploc del";

UPDATE command SET name="lookup tploc" WHERE name="lookup tele";

ALTER TABLE creature ADD INDEX (id);
ALTER TABLE creature ADD INDEX (guid,id);
ALTER TABLE gameobject ADD INDEX (id);
ALTER TABLE gameobject ADD INDEX (guid,id);

INSERT INTO command(name, security, help) VALUES
("tp", 2, "Usage: .tp [options] [target] str...
  |cffFFFFFF|h-e|h|r Next argument in str is (creature, gobj) id
  |cffFFFFFF|h-g|h|r Next argument in str is (player, creature, gobj) DB GUID
  |cffFFFFFF|h-p|h|r Disabled partial matching for str...
  |cffFFFFFF|h-t=<player>|h|r Player to teleport; you if not present

Valid targets are (short inside of parenthesis):
1.player (|cffFFFFFF|hp|h|r)
2.location (|cffFFFFFF|hloc|h|r)
3.creature (|cffFFFFFF|hc|h|r)
4.gameobject (|cffFFFFFF|hgo|h|r)
5.coordinates (|cffFFFFFF|hcoords|h|r)

If no target is given, it will try targets in the above ordered list until a match was found. If target is coordinates, str... will be interpreted as: x y z [map] [o]."),
("bring", 2, "Usage: .bring [options] [name]
  |cffFFFFFF|h-g|h|r Bring everyone in your group, or if name is specified her group, or if name is an integer a group with that id

If name is left out, your target, or targets specified by options will be brought to you."),
("tploc add", 3, "Usage: .tploc add name

Add a teleporation location that can later be teleported to .tp loc name"),
("tploc del", 3, "Usage: .tploc del name

Remove a previously added teleporation location by name.");
