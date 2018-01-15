-- Make Arena Preparation (32728) non-removable (Aka, add SPELL_ATTR_CANT_CANCEL to attributes)
UPDATE `spell_dbc` SET `Attributes` = `Attributes` | 0x80000000 WHERE `Id`=32728;