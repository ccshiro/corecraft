-- world / mangosd
DROP TABLE IF EXISTS `warden_check`;
CREATE TABLE `warden_check` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `check` int(3) DEFAULT NULL,
  `data` tinytext,
  `str` tinytext,
  `address` int(8) DEFAULT NULL,
  `length` int(2) DEFAULT NULL,
  `result` tinytext,
  `action` int(3) DEFAULT '0' NOT NULL,
  `comment` text,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- Insert Data into Warden
-- Actions are:
-- 0 - log, 1 - kick, 2 - ban wave, 3 - instant ban
INSERT INTO `warden_check` (`check`, `data`, `str`, `address`, `length`, `result`, `action`, `comment`) VALUES
(243,'','',0x0049DBA0,2,'558B',2,'lua protection'),
(243,'','',0x0089060B,5,'E04D62503F',3,'wowemuhacker - hyper mode'),
(243,'','',0x008C845B,5,'C0854A3340',3,'wowemuhacker - gravity'),
(243,'','',0x007B98DE,2,'7541',3,'wowemuhacker - air jump'),
(243,'','',0x007BA4C0,5,'8B4F7C894E',2,'wowemuhacker - no fall damage'),
(243,'','',0x007B88D5,3,'894808',3,'wowemuhacker - fly to pane'),
(243,'','',0x008C8398,4,'BB8D243F',3,'wowemuhacker - wall climb'),
(243,'','',0x0048A2F0,2,'721B',2,'wowemuhacker - all languages'),
(217,'','WPESPY.DLL',0,0,'',2,'WpeSpy.dll');
