-- world

ALTER TABLE spell_dbc ADD COLUMN
bounce_radius INT UNSIGNED NOT NULL DEFAULT 0;

UPDATE spell_dbc SET bounce_radius=10 WHERE
EffectChainTarget1 > 1 OR 
EffectChainTarget2 > 1 OR 
EffectChainTarget3 > 1;

-- Black Stalker's CL (we know this one reaches longer)
UPDATE spell_dbc SET bounce_radius=30 WHERE Id=31717;
