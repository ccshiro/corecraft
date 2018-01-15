-- Paladin's Return Dmg on Block spells should not be able to crit (BoSanc and HS)

-- Blessing of Sanctuary should not be able to crit (SPELL_ATTR_EX2_CANT_CRIT)
UPDATE `spell_dbc` SET `AttributesEx2` = `AttributesEx2` | 0x20000000 WHERE `Id`=20911; -- Rank 1
UPDATE `spell_dbc` SET `AttributesEx2` = `AttributesEx2` | 0x20000000 WHERE `Id`=20912; -- Rank 2
UPDATE `spell_dbc` SET `AttributesEx2` = `AttributesEx2` | 0x20000000 WHERE `Id`=20913; -- Rank 3
UPDATE `spell_dbc` SET `AttributesEx2` = `AttributesEx2` | 0x20000000 WHERE `Id`=20914; -- Rank 4
UPDATE `spell_dbc` SET `AttributesEx2` = `AttributesEx2` | 0x20000000 WHERE `Id`=27168; -- Rank 5
-- Holy Shield should not be able to crit (SPELL_ATTR_EX2_CANT_CRIT)
UPDATE `spell_dbc` SET `AttributesEx2` = `AttributesEx2` | 0x20000000 WHERE `Id`=20925; -- Rank 1
UPDATE `spell_dbc` SET `AttributesEx2` = `AttributesEx2` | 0x20000000 WHERE `Id`=20927; -- Rank 2
UPDATE `spell_dbc` SET `AttributesEx2` = `AttributesEx2` | 0x20000000 WHERE `Id`=20928; -- Rank 3
UPDATE `spell_dbc` SET `AttributesEx2` = `AttributesEx2` | 0x20000000 WHERE `Id`=27179; -- Rank 4