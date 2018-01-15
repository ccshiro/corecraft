
-- Engineering

-- Target dummies
-- General spell to make them lootable when they die, used for all the dummies
INSERT INTO spell_dbc(Id, EquippedItemClass, Effect1, EffectImplicitTargetA1, SpellName1)
VALUES (150036, -1, 3, 1, "Target Dummies' Material Return");
-- Masterwork
-- pet_template
INSERT INTO pet_template (cid, behavior, ctemplate_flags)
VALUES(12426, 0, 0x1 | 0x2 | 0x4 | 0x8 | 0x10 | 0x200 | 0x800);
-- creature_template
UPDATE creature_template SET AIName="SmartAI", minlevel=60, maxlevel=60, minhealth=5228, maxhealth=5228,
armor=3941, unit_class=1, lootid=12426, flags_extra=0x2 | 0x2000 WHERE entry=12426;
-- reference_loot_template
INSERT INTO reference_loot_template (entry, groupid, item, ChanceOrQuestChance, mincountOrRef, maxcount)
VALUES (50000, 1, 10561, 15, 1, 1), (50000, 1, 16000, 25, 1, 1), (50000, 1, 15994, 15, 2, 2),
(50000, 1, 6037, 10, 1, 1), (50000, 1, 8170, 20, 2, 2), (50000, 1, 14047, 15, 4, 4);
-- creature_loot_template
INSERT INTO creature_loot_template (entry, item, ChanceOrQuestChance, groupid, minCountOrRef, maxcount)
VALUES (12426, 50000, 100, 1, -50000, 3);
-- smart_scripts
INSERT INTO smart_scripts (entryorguid, id, event_type, action_type, action_param1, target_type, `comment`)
VALUES(12426, 0, 25, 20, 0, 1, "Reset | Disable Auto-Attack");
INSERT INTO smart_scripts (entryorguid, id, event_type, action_type, action_param1, target_type, `comment`)
VALUES(12426, 1, 25, 21, 0, 1, "Reset | Disable Combat Movement");
INSERT INTO smart_scripts (entryorguid, id, event_type, action_type, action_param1, action_param2, target_type, `comment`)
VALUES(12426, 2, 63, 11, 19809, 2, 1, "Spawn | Cast Passive");
INSERT INTO smart_scripts (entryorguid, id, event_type, event_flags, event_param1, event_param2, action_type, action_param1, action_param2, target_type, `comment`)
VALUES(12426, 3, 60, 1, 500, 500, 11, 4507, 2, 1, "Timer Once | Cast Spawn Taunt");
INSERT INTO smart_scripts (entryorguid, id, event_type, event_flags, event_param1, event_param2, action_type, action_param1, action_param2, target_type, `comment`)
VALUES(12426, 4, 60, 1, 1000, 1000, 11, 150036, 2, 1, "Timer Once | Cast Material Return");
INSERT INTO smart_scripts (entryorguid, id, event_type, event_flags, event_param1, event_param2, action_type, target_type, `comment`)
VALUES(12426, 5, 60, 1, 15000, 15000, 37, 1, "Timer Once | Die");

-- Target Dummy
-- pet_template
INSERT INTO pet_template (cid, behavior, ctemplate_flags)
VALUES(2673, 0, 0x1 | 0x2 | 0x4 | 0x8 | 0x10 | 0x200 | 0x800);
-- creature_template
UPDATE creature_template SET AIName="SmartAI", minlevel=20, maxlevel=20, minhealth=772, maxhealth=772,
armor=852, unit_class=1, lootid=2673, flags_extra=0x2 | 0x2000 WHERE entry=2673;
-- reference_loot_template
INSERT INTO reference_loot_template (entry, groupid, item, ChanceOrQuestChance, mincountOrRef, maxcount)
VALUES (50001, 1, 2841, 50, 2, 2), (50001, 1, 2592, 50, 2, 2);
-- creature_loot_template
INSERT INTO creature_loot_template (entry, item, ChanceOrQuestChance, groupid, minCountOrRef, maxcount)
VALUES (2673, 50001, 100, 1, -50001, 1);
-- smart_scripts
INSERT INTO smart_scripts (entryorguid, id, event_type, action_type, action_param1, target_type, `comment`)
VALUES(2673, 0, 25, 20, 0, 1, "Reset | Disable Auto-Attack");
INSERT INTO smart_scripts (entryorguid, id, event_type, action_type, action_param1, target_type, `comment`)
VALUES(2673, 1, 25, 21, 0, 1, "Reset | Disable Combat Movement");
INSERT INTO smart_scripts (entryorguid, id, event_type, action_type, action_param1, action_param2, target_type, `comment`)
VALUES(2673, 2, 63, 11, 4044, 2, 1, "Spawn | Cast Passive");
INSERT INTO smart_scripts (entryorguid, id, event_type, event_flags, event_param1, event_param2, action_type, action_param1, action_param2, target_type, `comment`)
VALUES(2673, 3, 60, 1, 500, 500, 11, 4507, 2, 1, "Timer Once | Cast Spawn Taunt");
INSERT INTO smart_scripts (entryorguid, id, event_type, event_flags, event_param1, event_param2, action_type, action_param1, action_param2, target_type, `comment`)
VALUES(2673, 4, 60, 1, 1000, 1000, 11, 150036, 2, 1, "Timer Once | Cast Material Return");
INSERT INTO smart_scripts (entryorguid, id, event_type, event_flags, event_param1, event_param2, action_type, target_type, `comment`)
VALUES(2673, 5, 60, 1, 15000, 15000, 37, 1, "Timer Once | Die");

-- Advanced Target Dummy
-- pet_template
INSERT INTO pet_template (cid, behavior, ctemplate_flags)
VALUES(2674, 0, 0x1 | 0x2 | 0x4 | 0x8 | 0x10 | 0x200 | 0x800);
-- creature_template
UPDATE creature_template SET AIName="SmartAI", minlevel=40, maxlevel=40, minhealth=2672, maxhealth=2672,
armor=2057, unit_class=1, lootid=2674, flags_extra=0x2 | 0x2000 WHERE entry=2674;
-- reference_loot_template
INSERT INTO reference_loot_template (entry, groupid, item, ChanceOrQuestChance, mincountOrRef, maxcount)
VALUES (50002, 1, 4387, 25, 1, 1), (50002, 1, 4382, 25, 1, 1), (50002, 1, 4389, 25, 1, 1),
(50002, 1, 4234, 25, 4, 4);
-- creature_loot_template
INSERT INTO creature_loot_template (entry, item, ChanceOrQuestChance, groupid, minCountOrRef, maxcount)
VALUES (2674, 50002, 100, 1, -50002, 2);
-- smart_scripts
INSERT INTO smart_scripts (entryorguid, id, event_type, action_type, action_param1, target_type, `comment`)
VALUES(2674, 0, 25, 20, 0, 1, "Reset | Disable Auto-Attack");
INSERT INTO smart_scripts (entryorguid, id, event_type, action_type, action_param1, target_type, `comment`)
VALUES(2674, 1, 25, 21, 0, 1, "Reset | Disable Combat Movement");
INSERT INTO smart_scripts (entryorguid, id, event_type, action_type, action_param1, action_param2, target_type, `comment`)
VALUES(2674, 2, 63, 11, 4048, 2, 1, "Spawn | Cast Passive");
INSERT INTO smart_scripts (entryorguid, id, event_type, event_flags, event_param1, event_param2, action_type, action_param1, action_param2, target_type, `comment`)
VALUES(2674, 3, 60, 1, 500, 500, 11, 4092, 2, 1, "Timer Once | Cast Spawn Taunt");
INSERT INTO smart_scripts (entryorguid, id, event_type, event_flags, event_param1, event_param2, action_type, action_param1, action_param2, target_type, `comment`)
VALUES(2674, 4, 60, 1, 1000, 1000, 11, 150036, 2, 1, "Timer Once | Cast Material Return");
INSERT INTO smart_scripts (entryorguid, id, event_type, event_flags, event_param1, event_param2, action_type, target_type, `comment`)
VALUES(2674, 5, 60, 1, 15000, 15000, 37, 1, "Timer Once | Die");

-- Compact Harvest Reaper
-- pet_template
INSERT INTO pet_template (cid, behavior, ctemplate_flags)
VALUES(2676, 0, 0x1 | 0x4 | 0x8 | 0x10 | 0x200 | 0x800);
-- creature_template
UPDATE creature_template SET AIName="SmartAI", minlevel=35, maxlevel=35, minhealth=2538, maxhealth=2538,
armor=1954, unit_class=1, lootid=2676, flags_extra=0x2 | 0x2000 WHERE entry=2676;
-- creature_model_info
UPDATE creature_model_info SET bounding_radius=0.8, combat_reach=5 WHERE modelid=1159;
-- creature_loot_template
DELETE FROM creature_loot_template WHERE entry=2676;
INSERT INTO creature_loot_template (entry, item, ChanceOrQuestChance, minCountOrRef, maxcount)
VALUES (2676, 7191, 100, 1, 3);
-- smart_scripts
INSERT INTO smart_scripts (entryorguid, id, event_type, action_type, action_param1, action_param2, target_type, `comment`)
VALUES(2676, 0, 25, 11, 150036, 2, 1, "Reset | Cast Material Return");
INSERT INTO smart_scripts (entryorguid, id, event_type, event_flags, event_param1, event_param2, action_type, target_type, `comment`)
VALUES(2676, 5, 60, 1, 10 * 60 * 1000, 10 * 60 * 1000, 37, 1, "Timer Once | Die");

-- Mechanical Dragonling
INSERT INTO pet_template (cid, behavior, ctemplate_flags) VALUES(2678, 0, 0x4 | 0x8 | 0x10 | 0x200);
UPDATE creature_template SET minlevel=40, maxlevel=40, minhealth=457, maxhealth=457, armor=3941, unit_class=1 WHERE entry=2678;
UPDATE creature_model_info SET bounding_radius=0.8, combat_reach=5 WHERE modelid=4465;

-- Mithril Mechanical Dragonling
INSERT INTO pet_template (cid, behavior, ctemplate_flags) VALUES(8615, 0, 0x4 | 0x8 | 0x10 | 0x200);
UPDATE creature_template SET minlevel=50, maxlevel=50, minhealth=664, maxhealth=664, armor=4429, unit_class=1 WHERE entry=8615;
UPDATE creature_model_info SET bounding_radius=0.8, combat_reach=5 WHERE modelid=7908;

-- Arcanite Dragonling
INSERT INTO pet_template (cid, behavior, ctemplate_flags) VALUES(12473, 0, 0x4 | 0x8 | 0x10 | 0x200);
UPDATE creature_template SET minlevel=60, maxlevel=60, minhealth=1494, maxhealth=1494, armor=4892, unit_class=1 WHERE entry=12473;
UPDATE creature_model_info SET bounding_radius=0.8, combat_reach=5 WHERE modelid=12489;

-- Explosive Sheep
INSERT INTO pet_template (cid, behavior, ctemplate_flags, pet_flags) VALUES(2675, 3, 0x4 | 0x8 | 0x10 | 0x200, 0x2);
UPDATE creature_template SET minlevel=30, maxlevel=30, minhealth=328, maxhealth=328, armor=1278, unit_class=1 WHERE entry=2675;
UPDATE creature_model_info SET bounding_radius=0.8, combat_reach=5 WHERE modelid=3886;
INSERT INTO petcreateinfo_spell (entry, Spell1, auto_cast1) VALUES(2675, 4050, 1);
UPDATE spell_dbc SET RangeIndex=2, Effect2=1 WHERE Id=4050;
UPDATE spell_dbc SET DurationIndex=25 WHERE Id=4074;

-- Gnomish Rocket Boots
UPDATE spell_dbc SET Effect2=6, EffectImplicitTargetA2=1, EffectAmplitude2=1000, EffectApplyAuraName2=226  WHERE Id=13141;
UPDATE spell_dbc SET AttributesEx = AttributesEx | 0x80 WHERE Id=13158;
-- Goblin Rocket Boots
UPDATE spell_dbc SET Effect2=6, EffectImplicitTargetA2=1, EffectAmplitude2=1000, EffectApplyAuraName2=226  WHERE Id=8892;

-- Rocket Boots Xtreme & Rocket Boots Xtreme Lite
UPDATE spell_dbc SET Category=0 WHERE Id=30452;

-- Gnomish Shrink Ray
UPDATE spell_dbc SET AttributesEx = AttributesEx | 0x80 WHERE Id=13003 OR Id=13004 OR Id=13010;

-- Gnomish Harm Prevention Belt
UPDATE spell_dbc SET Effect2=6, EffectApplyAuraName2=42, EffectTriggerSpell2=13235, EffectImplicitTargetA2=1 WHERE Id=13234;
UPDATE spell_dbc SET AttributesEx = AttributesEx | 0x80 WHERE Id=13235;

-- Goblin Rocket Launcher
UPDATE spell_dbc SET Effect2=64, EffectImplicitTargetA2=1, EffectTriggerSpell2=13360 WHERE Id=46567;

-- Goblin Bomb Dispenser
INSERT INTO pet_template (cid, behavior, ctemplate_flags, pet_flags) VALUES(8937, 3, 0x4 | 0x8 | 0x10 | 0x200, 0x2);
UPDATE creature_template SET minlevel=46, maxlevel=46, minhealth=2024, maxhealth=2024, armor=2468, unit_class=1 WHERE entry=8937;
UPDATE creature_model_info SET bounding_radius=0.8, combat_reach=5 WHERE modelid=8189;
INSERT INTO petcreateinfo_spell (entry, Spell1, auto_cast1) VALUES(8937, 13259, 1);
UPDATE spell_dbc SET RangeIndex=2, Effect2=1 WHERE Id=13259;

-- Gnomish Net-o-Matic Projector
UPDATE spell_dbc SET EffectTriggerSpell3=13138 WHERE Id=13119;
UPDATE spell_dbc SET EffectImplicitTargetA1=1 WHERE Id=13138;

-- Gnomish Battle Chicken
INSERT INTO pet_template (cid, behavior, ctemplate_flags, pet_flags) VALUES(8836, 0, 0x4 | 0x8 | 0x10 | 0x200, 0x2);
UPDATE creature_template SET minlevel=46, maxlevel=46, minhealth=2024, maxhealth=2024, armor=2468, unit_class=1, mindmg=46, maxdmg=72 WHERE entry=8836;
UPDATE creature_model_info SET bounding_radius=0.8, combat_reach=5 WHERE modelid=6909;
INSERT INTO petcreateinfo_spell (entry, Spell1, Spell2, auto_cast1, auto_cast2) VALUES(8836, 23060, 13168, 1, 1);
UPDATE spell_dbc SET RecoveryTime=20000 WHERE Id=13168;
UPDATE spell_dbc SET RecoveryTime=100000 WHERE Id=23060;

-- Goblin Dragon Gun
UPDATE spell_dbc SET Effect2=6, EffectApplyAuraName2=4, EffectImplicitTargetA2=1 WHERE Id=13183;

-- Crashin' Thrashin' Robot
UPDATE creature_template SET MovementType=0, AIName="SmartAI", unit_class=4,
minhealth=42, maxhealth=42, armor=0 WHERE entry=17299;
INSERT INTO creature_ai_spells (creature_id, spell_id, cooldown_min, cooldown_max)
VALUES (17299, 42382, 2000, 4000), (17299, 42372, 10000, 20000), (17299, 6533, 10000, 20000);
INSERT INTO smart_scripts (entryorguid, id, event_type, event_param1, event_param2, event_param3,
event_param4, action_type, target_type, target_param1, target_param2, target_param3, `comment`)
VALUES(17299, 0, 1, 1000, 1000, 1000, 1000, 49, 9, 17299, 0, 20, "OOC Timer -- Attack fellow Robot");
UPDATE spell_dbc SET Attributes = Attributes & ~0x80000 WHERE Id=42382;
UPDATE spell_dbc SET Attributes = Attributes & ~0x80000 WHERE Id=42372;

-- The Mortar: Reloaded
UPDATE spell_dbc SET Effect1=3 WHERE Id=13240;

-- Gnomish Flame Turret
INSERT INTO pet_template (cid, behavior, ctemplate_flags) VALUES(17458, 0, 0x1 | 0x2 | 0x4 | 0x8 | 0x10 | 0x200 | 0x800);
UPDATE creature_template SET minlevel=70, maxlevel=70, minhealth=1300, maxhealth=1300, armor=1500, unit_class=1, AIName="SmartAI", lootid=17458, flags_extra=0x2000 WHERE entry=17458;
INSERT INTO spell_dbc(Id, EquippedItemClass, Effect1, EffectImplicitTargetA1, SpellName1)
-- smart_scripts
VALUES (150037, -1, 3, 1, "Gnomish Flame Turret Handler");
INSERT INTO smart_scripts (entryorguid, id, event_type, action_type, action_param1, target_type, `comment`)
VALUES(17458, 0, 25, 114, 1, 1, "Reset | Disable Combat Reactions");
INSERT INTO smart_scripts (entryorguid, id, event_type, action_type, action_param1, target_type, `comment`)
VALUES(17458, 1, 25, 21, 0, 1, "Reset | Disable Combat Movement");
INSERT INTO smart_scripts (entryorguid, id, event_type, event_param1, event_param2, event_param3, event_param4, action_type, action_param1, action_param2, target_type, `comment`)
VALUES (17458, 2, 60, 500, 500, 500, 500, 11, 150037, 2, 1, "Timer | Cast Gnomish Flame Turret Handler");
INSERT INTO smart_scripts (entryorguid, id, event_type, event_flags, event_param1, event_param2, action_type, action_param1, action_param2, target_type, `comment`)
VALUES(17458, 3, 60, 1, 1000, 1000, 11, 150036, 2, 1, "Timer Once | Cast Material Return");
INSERT INTO smart_scripts (entryorguid, id, event_type, event_flags, event_param1, event_param2, action_type, target_type, `comment`)
VALUES(17458, 4, 60, 1, 45000, 45000, 37, 1, "Timer Once | Die");
-- reference_loot_template
INSERT INTO reference_loot_template (entry, groupid, item, ChanceOrQuestChance, mincountOrRef, maxcount)
VALUES (50003, 1, 23784, 15, 1, 1), (50003, 1, 23783, 35, 2, 2),
(50003, 1, 23781, 35, 3, 3), (50003, 1, 23782, 15, 1, 1);
-- creature_loot_template
INSERT INTO creature_loot_template (entry, item, ChanceOrQuestChance, groupid, minCountOrRef, maxcount)
VALUES (17458, 50003, 100, 1, -50003, 2);
-- spell_dbc
UPDATE spell_dbc SET DurationIndex=21 WHERE Id=30526;

-- Goblin Land Mine
INSERT INTO pet_template (cid, behavior, ctemplate_flags)
VALUES(7527, 0, 0x1 | 0x2 | 0x4 | 0x8 | 0x10 | 0x100 | 0x200);
-- creature_template
UPDATE creature_template SET AIName="SmartAI", unit_flags=0x2 | 0x2000000, unit_class=1, flags_extra=0x2 | 0x2000 WHERE entry=7527;
-- smart_scripts
INSERT INTO smart_scripts (entryorguid, id, event_type, action_type, action_param1, target_type, `comment`)
VALUES(7527, 0, 25, 114, 1, 1, "Reset | Disable Combat Reactions");
INSERT INTO smart_scripts (entryorguid, id, event_type, action_type, target_type, `comment`)
VALUES(7527, 1, 25, 101, 1, "Reset | Set Home Position");
INSERT INTO smart_scripts (entryorguid, id, event_type, event_flags, event_param1, event_param2, action_type, action_param1, target_type, `comment`)
VALUES(7527, 2, 60, 1, 10000, 10000, 22, 1, 1, "Timer Once | Arm trap (Set P1)");
INSERT INTO smart_scripts (entryorguid, id, link, event_type, event_phase_mask, event_flags, event_param1, event_param2, action_type, action_param1, action_param2, target_type, `comment`)
VALUES(7527, 3, 4, 10, 1, 1, 0, 8, 11, 4043, 2, 1, "P1: LoS | Cast Detonation");
INSERT INTO smart_scripts (entryorguid, id, link, event_type, action_type, action_param1, target_type, `comment`)
VALUES(7527, 4, 5, 61, 22, 0, 1, "Link | Set Phase 0");
INSERT INTO smart_scripts (entryorguid, id, event_type, event_param1, event_param2, action_type, action_param1, target_type, `comment`)
VALUES(7527, 5, 61, 1000, 1000, 41, 0, 1, "Delayed Link | Despawn");

-- Firework Launcher
UPDATE gameobject_template SET `type`=8, data0=1351 WHERE entry=180850;
UPDATE creature_template SET unit_flags=0x2 | 0x200 | 0x2000000 WHERE `name` LIKE "pat's firework guy%";

-- Gnomish Alarm-o-Bot
UPDATE creature_template SET AIName="SmartAI", unit_class=1, flags_extra=0x2 | 0x2000, minlevel=53, maxlevel=53, minhealth=491, maxhealth=491, lootid=14434 WHERE entry=14434;
INSERT INTO spell_dbc(Id, EquippedItemClass, Effect1, EffectImplicitTargetA1, SpellName1)
VALUES (150038, -1, 3, 1, "Alarm-o-Bot warning");
INSERT INTO pet_template (cid, behavior, ctemplate_flags, pet_flags)
VALUES(14434, 0, 0x1 | 0x4 | 0x8 | 0x10 | 0x200 | 0x800, 0x4);
UPDATE spell_dbc SET Effect3=3, EffectImplicitTargetA3=1, AttributesEx=AttributesEx | 0x400, AttributesEx3=AttributesEx3 | 0x20000 WHERE Id=23002;
UPDATE spell_dbc SET Attributes = Attributes & ~0x40 WHERE Id=23003;
UPDATE spell_dbc SET DurationIndex=21 WHERE Id=23004;
INSERT INTO petcreateinfo_spell (entry, Spell1, auto_cast1) VALUES(14434, 23003, 1);
-- smart_scripts
INSERT INTO smart_scripts (entryorguid, id, event_type, action_type, action_param1, target_type, `comment`)
VALUES(14434, 0, 25, 114, 1, 1, "Reset | Disable Combat Reactions");
INSERT INTO smart_scripts (entryorguid, id, event_type, event_param1, event_param3, event_param4, action_type, action_param1, target_type, `comment`)
VALUES(14434, 1, 8, 150038, 5000, 5000, 1, 1, 1, "SpellHit | Do Say");
INSERT INTO smart_scripts (entryorguid, id, event_type, event_flags, event_param1, event_param2, action_type, action_param1, action_param2, target_type, `comment`)
VALUES(14434, 2, 60, 1, 1000, 1000, 11, 150036, 2, 1, "Timer Once | Cast Material Return");
INSERT INTO smart_scripts (entryorguid, id, event_type, event_flags, event_param1, event_param2, action_type, target_type, `comment`)
VALUES(14434, 3, 60, 1, 10 * 60 * 1000, 10 * 60 * 1000, 37, 1, "Timer Once | Die");
-- creature_text
INSERT INTO creature_text (entry, groupid, id, text, `type`, probability)
VALUES(14434, 1, 1, "Warning! Warning! Stealthed intruder detected!", 12, 100);
-- reference_loot_template
INSERT INTO reference_loot_template (entry, groupid, item, ChanceOrQuestChance, mincountOrRef, maxcount)
VALUES (50004, 1, 12359, 20, 4, 4), (50004, 1, 15994, 25, 2, 2), (50004, 1, 8170, 25, 4, 4),
(50004, 1, 7910, 20, 1, 1), (50004, 1, 7191, 10, 1, 1);
-- creature_loot_template
INSERT INTO creature_loot_template (entry, item, ChanceOrQuestChance, groupid, minCountOrRef, maxcount)
VALUES (14434, 50004, 100, 1, -50004, 3);

-- Flash bomb, chance to fizzle when used against targets over level 60
UPDATE spell_dbc SET AttributesCustom=0x40 WHERE Id=5134;

-- Transporter Malfunction, negative aura
UPDATE spell_dbc SET AttributesEx=0x80 WHERE Id=23444;

-- Dimensional Ripper - Area 52
UPDATE creature_template SET modelid_1=20316, modelid_2=20319, modelid_3=20321, modelid_4=20322 WHERE entry=21490; -- Horde
UPDATE creature_template SET modelid_1=20317, modelid_2=20318, modelid_3=20320, modelid_4=20323 WHERE entry=21491; -- Alliance

-- Dimensional Ripper - Everlook
UPDATE creature_template SET modelid_1=26, modelid_2=33, modelid_3=0, modelid_4=0 WHERE entry=14681;

-- Goblin Rocket Helmet
-- the short stun
UPDATE spell_dbc SET Effect3=64, EffectImplicitTargetA3=1, EffectTriggerSpell3=13360 WHERE Id=13327;
-- chance to fizzle when used against targets over level 60
UPDATE spell_dbc SET AttributesCustom=0x40 WHERE Id=22641;

-- Gnomish Mind Control Cap
-- chance to fizzle when used against targets over level 60
UPDATE spell_dbc SET AttributesCustom=0x40 WHERE Id=13180;
-- remove the "can't target in combat" requirement on the triggered spell
UPDATE spell_dbc SET AttributesEx=0 WHERE Id=13181;

-- Steam Tonk Controller
-- split spell up into two, trigger second spell and move third effect (pacify) of first spell to second spell
-- also move casting time to first spell
UPDATE spell_dbc SET CastingTimeIndex=5, Effect3=64, EffectApplyAuraName3=0, EffectBasePoints3=0, EffectTriggerSpell3=45262 WHERE Id=45440;
UPDATE spell_dbc SET CastingTimeIndex=0, DurationIndex=5, Effect2=6, EffectImplicitTargetA2=1, EffectApplyAuraName2=25, EffectBasePoints2=4 WHERE Id=45262;
-- add spells to pet create info
INSERT INTO petcreateinfo_spell(entry, Spell1, Spell2, Spell3, Spell4) VALUES(19405, 24933, 25003, 25024, 27746);
-- remove OOC not attackable
UPDATE creature_template SET unit_flags=0 WHERE entry=19405;

-- Gnomish Death Ray
-- add a dummy aura to the death ray effect
UPDATE spell_dbc SET Effect3=6, EffectImplicitTargetA3=1, EffectApplyAuraName3=4 WHERE Id=13278;
-- create a death ray marker which will be used to remember the target as well as the damage it should inflict
INSERT INTO spell_dbc (Id, EquippedItemClass, RangeIndex, DurationIndex, Effect1, EffectApplyAuraName1, EffectImplicitTargetA1, SpellName1)
VALUES(150039, -1, 13, 31, 6, 4, 6, "Death Ray Marker");

-- Alchemy: Dream Vision
INSERT INTO spell_dbc(Id, EquippedItemClass, RangeIndex, DurationIndex, Effect1, EffectApplyAuraName1, EffectImplicitTargetA1,
Effect2, EffectApplyAuraName2, EffectImplicitTargetA2, EffectBasePoints2, SpellName1)
VALUES(150040, -1, 1, 21, 6, 105, 1, 6, 18, 1, 350, "Dream Vision Passive (DND)");

-- Jewelcrafting

-- Figurine - Truesilver Boar
UPDATE spell_dbc SET SpellFamilyName=0, EffectMiscValueB2=61, Effect3=3, EffectImplicitTargetA3=1 WHERE Id=26593;
-- Figurine - Felsteel Boar
UPDATE spell_dbc SET SpellFamilyName=0, Effect3=3, EffectImplicitTargetA3=1 WHERE Id=31038;
-- Figurine - Khorium Boar
UPDATE spell_dbc SET SpellFamilyName=0, Effect3=3, EffectImplicitTargetA3=1 WHERE Id=46782;

-- Figurines
UPDATE creature_template SET unit_flags=0x2 WHERE entry=15919 OR entry=15923 OR entry=15926
OR entry=15927 OR entry=15944 OR entry=15948 OR entry=15955 OR entry=15959 OR entry=17707 OR entry=17708
OR entry=17709 OR entry=17710 OR entry=26242 OR entry=26243 OR entry=26244 OR entry=26240;
INSERT INTO pet_template (cid, pet_flags, ctemplate_flags) VALUES (15919, 0x4, 0x100),
(15923, 0x4, 0x100), (15926, 0x4, 0x100), (15927, 0x4, 0x100), (15944, 0x4, 0x100),
(15948, 0x4, 0x100), (15955, 0x4, 0x100), (15959, 0x4, 0x100), (17707, 0x4, 0x100),
(17708, 0x4, 0x100), (17709, 0x4, 0x100), (17710, 0x4, 0x100), (26242, 0x4, 0x100),
(26243, 0x4, 0x100), (26244, 0x4, 0x100), (26240, 0x4, 0x100);
