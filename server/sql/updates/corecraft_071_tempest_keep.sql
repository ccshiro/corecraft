-- Trash fixes
-- 4 second duration on Golem Repair as well as single target
UPDATE spell_dbc SET DurationIndex=35, EffectImplicitTargetA1=21, EffectImplicitTargetB1=0 WHERE Id=34946;
-- 5 Max targets on Domination
UPDATE spell_dbc SET MaxAffectedTargets=5 WHERE Id=37135;
-- Frost Attack Trigger
INSERT INTO spell_dbc (Id, Attributes, CastingTimeIndex, procFlags, procChance, DurationIndex,
    rangeIndex, EquippedItemClass, Effect1, EffectBasePoints1, EffectImplicitTargetA1, EffectApplyAuraName1,
    EffectTriggerSpell1, SpellName1) 
VALUES (150021, 80, 1, 20, 40, 21, 1, -1, 6, 13, 1, 42, 39087, "Frost Attack Trigger");
-- Power up target script
UPDATE spell_dbc SET EffectImplicitTargetA1=38, EffectImplicitTargetA2=38, RangeIndex=5 WHERE Id=37112;
INSERT INTO spell_script_target(entry, type, targetEntry) VALUES (37112, 1, 20040), (37112, 1, 20041);
-- Add dummy spell for Recharge targeting
INSERT INTO spell_dbc (Id, rangeIndex, EquippedItemClass, Effect1, EffectImplicitTargetA1,
  EffectImplicitTargetB1, EffectRadiusIndex1, MaxAffectedTargets, SpellName1) 
VALUES (150022, 1, -1, 3, 22, 30, 12, 1, "Recharge Targeting");

-- Trash Nerfs
-- Arcane Buffet
UPDATE spell_dbc SET EffectBasePoints2=99 WHERE Id=37133;
-- Arcane Explosion
UPDATE spell_dbc SET EffectDieSides1=151, EffectBasePoints1=1424 WHERE Id=38725;
-- Arcane Shock
UPDATE spell_dbc SET EffectDieSides1=241, EffectBasePoints1=1079 WHERE Id=37132;
-- Arcane Volley
UPDATE spell_dbc SET EffectDieSides1=225, EffectBasePoints1=1387 WHERE Id=37129;
-- Frost Attack
UPDATE spell_dbc SET EffectDieSides1=2201 WHERE Id=39087;
-- Fire Nova
UPDATE spell_dbc SET EffectDieSides1=225, EffectBasePoints1=1387 WHERE Id=38728;
-- Fire Shield
UPDATE spell_dbc SET EffectDieSides1=41, EffectBasePoints1=379 WHERE Id=37283;
-- Fireball
UPDATE spell_dbc SET EffectDieSides1=601, EffectBasePoints1=1699 WHERE Id=37111;
-- Rain of Fire
UPDATE spell_dbc SET EffectDieSides1=225, EffectBasePoints1=1387 WHERE Id=37279;
-- Starfall
UPDATE spell_dbc SET DurationIndex=1, EffectAmplitude1=2000, EffectRadiusIndex1=13 WHERE Id=37124;
-- Fragmentation Bomb
UPDATE spell_dbc SET DurationIndex=9, EffectDieSides1=751, EffectBasePoints1=2124 WHERE Id=37120;
-- Shell Shock
UPDATE spell_dbc SET DurationIndex=35 WHERE Id=37118;
-- Countercharge was not removed and reapplied as it is currently
UPDATE spell_dbc SET procCharges=0, procFlags=0x4 | 0x10 WHERE Id=35035;
UPDATE spell_dbc SET procFlags=0x4 | 0x10, Effect2=0, EffectTriggerSpell2=0 WHERE Id=35781;


-- Boss Fixes
-- Wrath of the Astromancer (AoE only hit one target)
UPDATE spell_dbc SET MaxAffectedTargets=1 WHERE Id=33048;
-- Wrath of the Astromancer (Add a dummy aura to the DoT)
UPDATE spell_dbc SET Effect2=6, EffectApplyAuraName2=4, EffectImplicitTargetA2=25 WHERE Id=33045;
-- Remove Target Player flag from Dive Bomb
UPDATE spell_dbc SET AttributesEx3=AttributesEx3 & ~0x100 WHERE Id=35367;
-- Remove the force cast component of Ember of Al'ar (that is meant to do damage on Al'ar)
UPDATE spell_dbc SET Effect3=0, EffectImplicitTargetA3=0, EffectTriggerSpell3=0 WHERE Id=34133;
-- Add 2 ember summons to Dive Bomb
UPDATE spell_dbc SET Effect2=28, EffectBasePoints2=2, EffectImplicitTargetA2=6,
  EffectMiscValue2=19551, EffectMiscValueB2=64, EffectRadiusIndex2=13 WHERE Id=35181;
-- Make Resurrection not be negative
UPDATE spell_dbc SET AttributesEx = AttributesEx & ~0x80 WHERE Id=36450;
-- Gravity Lapse dummy effect
UPDATE spell_dbc SET Effect2=3, EffectImplicitTargetA2=1, AttributesEx3 = AttributesEx3 & ~0x100 WHERE Id=35941;
-- Gravity Lapse Negative
UPDATE spell_dbc SET AttributesEx = AttributesEx | 0x80 WHERE Id=34480;
-- Make pyroblast single targeted
UPDATE spell_dbc SET EffectImplicitTargetA1=6, EffectImplicitTargetB1=0 WHERE Id=36819;
-- Add a dummy to Flame Strike so we can despawn it when aura expires
UPDATE spell_dbc SET Effect3=6, EffectApplyAuraName3=4, EffectImplicitTargetA3=1 WHERE Id=36731;
-- Mind Control target 2 players
UPDATE spell_dbc SET MaxAffectedTargets=2 WHERE Id=36797;
-- Make Astromancer spells not cause aggro or threat (or they will bug with the dummy mob)
UPDATE spell_dbc SET AttributesEx = AttributesEx | 0x200, AttributesEx3 = AttributesEx3 | 0x20000 WHERE Id=33040;
  OR Id=33044 OR Id=33045 OR Id=33048 OR Id=33049;
-- Make wrath not spread outside of instance
UPDATE spell_dbc SET AreaId=3845 WHERE Id=33048;
-- Make remote toy not cause aggro or threat (can cause him to reattack if the fight is over)
UPDATE spell_dbc SET AttributesEx = AttributesEx | 0x200, AttributesEx3 = AttributesEx3 | 0x20000 WHERE Id=37027 OR Id=37029;

-- Boss Nerfs
-- Mark of Solarian
UPDATE spell_dbc SET EffectBasePoints1=-31 WHERE Id=33023;
-- Arcane Missiles
UPDATE spell_dbc SET EffectBasePoints1=3499 WHERE Id=39414;
-- Wrath of the Astromancer
UPDATE spell_dbc SET Effect3=2, EffectBasePoints1= 599, EffectBasePoints3=2399, EffectImplicitTargetA3=25 WHERE Id=33045;
-- Ember Blast
UPDATE spell_dbc SET EffectRadiusIndex1=18, EffectRadiusIndex2=18 WHERE Id=34341;
-- Dive Bomb
UPDATE spell_dbc SET EffectBasePoints1=142499, EffectDieSides1=15001 WHERE Id=35181;
-- Shock Barrier
UPDATE spell_dbc SET EffectBasePoints1=99999 WHERE Id=36815;

-- Quests
-- Increase radius of boar finding
UPDATE spell_dbc SET EffectRadiusIndex1=20 WHERE Id=36652;
-- Add a script effect to the plant spell
UPDATE spell_dbc SET Effect2=77, EffectMiscValue2=0, EffectImplicitTargetA2=1 WHERE Id=37062;
-- Fix Blood Elf Illusion spells
UPDATE spell_dbc SET Attributes = Attributes & ~0x80000000, Effect2=6, EffectApplyAuraName2=139, EffectMiscValue2=968,
  EffectBasePoints2=4, EffectImplicitTargetA2=1, AuraInterruptFlags=0x20022, AreaId=3822 WHERE Id=37092 OR Id=37094;
-- Redo utter spell
UPDATE spell_dbc SET Effect1=3, EffectMiscValue1=0, EffectImplicitTargetA1=38, RangeIndex=13 WHERE Id=37236;
