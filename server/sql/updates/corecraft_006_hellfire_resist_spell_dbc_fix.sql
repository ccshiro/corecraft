-- Apply the SPELL_ATTR_EX3_CANT_MISS to the hellfire spell so the warlock can't resist his own hellfire
UPDATE spell_dbc SET AttributesEx3 = AttributesEx3 | 0x00040000 WHERE id=1949;  -- Rank 1
UPDATE spell_dbc SET AttributesEx3 = AttributesEx3 | 0x00040000 WHERE id=11683; -- Rank 2
UPDATE spell_dbc SET AttributesEx3 = AttributesEx3 | 0x00040000 WHERE id=11684; -- Rank 3
UPDATE spell_dbc SET AttributesEx3 = AttributesEx3 | 0x00040000 WHERE id=27213; -- Rank 4

