-- Set Damage for Earth Elemental (And about 50% dmg reduction)
UPDATE creature_template SET attackpower=0, mindmg=80, maxdmg=120, minhealth=4200, maxhealth=4200, armor=10000 WHERE entry=15352;

-- And base damage for Greate Fire Elemental
UPDATE creature_template SET attackpower=0, mindmg=40, maxdmg=70, minhealth=2200, maxhealth=2200, minmana=2100, maxmana=2100 WHERE entry=15438;

-- Change the fire elemental's spells:
-- Fire Blast:
UPDATE spell_dbc SET manaCost=560, CategoryRecoveryTime=6000, StartRecoveryCategory=133, StartRecoveryTime=2500, EffectBasePoints1=400, EffectDieSides1=55 WHERE Id=25028;
-- Fire Nova (we took an unused one, as the "right one" was heavily in used)
UPDATE spell_dbc SET Attributes=65536,maxLevel=73, baseLevel=68, spellLevel=68, CastingTimeIndex=5, EffectBasePoints1=650, EffectDieSides1=105, EffectRealPointsPerLevel1=0, manaCost=490, CategoryRecoveryTime=6000, StartRecoveryCategory=133, StartRecoveryTime=2500, EffectRadiusIndex1=18 WHERE Id=30941;
-- Server side Fire shield aura
INSERT INTO spell_dbc (Id, procChance, maxLevel, baseLevel, spellLevel, DurationIndex, rangeIndex, EquippedItemClass, Effect1, EffectDieSides1, EffectBaseDice1, EffectBasePoints1, EffectImplicitTargetA1, EffectApplyAuraName1, EffectAmplitude1,EffectTriggerSpell1, SpellIconId, SpellName1, SpellName2, DmgClass, DmgMultiplier1, DmgMultiplier2, DmgMultiplier3, SchoolMask) VALUES (32983,101,73,68,68,21,4,-1,6,1,1,-1,1,23,3000,13376,1,'Fire Shield','Fire Elementals Fire Shield Aura',1,1065353216,1065353216,1065353216,4);
