-- Restore a radius we had changed a long time ago to its original state
UPDATE spell_dbc SET EffectRadiusIndex1=15 WHERE Id=31946;

-- Make Acid Spit single target
UPDATE spell_dbc SET EffectImplicitTargetA1=6, EffectImplicitTargetB1=0 WHERE Id=34290;

-- Make levitate single target
UPDATE spell_dbc SET EffectImplicitTargetA1=6, EffectImplicitTargetB1=0, EffectImplicitTargetA2=6, EffectImplicitTargetB2=0 WHERE Id=31704;

-- Make Spore Explosion Negative
UPDATE spell_dbc SET AttributesEx = AttributesEx | 0x80 WHERE Id=37966 OR Id=32327;

-- Fix fungal decay
UPDATE spell_dbc SET procFlags=0 WHERE Id=32065;
INSERT INTO spell_dbc (Id, EquippedItemClass, DurationIndex, Effect1, EffectImplicitTargetA1, Effect2, EffectApplyAuraName2, EffectImplicitTargetA2, SpellName1) VALUES(150004, -1, 39, 3, 6, 6, 4, 6, "Fungal Decay Dummy");
UPDATE spell_dbc SET Effect3=6, EffectApplyAuraName3=23, EffectImplicitTargetA3=6, EffectAmplitude3=3100, EffectTriggerSpell3=150004 WHERE Id=32065;

-- Chain Lighting not reflectable
UPDATE spell_dbc SET AttributesEx2 = AttributesEx2 | 0x04 WHERE Id=31717;
