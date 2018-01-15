-- mangosd
-- Beta and Alpha channel have too short range
UPDATE spell_dbc SET RangeIndex=5 WHERE Id=36856 OR Id=36854;

-- Chaos Breath Frontal Cone
UPDATE spell_dbc SET EffectImplicitTargetA1=24, EffectRadiusIndex1=13 WHERE Id=36677;

-- Flaming Weapon Trigger Normal
INSERT INTO spell_dbc (Id, Attributes, CastingTimeIndex, procFlags, procChance, DurationIndex,
    rangeIndex, EquippedItemClass, Effect1, EffectBasePoints1, EffectImplicitTargetA1, EffectApplyAuraName1,
    EffectTriggerSpell1, SpellName1) 
VALUES (150008, 80, 1, 20, 20, 21, 1, -1, 6, 13, 1, 42, 36601, "Flaming Weapon Trigger");

-- Flaming Weapon Trigger Heroic
INSERT INTO spell_dbc (Id, Attributes, CastingTimeIndex, procFlags, procChance, DurationIndex,
    rangeIndex, EquippedItemClass, Effect1, EffectBasePoints1, EffectImplicitTargetA1, EffectApplyAuraName1,
	EffectTriggerSpell1, SpellName1) 
VALUES (150009, 80, 1, 20, 20, 21, 1, -1, 6, 13, 1, 42, 38804, "Flaming Weapon Trigger (1)");

-- Infected Blood had target self, should be chain
UPDATE spell_dbc SET EffectImplicitTargetA1=6 WHERE Id=36621 OR Id=38812;

-- Dispel Death Count dummy spell
INSERT INTO spell_dbc (Id, CastingTimeIndex, rangeIndex, EquippedItemClass, Effect1,
    EffectImplicitTargetA1, EffectImplicitTargetB1, EffectRadiusIndex1, SpellName1) 
VALUES (150010, 1, 6, -1, 3, 22, 15, 12, "Dispel Death Count");

-- Remove radius from Summon Negaton Field
UPDATE spell_dbc SET EffectImplicitTargetA1=1, EffectRadiusIndex1=0 WHERE Id=36813;

-- Set all npc Seed of Corruption spells to procCharges = 1
-- Note: I know it'd be better to keep this in a core sql patch, but the error
-- appeared while dealing with arcatraz, so I decided to put it in here against better knowing.
UPDATE spell_dbc SET procCharges=1 WHERE Id=32863
                                      OR Id=36123
                                      OR Id=38252
                                      OR Id=39367
                                      OR Id=44141;

-- Death Blast was nerfed in 2.1, this is the values from a 2.0.3 dbc
UPDATE spell_dbc SET EffectBaseDice1=1501, EffectBasePoints1=4249 WHERE Id=36662;
UPDATE spell_dbc SET EffectBaseDice1=3601, EffectBasePoints1=10199 WHERE Id=38819;

-- Domination was 8 seconds pre 2.1
UPDATE spell_dbc SET DurationIndex=31 WHERE Id=37162 OR Id=39019;

-- Eredar Deathbringer's Diminish Soul. Data from 2.0.3 dbc
UPDATE spell_dbc SET EffectBasePoints1=149 WHERE Id=36789;
UPDATE spell_dbc SET EffectBasePoints1=299 WHERE Id=38848;

-- Chastise heroic pre 2.1
UPDATE spell_dbc SET EffectBaseDice1=451, EffectBasePoints1=4274 WHERE Id=38851;

-- War Stomp pre 2.1
UPDATE spell_dbc SET EffectBaseDice3=325, EffectBasePoints3=2337 WHERE Id=36835;
UPDATE spell_dbc SET EffectBaseDice3=876, EffectBasePoints3=2354 WHERE Id=38911;

-- Soul Chill pre 2.1
UPDATE spell_dbc SET EffectBaseDice1=337, EffectBasePoints1=2081 WHERE Id=36786;
UPDATE spell_dbc SET EffectBaseDice1=601, EffectBasePoints1=3699 WHERE Id=38843;

-- Energy Discharge pre 2.1
UPDATE spell_dbc SET EffectBaseDice1=437, EffectBasePoints1=656 WHERE Id=36717;
UPDATE spell_dbc SET EffectBaseDice1=901, EffectBasePoints1=1349 WHERE Id=38829;

-- Lightning Discharge pre 2.1
UPDATE spell_dbc SET EffectBaseDice1=181, EffectBasePoints1=1109 WHERE Id=36915;
UPDATE spell_dbc SET EffectBaseDice1=375, EffectBasePoints1=2312 WHERE Id=39028;

-- Berserker Charge pre 2.1
UPDATE spell_dbc SET EffectDieSides3=1, EffectBasePoints3=2649 WHERE Id=36833;

-- Soul Steal
UPDATE spell_dbc SET Effect3=64, EffectImplicitTargetA3=1, EffectTriggerSpell3=36782 WHERE Id=36778;

-- Hex
UPDATE spell_dbc SET AuraInterruptFlags=AuraInterruptFlags | 0x2 WHERE Id=36700;
