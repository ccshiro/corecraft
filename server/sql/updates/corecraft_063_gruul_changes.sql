-- Add dummy effect to Ground Slam
UPDATE spell_dbc SET Effect3=3, EffectImplicitTargetA3=1 WHERE Id=33525;

-- Make Player Shatter not cause combat
UPDATE spell_dbc SET AttributesEx3 = AttributesEx3 | 0x20000, AttributesEx = AttributesEx | 0x400 WHERE Id=33671;

-- Intimidating Roar Aura Interrupt Flag
UPDATE spell_dbc SET AuraInterruptFlags = 0x2 WHERE Id=16508;
