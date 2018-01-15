-- Run patch in the mangos world db

-- Add Flee to mangos_strings
INSERT INTO mangos_string(entry, content_default) VALUES(5001, "%s attempts to run away in fear!");

-- Create the smart_scripts table
DROP TABLE IF EXISTS smart_scripts;
CREATE TABLE `smart_scripts` (
  `entryorguid` mediumint(11) NOT NULL,
  `source_type` mediumint(5) unsigned NOT NULL DEFAULT '0',
  `id` mediumint(11) unsigned NOT NULL DEFAULT '0',
  `link` mediumint(11) unsigned NOT NULL DEFAULT '0',
  `event_type` mediumint(5) unsigned NOT NULL DEFAULT '0',
  `event_phase_mask` mediumint(11) unsigned NOT NULL DEFAULT '0',
  `event_chance` mediumint(5) unsigned NOT NULL DEFAULT '100',
  `event_flags` mediumint(11) unsigned NOT NULL DEFAULT '0',
  `event_param1` mediumint(11) unsigned NOT NULL DEFAULT '0',
  `event_param2` mediumint(11) unsigned NOT NULL DEFAULT '0',
  `event_param3` mediumint(11) unsigned NOT NULL DEFAULT '0',
  `event_param4` mediumint(11) unsigned NOT NULL DEFAULT '0',
  `action_type` mediumint(5) unsigned NOT NULL DEFAULT '0',
  `action_param1` mediumint(11) unsigned NOT NULL DEFAULT '0',
  `action_param2` mediumint(11) unsigned NOT NULL DEFAULT '0',
  `action_param3` mediumint(11) unsigned NOT NULL DEFAULT '0',
  `action_param4` mediumint(11) unsigned NOT NULL DEFAULT '0',
  `action_param5` mediumint(11) unsigned NOT NULL DEFAULT '0',
  `action_param6` mediumint(11) unsigned NOT NULL DEFAULT '0',
  `target_type` mediumint(11) unsigned NOT NULL DEFAULT '0',
  `target_param1` mediumint(11) unsigned NOT NULL DEFAULT '0',
  `target_param2` mediumint(11) unsigned NOT NULL DEFAULT '0',
  `target_param3` mediumint(11) unsigned NOT NULL DEFAULT '0',
  `target_x` float NOT NULL DEFAULT '0',
  `target_y` float NOT NULL DEFAULT '0',
  `target_z` float NOT NULL DEFAULT '0',
  `target_o` float NOT NULL DEFAULT '0',
  `comment` varchar(255) NOT NULL DEFAULT '' COMMENT 'Event Comment',
  PRIMARY KEY (`entryorguid`,`source_type`,`id`,`link`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 ROW_FORMAT=FIXED;

-- Create table for smart AI waypoints
DROP TABLE IF EXISTS waypoints;
CREATE TABLE `waypoints` (
  `entry` mediumint(8) unsigned NOT NULL DEFAULT '0',
  `pointid` mediumint(8) unsigned NOT NULL DEFAULT '0',
  `position_x` float NOT NULL DEFAULT '0',
  `position_y` float NOT NULL DEFAULT '0',
  `position_z` float NOT NULL DEFAULT '0',
  `point_comment` text,
  PRIMARY KEY (`entry`,`pointid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 ROW_FORMAT=FIXED COMMENT='Creature waypoints';

-- CreatureTextMgr is a seperate system from smart scripts, but it is ported and only used for smart scripts atm
DROP TABLE IF EXISTS `creature_text`;
CREATE TABLE `creature_text` (
  `entry` mediumint(8) unsigned NOT NULL DEFAULT '0',
  `groupid` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `id` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `text` longtext,
  `type` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `language` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `probability` float NOT NULL DEFAULT '0',
  `emote` mediumint(8) unsigned NOT NULL DEFAULT '0',
  `duration` mediumint(8) unsigned NOT NULL DEFAULT '0',
  `sound` mediumint(8) unsigned NOT NULL DEFAULT '0',
  `comment` varchar(255) DEFAULT '',
  PRIMARY KEY (`entry`,`groupid`,`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
