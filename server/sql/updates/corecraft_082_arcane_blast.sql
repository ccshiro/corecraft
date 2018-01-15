-- world

-- The debuff applied on yourself by Arcane Blast should have no casting time.
UPDATE spell_dbc SET CastingTimeIndex=0 WHERE Id=36032;
