-- Add dummy effect to Serpentshrine Parasite AoE
UPDATE spell_dbc SET Effect2=6, EffectImplicitTargetA2=6, EffectApplyAuraName2=4, EffectChainTarget2=5 WHERE Id=39044;
-- Add dummy effect to Serpentshrine Parasite Single
UPDATE spell_dbc SET Effect2=6, EffectImplicitTargetA2=6, EffectApplyAuraName2=4 WHERE Id=39053;
-- Change AreaId Serpentshrine Parasite. Blizzard set it to the wrong one (which is probably why their spell stopped working...)
UPDATE spell_dbc SET AreaId=3607 WHERE Id=39045;

-- Blue Mushroom Dummy
INSERT INTO spell_dbc (Id, EquippedItemClass, Effect1, EffectImplicitTargetA1, SpellName1)
  VALUES(150019, -1, 3, 1, "Refreshing Mist Primer");
  
-- Atrophic Blow dummy
INSERT INTO spell_dbc (Id, EquippedItemClass, Effect1, EffectApplyAuraName1, EffectTriggerSpell1,
  EffectImplicitTargetA1, SpellName1, procChance, procFlags, DurationIndex)
  VALUES(150020, -1, 6, 42, 39015, 1, "Atrophic Blow Aura", 101, 0x14, 21);

-- Spore Quake Knockdown
UPDATE spell_dbc SET Effect2=6, EffectApplyAuraName2=23, EffectImplicitTargetA2=22, EffectImplicitTargetB2=15,
  EffectTriggerSpell2=39002, EffectAmplitude2=2000, EffectRadiusIndex2=10 WHERE Id=38976;

-- Holy Fire
UPDATE spell_dbc SET EffectBaseDice1=953, EffectBasePoints1=3523 WHERE Id=38585;

-- Poison Bolt Volley
UPDATE spell_dbc SET EffectBaseDice1=625, EffectBasePoints1=2187 WHERE Id=38655;

-- Spore Cloud 
UPDATE spell_dbc SET EffectRadiusIndex1=17 WHERE Id=38653;

-- Spore Cloud adding back removed effect
UPDATE spell_dbc SET Effect3=27, EffectApplyAuraName3=3, EffectAmplitude3=2000, EffectBasePoints3=499,
  EffectBaseDice3=1, EffectRealPointsPerLevel3=10, EffectImplicitTargetA3=22, EffectImplicitTargetB3=15,
  EffectRadiusIndex1=13, EffectRadiusIndex2=13, EffectRadiusIndex3=13 WHERE Id=38652;

-- Fire Vulnerability, Nature Vulnerability, Frost Vulnerability
UPDATE spell_dbc SET EffectBasePoints1=14, EffectBasePoints2=49 WHERE Id=38715;
UPDATE spell_dbc SET EffectBasePoints1=14, EffectBasePoints2=49 WHERE Id=38714;
UPDATE spell_dbc SET EffectBasePoints1=14, EffectBasePoints2=49 WHERE Id=38717;

-- Frightening Shout
UPDATE spell_dbc SET AuraInterruptFlags=2, EffectApplyAuraName1=12 WHERE Id=38946;

-- Spore Quake had a 12 second duration before.
UPDATE spell_dbc SET DurationIndex=29 WHERE Id=38976;

-- Virulent Poison
UPDATE spell_dbc SET StackAmount=20, EffectBasePoints1=849 WHERE Id=39029;

-- Blue Beam
UPDATE spell_dbc SET EffectImplicitTargetA1=38, EffectImplicitTargetB1=0, EffectRadiusIndex1=0 WHERE Id=38015;
INSERT INTO spell_script_target VALUES(38015, 1, 21216);

-- Trigger the spout proc from the spout aura
UPDATE spell_dbc SET EffectTriggerSpell1=37433 WHERE Id=37430;

-- Change casted Spout to a dummy instead of script effect
UPDATE spell_dbc SET Effect1=3 WHERE Id=37431;

-- Make the spout proc a narrow frontal cone
UPDATE spell_dbc SET EffectImplicitTargetA1=60, EffectImplicitTargetA2=60 WHERE Id=37433;

-- Remove Mechanic of Leotheras RP banish
UPDATE spell_dbc SET Mechanic=0 WHERE Id=37546;

-- Chaos Blast Negative
UPDATE spell_dbc SET AttributesEx = AttributesEx | 0x80, EffectBasePoints1=99, EffectBaseDice1=1 WHERE Id=37675;

-- Green Beam script target
INSERT INTO spell_script_target VALUES(37626, 1, 21215);

-- Banish Summon Spell
INSERT INTO spell_dbc (Id, EquippedItemClass, SpellName1, Effect1, EffectImplicitTargetA1)
  VALUES(37545, -1, "Phantom Leotheras", 3, 1);

-- Bestial Wrath targets
INSERT INTO spell_script_target VALUES (38371, 1, 22119), (38371, 1, 22120);

-- Make all Watery Graves dummy auras instead. And trigger explosion when they expire in the core.
UPDATE spell_dbc SET EffectTriggerSpell3=0, EffectApplyAuraName3=4, EffectAmplitude3=0
  WHERE Id=37850 OR Id=38023 OR Id=38024 OR Id=38025;
  
-- Make Watery Grave Explosion not scale with Damage Done
UPDATE spell_dbc SET AttributesEx3 = AttributesEx3 | 0x20000000 WHERE Id=37852;

-- LoS ignoring spells
INSERT INTO spell_los_ignore VALUES
  (37138),
  (37676),
  (37764),
  (37850),
  (38023),
  (38024),
  (38025),
  (37854),
  (37858),
  (37860),
  (37861),
  (38145),
  (38253),
  (37749);

-- Watery Tomb
UPDATE spell_dbc SET DurationIndex=28, EffectRadiusIndex1=14, EffectRadiusIndex2=14 WHERE Id=38235;

-- Whirl
UPDATE spell_dbc SET EffectRadiusIndex1=10, EffectRadiusIndex2=10 WHERE id=37363;

-- Chaos Blast
UPDATE spell_dbc SET EffectRadiusIndex1=18 WHERE id=37674;

-- Earth Quake
UPDATE spell_dbc SET EffectDieSides1=675, EffectBasePoints1=4162 WHERE id=37764;

-- Watery Grave Explosion
UPDATE spell_dbc SET EffectRadiusIndex1=19 WHERE id=37852;

-- Multi-Shot
UPDATE spell_dbc SET EffectDieSides1=1351, EffectBasePoints1=8324 WHERE id=38310;

-- Toxic Spores
UPDATE spell_dbc SET Effect1=2, EffectDieSides1=451, EffectBaseDice1=1, EffectBasePoints1=2774, EffectImplicitTargetA1=18, EffectImplicitTargetB1=31, EffectRadiusIndex1=8 WHERE id=38574;

-- The Mark of Vashj (LOS)
INSERT INTO spell_los_ignore VALUES(39145);

-- Scalding Water
UPDATE spell_dbc SET EffectBasePoints1=999, EffectBasePoints2=999, EffectAmplitude1=2000 WHERE id=37284;

-- Submerge
UPDATE spell_dbc SET CastingTimeIndex=6 WHERE Id=37745;
