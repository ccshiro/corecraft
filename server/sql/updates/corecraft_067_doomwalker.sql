-- Mark of Death pulse. No Initial Aggro, Only Target Players, Ignore Invulnerability, cannot miss & cannot reflected
INSERT INTO spell_dbc (Id, EquippedItemClass, SpellName1, RangeIndex, DurationIndex, Attributes, AttributesEx2, AttributesEx3,
  Effect1, EffectApplyAuraName1, EffectImplicitTargetA1, EffectImplicitTargetB1, EffectRadiusIndex1)
VALUES(150018, -1, "Mark of Death Dummy Aura", 1, 8, 0x20000000, 0x4, 0x60100, 6, 4, 22, 15, 12);
INSERT INTO spell_los_ignore (id) VALUES(150018);
-- Negative flag to mark of death
UPDATE spell_dbc SET AttributesEx = AttributesEx | 0x80 WHERE Id=37128;
-- Earthquake knockdown. Not the correct spell but doesnt have a visual/appear in combat log, so all's fine
UPDATE spell_dbc SET EffectRadiusIndex2=12, EffectTriggerSpell2=39002 WHERE Id=32686;
-- Spore Quake Knockdown. Remove cooldown
UPDATE spell_dbc SET RecoveryTime=0 WHERE Id=39002;

INSERT INTO spell_los_ignore (id) VALUES(32686);
