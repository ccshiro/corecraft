-- Sets the following proc flags for pyroclasm: do periodic, do spell, damage and negative
UPDATE `spell_dbc` SET `procFlags`=0x50000 WHERE `Id`=18073;
UPDATE `spell_dbc` SET `procFlags`=0x50000 WHERE `Id`=18096;
-- Add spell_proc_event if they do not exist or replace if they do
DELETE FROM spell_proc_event WHERE entry=18073 or entry=18096;
INSERT INTO spell_proc_event (entry, SpellFamilyName, SpellFamilyMask0, SpellFamilyMask1, SpellFamilyMask2) VALUES(18073, 5, 0x20 | 0x40 | 0x8000000000, 0x20 | 0x40 | 0x8000000000, 0x20 | 0x40 | 0x8000000000);
INSERT INTO spell_proc_event (entry, SpellFamilyName, SpellFamilyMask0, SpellFamilyMask1, SpellFamilyMask2) VALUES(18096, 5, 0x20 | 0x40 | 0x8000000000, 0x20 | 0x40 | 0x8000000000, 0x20 | 0x40 | 0x8000000000);
