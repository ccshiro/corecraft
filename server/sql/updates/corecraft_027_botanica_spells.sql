-- Patch by nIM

-- Got Invalid Target even though the spell script target was correct.
-- Set the values to 0 (no target) rather than 38 (script target).
UPDATE spell_dbc SET EffectImplicitTargetA1=0, EffectImplicitTargetA2=0, EffectImplicitTargetA3=0 where Id=34187;

-- Thorn Missiles are now cast on a target instead of self
UPDATE spell_dbc SET EffectImplicitTargetA1=6 where Id=35071;
UPDATE spell_dbc SET EffectImplicitTargetA1=6 where Id=34714;

-- Patch by shiro

-- Remove the Negative flag from all friendly Geomancer spells
UPDATE spell_dbc SET AttributesEx=AttributesEx & ~0x80 WHERE Id=34167 OR Id=34169 OR Id=34170 OR Id=34183 OR Id=34185;

-- Chaned SPELL_AURA_MOD_HASTE from Geomanncer's RP blizzard and RoF spell. Blizzard added it as a ghetto solution, but
-- it messes with mangos due to mangos IsPositive check failing. We make it a friendly dummy here instead
UPDATE spell_dbc SET EffectApplyAuraName2=4, EffectRadiusIndex2=19, EffectImplicitTargetA2=1, EffectImplicitTargetB2=0,
    EffectBaseDice2=1, EffectDieSides2=1, Effect2=27 WHERE Id=34167 OR Id=34169 OR Id=34183 OR Id=34185;

-- Sarannis double attack proccs a non-existant server spell, we create one that does what that one should've done
INSERT INTO spell_dbc (Id, CastingTimeIndex, procChance, rangeIndex, EquippedItemClass, Effect1, EffectDieSides1, EffectBaseDice1, 
    EffectBasePoints1, EffectImplicitTargetA1, DmgMultiplier1, DmgClass, PreventionType, SchoolMask, SpellName1)
    VALUES(18941, 1, 101, 1, -1, 19, 1, 1, 1, 1, 1, 2, 2, 1, "Double Attack Server-Side");

-- Arcane Flurry N & H have a dummy spell effect that doesn't exist
INSERT INTO spell_dbc (Id, CastingTimeIndex, procChance, EquippedItemClass, Effect1, EffectDieSides1, EffectBaseDice1, SpellName1)
    VALUES(34822, 1, 101, -1, 3, 1, 1, "Arcane Flurry Server-Side Dummy");
INSERT INTO spell_dbc (Id, CastingTimeIndex, procChance, EquippedItemClass, Effect1, EffectDieSides1, EffectBaseDice1, SpellName1)
    VALUES(37269, 1, 101, -1, 3, 1, 1, "Arcane Flurry Server-Side Dummy");
