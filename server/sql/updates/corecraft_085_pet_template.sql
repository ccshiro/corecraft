-- World

DROP TABLE IF EXISTS `pet_template`;
CREATE TABLE `pet_template`
(
`cid` INT UNSIGNED NOT NULL,
`behavior` INT UNSIGNED NOT NULL DEFAULT '0',
`ctemplate_flags` INT UNSIGNED NOT NULL DEFAULT '0',
`pet_flags` INT UNSIGNED NOT NULL DEFAULT '0',
`behavior_flags` INT UNSIGNED NOT NULL DEFAULT '0',
`spell_dist` FLOAT NOT NULL DEFAULT '0',
`spell_oom` INT NOT NULL DEFAULT '0',
`spell_immunity` INT UNSIGNED NOT NULL DEFAULT '0',
PRIMARY KEY(`cid`)
);

-- Add auto-cast columns to petcreateinfo_spell which allows spells to be auto-casted by default
ALTER TABLE `petcreateinfo_spell` ADD COLUMN `auto_cast1` TINYINT UNSIGNED NOT NULL DEFAULT '0';
ALTER TABLE `petcreateinfo_spell` ADD COLUMN `auto_cast2` TINYINT UNSIGNED NOT NULL DEFAULT '0';
ALTER TABLE `petcreateinfo_spell` ADD COLUMN `auto_cast3` TINYINT UNSIGNED NOT NULL DEFAULT '0';
ALTER TABLE `petcreateinfo_spell` ADD COLUMN `auto_cast4` TINYINT UNSIGNED NOT NULL DEFAULT '0';

-- Warlock's imp
INSERT INTO `pet_template` (`cid`, `behavior`, `spell_dist`, `spell_oom`)
VALUES(416, 2, 30, -3110);

-- Mage's Water Elemental
INSERT INTO `pet_template` (`cid`, `behavior`, `spell_dist`, `spell_oom`, `spell_immunity`)
VALUES(510, 2, 35, -31707, 0x410);
UPDATE `petcreateinfo_spell` SET auto_cast1 = 1 WHERE entry=510;

-- Shaman's Earth Elemental
INSERT INTO `pet_template` (`cid`, `behavior`, `spell_immunity`, `pet_flags`)
VALUES(15352, 0, 0x8, 0xB);
DELETE FROM `petcreateinfo_spell` WHERE entry=15352;
INSERT INTO `petcreateinfo_spell` (`entry`, `Spell1`, `auto_cast1`)
VALUES(15352, 36213, 1);

-- Shaman's Fire Elemental
INSERT INTO `pet_template` (`cid`, `behavior`, `spell_immunity`, `pet_flags`)
VALUES(15438, 0, 0x4, 0xB);
DELETE FROM `petcreateinfo_spell` WHERE entry=15438;
INSERT INTO `petcreateinfo_spell` (`entry`, `Spell1`, `Spell2`, `Spell3`, `auto_cast1`, `auto_cast2`)
VALUES(15438, 30941, 25028, 32983, 1, 1);
-- Make a new Fire Shield spell for the Fire Elemental, the previously made one wasn't fully working
DELETE FROM spell_dbc WHERE Id=32983;
INSERT INTO spell_dbc (Id, Attributes, CastingTimeIndex, baseLevel, maxLevel, spellLevel, DurationIndex, rangeIndex,
EquippedItemClass, Effect1, EffectImplicitTargetA1, EffectApplyAuraName1, EffectAmplitude1, EffectTriggerSpell1, SpellIconId,
SpellName1)
VALUES(32983, 0x40, 1, 68, 73, 68, 21, 1, -1, 6, 1, 23, 3000, 13376, 1, "Fire Elemental's Fire Shield");

-- Hunter's Viper (Snake Trap)
DELETE FROM `petcreateinfo_spell` WHERE entry=19921;
INSERT INTO `petcreateinfo_spell` (`entry`, `Spell1`, `Spell2`, `Spell3`, `auto_cast1`, `auto_cast2`, `auto_cast3`)
VALUES(19921, 30981, 25810, 34655, 1, 1, 1);

-- Warlock's Eye of Kilrogg
INSERT INTO `pet_template` (`cid`, `pet_flags`, `ctemplate_flags`)
VALUES(4277, 0x4, 0x1C);
INSERT INTO `petcreateinfo_spell` (`entry`, `Spell1`, `auto_cast1`) VALUES (4277, 2585, 1);
UPDATE `creature_template` SET minhealth=1, maxhealth=1, minmana=0, maxmana=0, armor=0 WHERE entry=4277;
