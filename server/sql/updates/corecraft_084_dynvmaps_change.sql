-- World / Mangosd
ALTER TABLE `gameobject_template`
ADD COLUMN `vmap` TINYINT UNSIGNED NOT NULL DEFAULT '0'
AFTER `maxgold`;

-- Enable vmaps for all doors
UPDATE `gameobject_template` SET vmap = 1 WHERE `type` = 0;
