-- mangosd
-- Dark Shell. Make it pre 2.1
UPDATE spell_dbc SET DurationIndex=31, CastingTimeIndex=1 WHERE Id=32358 OR Id=38759;

-- Death coil, remove friendly
UPDATE spell_dbc SET AttributesEx = AttributesEx | 0x80 WHERE Id=38065;
