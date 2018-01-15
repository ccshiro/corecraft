--
-- Hellfire Warder Spell Nerfs
--
-- Shadow Bolt Volley
UPDATE spell_dbc SET CastingTimeIndex=4, EffectBasePoints1=1699, EffectDieSides1=601 WHERE Id=39175;
-- Rain of Fire
UPDATE spell_dbc SET EffectBasePoints1=2774, EffectDieSides1=451, EffectRadiusIndex1=13 WHERE Id=34435;
-- Shadow Burst
UPDATE spell_dbc SET Effect2=98, EffectBasePoints2=29, EffectDieSides2=1, EffectImplicitTargetA2=22,
  EffectImplicitTargetB2=15, EffectRadiusIndex2=14 WHERE Id=34436;
-- Shadow Word: Pain
UPDATE spell_dbc SET EffectBasePoints1=999 WHERE Id=34441;
-- Unstable Affliction
UPDATE spell_dbc SET EffectBasePoints1=499 WHERE Id=34439;

--
-- Hellfire Channeler Spell Nerfs
--
-- Shadow Bolt Volley
UPDATE spell_dbc SET CastingTimeIndex=4 WHERE Id=30510;
-- Soul Transfer
UPDATE spell_dbc SET EffectBasePoints1=29, EffectBasePoints2=29 WHERE Id=30531;
-- Burning Abyssal
UPDATE spell_dbc SET EffectBasePoints2=2624, EffectDieSides2=751 WHERE Id=30511;
-- Fire Blast (Abyssal's spell)
UPDATE spell_dbc SET EffectBasePoints1=1749, EffectDieSides1=501 WHERE Id=30512;

--
-- Magtheridon Spell Nerfs
--
-- Mind Exhaustion: 3 minutes
UPDATE spell_dbc SET DurationIndex=25 WHERE Id=44032;

--
-- Spell Fixes
--
-- Add a stun effect to the OOC shadow cage
UPDATE spell_dbc SET Effect2=6, EffectApplyAuraName2=12, EffectImplicitTargetA2=2 WHERE Id=30205;
-- Remove Spell Script Target effect from player's Shadow Grasp
UPDATE spell_dbc SET EffectImplicitTargetA1=1 WHERE Id=30410;
DELETE FROM spell_script_target WHERE entry=30410;
-- Blaze change target
UPDATE spell_dbc SET RangeIndex=6, EffectImplicitTargetA1=21, EffectImplicitTargetB1=0, Effect1=3 WHERE Id=30541;
-- Conflagration. No threat or aggro.
UPDATE spell_dbc SET AttributesEx = AttributesEx | 0x400, AttributesEx3 = AttributesEx3 | 0x20000 WHERE Id=30757;
-- Debris Range And Target
UPDATE spell_dbc SET RangeIndex=6, Targets=0x40 WHERE Id=30631;
-- Debris Range
UPDATE spell_dbc SET RangeIndex=6 WHERE Id=30632;
-- Soul Transfer Script Target
INSERT INTO spell_script_target (entry, type, targetEntry) VALUES(30531, 1, 17256);

--
-- LoS Ignoring
--
-- A lot of spells in this dungeon ignroes Line of Sight:
INSERT INTO spell_los_ignore (id) VALUES
  (34441),
  (34439),
  (39175),
  (34435),
  (30510),
  (30531),
  (30616),
  (30571),
  (36449);
