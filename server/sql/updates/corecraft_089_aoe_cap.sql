-- Run in world

ALTER TABLE `spell_dbc` ADD COLUMN `aoe_cap` INT(11) UNSIGNED NOT NULL DEFAULT '0' AFTER `SchoolMask`;

-- Flamestrike
UPDATE spell_dbc SET aoe_cap = (EffectBasePoints1 + EffectDieSides1) * 13.61 WHERE(Id = 27086 OR Id = 10216 OR Id = 10215 OR Id = 8423 OR Id = 8422 OR Id = 2121 OR Id = 2120);
-- Blastwave
UPDATE spell_dbc SET aoe_cap = (EffectBasePoints1 + EffectDieSides1) * 13.03 WHERE(Id = 33933 OR Id = 27133 OR Id = 13021 OR Id = 11113 OR Id = 13020 OR Id = 13019 OR Id = 13018);
-- Arcane Explosion
UPDATE spell_dbc SET aoe_cap = (EffectBasePoints1 + EffectDieSides1) * 24.75 WHERE(Id = 27082 OR Id = 27080 OR Id = 10202 OR Id = 10201 OR Id = 8437 OR Id = 8439 OR Id = 8438 OR Id = 1449);
-- Dragon's Breath
UPDATE spell_dbc SET aoe_cap = (EffectBasePoints1 + EffectDieSides1) * 12.76 WHERE(Id = 33043 OR Id = 33042 OR Id = 33041 OR Id = 31661);
-- Cone of Cold
UPDATE spell_dbc SET aoe_cap = (EffectBasePoints2 + EffectDieSides2) * 14.47 WHERE(Id = 27087 OR Id = 10161 OR Id = 10160 OR Id = 10159 OR Id = 8492 OR Id = 120);
-- BLizzard
UPDATE spell_dbc SET aoe_cap = (EffectBasePoints1 + EffectDieSides1) * 23.67 WHERE(Id = 42198 OR Id = 42213 OR Id = 42212 OR Id = 42211 OR Id = 42210 OR Id = 42209 OR Id = 42208);
-- Rain of Fire
UPDATE spell_dbc SET aoe_cap = (EffectBasePoints1 + EffectDieSides1) * 22.11 WHERE(Id = 42218 OR Id = 42226 OR Id = 42225 OR Id = 42224 OR Id = 42223);
-- Hellfire
UPDATE spell_dbc SET aoe_cap = (EffectBasePoints1 + EffectDieSides1) * 17 WHERE(Id = 27214 OR Id = 11682 OR Id = 11681 OR Id = 5857);
-- Seed of Corruption
UPDATE spell_dbc SET aoe_cap = 13850 WHERE(Id = 27285 OR Id = 43991);
-- Magma Totem
+UPDATE spell_dbc SET aoe_cap = (EffectBasePoints1 + EffectDieSides1) * 15.97 WHERE(Id = 25550 OR Id = 10581 OR Id = 10580 OR Id = 10579 OR Id = 8187);
-- Holy Nova
UPDATE spell_dbc SET aoe_cap = (EffectBasePoints1 + EffectDieSides1) * 20.66 WHERE(Id = 25331 OR Id = 27801 OR Id = 27800 OR Id = 27799 OR Id = 15431 OR Id = 15430 OR Id = 15237);
-- Magma Totem
UPDATE spell_dbc SET aoe_cap = (EffectBasePoints1 + EffectDieSides1) * 25.24 WHERE(Id = 42230 OR Id = 42233 OR Id = 42232 OR Id = 42231);
