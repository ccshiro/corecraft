-- Warlord's rage fixes
UPDATE spell_dbc SET AttributesEx = AttributesEx & ~0x04 WHERE Id=37081;
UPDATE spell_dbc SET RangeIndex = 13 WHERE Id=37076;
UPDATE spell_dbc SET EffectAmplitude2=2900 WHERE Id=31543;
