-- world

-- Add dummy aura to combustion stack spell
UPDATE spell_dbc SET Effect2=6, EffectApplyAuraName2=4, EffectImplicitTargetA2=1 WHERE Id=28682;

-- Combustion hidden effect, make it not dispeltype magic.
UPDATE spell_dbc SET Dispel=0 WHERE Id=11129;

