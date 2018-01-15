-- Drop battleground templates
DROP TABLE IF EXISTS `battleground_template`;

-- Update WSG buffs
-- Resto
UPDATE gameobject SET spawntimesecs=18 WHERE id=179904 AND map=489;
-- Berserker
UPDATE gameobject SET spawntimesecs=120 WHERE id=179905 AND map=489;
-- Speed
UPDATE gameobject SET spawntimesecs=120 WHERE id=179871 AND map=489;
