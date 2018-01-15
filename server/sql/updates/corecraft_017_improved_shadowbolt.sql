-- Sets charges to 5 even though it should be 4, this is because the first charge
-- is immediately consumed on proc, but gives no effect at all.
-- Setting the charges to 5 effectively makes them 4
UPDATE spell_dbc SET procCharges = 5 WHERE id = 17794;
UPDATE spell_dbc SET procCharges = 5 WHERE id = 17797;
UPDATE spell_dbc SET procCharges = 5 WHERE id = 17798;
UPDATE spell_dbc SET procCharges = 5 WHERE id = 17799;
UPDATE spell_dbc SET procCharges = 5 WHERE id = 17800;
