-- world

ALTER TABLE `spell_proc_item_enchant`
ADD COLUMN `flags` INT(11) UNSIGNED NOT NULL DEFAULT '0';

-- Windfury totem (which has 0 PPM, but restricted proc opportunities)
INSERT INTO spell_proc_item_enchant (entry, ppmRate, flags) VALUES
(8516, 0, 0x1 | 0x4), (10608, 0, 0x1 | 0x4), (10610, 0, 0x1 | 0x4),
(25583, 0, 0x1 | 0x4), (25584, 0, 0x1 | 0x4);
