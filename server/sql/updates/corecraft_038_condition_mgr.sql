-- Only the SmartAI part of the condition manager is used, and furthermore we give it another name
-- until/if we decided to completely port it over, at which time we will need to change its name back to just "conditions"
CREATE TABLE `trinity_conditions` (
  `SourceTypeOrReferenceId` mediumint(8) NOT NULL DEFAULT '0',
  `SourceGroup` mediumint(8) unsigned NOT NULL DEFAULT '0',
  `SourceEntry` int(11) NOT NULL DEFAULT '0',
  `SourceId` int(11) NOT NULL DEFAULT '0',
  `ElseGroup` mediumint(8) unsigned NOT NULL DEFAULT '0',
  `ConditionTypeOrReference` mediumint(8) NOT NULL DEFAULT '0',
  `ConditionTarget` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `ConditionValue1` mediumint(8) unsigned NOT NULL DEFAULT '0',
  `ConditionValue2` mediumint(8) unsigned NOT NULL DEFAULT '0',
  `ConditionValue3` mediumint(8) unsigned NOT NULL DEFAULT '0',
  `NegativeCondition` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `ErrorTextId` mediumint(8) unsigned NOT NULL DEFAULT '0',
  `ScriptName` char(64) NOT NULL DEFAULT '',
  `Comment` varchar(255) DEFAULT NULL
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COMMENT='Trinity Condition System';

-- TC has done some changes, here's the list:
-- @@09196_world_conditions.sql
-- Add ScriptName after ErrorTextId (ADDED)
-- 01_29_00_ && 01_30_00 && 03_27_00_wordl_condtions.sql ADD & Change SourceId (APPLIED ALL)
-- 01_30_00 Dropped PRIMARY KEY (ADDED)
-- 02_10_00 Add NegativeCondition column (ADDED)
-- 02_10_04 Add ConditionTarget column (ADDED)

-- I did the following change: SourceEntry is now a signed int rather than unsigned mediumint; or else guids for smart AI won't work
