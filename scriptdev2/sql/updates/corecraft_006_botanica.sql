-- Fix Falconer
UPDATE creature_template SET ScriptName="mob_bloodwarder_falconer" WHERE entry=17994;
-- And their spawn positions
UPDATE creature SET position_x=-34.7252, position_y=287.805, position_z=-1.84731, orientation=2.18113 WHERE guid=82995;
UPDATE creature SET position_x=104.093, position_y=282.485, position_z=-6.8537, orientation=0.53428 WHERE guid=83007;


-- Fix falcons
UPDATE creature_template SET MovementType=0, speed_walk=3.7, speed_run=1.71 WHERE entry=18155;
UPDATE creature_template SET MovementType=0, speed_walk=3.7, speed_run=1.71 WHERE entry=21544;
-- Remove already existing falcons (we're spawning them in the script)
DELETE FROM creature WHERE id=18155;


-- Add a ghetto fix controller to Thorngrin's mmaps problem. TODO: Remove this if we end up with a proper fix sometime
INSERT INTO creature_template (entry, modelid_1, modelid_3, name, minlevel, maxlevel, minhealth, maxhealth,
    faction_A, faction_H, type, flags_extra, ScriptName)
    VALUES(81001, 3277, 3277, "Thorngrin ghetto mmaps", 1, 1, 1, 1, 35, 35, 10, 130, "thorngrin_mmaps_ghetto_fix");
INSERT INTO creature (guid, id, map, spawnMask, position_x, position_y, position_z, orientation)
    VALUES(70000, 81001, 553, 3, 5.03811, 593.451, -15.1414, 4.68254);

-- Bloodwarder Protector
UPDATE creature_template SET ScriptName="mob_bloodwarder_protector" WHERE entry=17993;