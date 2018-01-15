-- Flame Arrow of gauntlet event made single target
UPDATE spell_dbc SET EffectImplicitTargetA1=6, EffectImplicitTargetB1=0 WHERE Id=30952;

-- Shadow Sear of Nethekurse's pre-event. Make it single target, reduce the duration & remove the initial delay
UPDATE spell_dbc SET EffectImplicitTargetA1=6, EffectImplicitTargetB1=0, speed=0, DurationIndex=28 WHERE Id=30735;

-- Shadow Fissure of Nethekurse fight. Add so it doesn't cause threat (the spell doesn't do the damage anyway so no need for threat)
UPDATE spell_dbc SET AttributesEx = AttributesEx | 0x400 WHERE Id=30496;

-- Dark Spin of Nethekurse fight. We stop the spell from casting the shadow ball and cast it in our script instead
UPDATE spell_dbc SET Effect1=0 WHERE Id=30502;

-- Death coil of Nethekurse fight shouldn't be friendly (removable by right clicking)
UPDATE spell_dbc SET EffectImplicitTargetA1=6, EffectImplicitTargetA2=6 WHERE Id=30500;

-- Shadow Fissure's Consumption spells. Change target type
UPDATE spell_dbc SET EffectImplicitTargetA1=22, EffectImplicitTargetB1=15 WHERE Id=35951 OR Id=32251;

-- Frightening Roar is actually a spell from OHF but Porung uses it as well, and the main component is broken (should be a stun), so we fix it here:
UPDATE spell_dbc SET EffectApplyAuraName1=12, AuraInterruptFlags=0x2 WHERE Id=29544 OR Id=38946;

-- Create server side aura for wound poison and crippling poison (Assassin's have those on)
INSERT INTO spell_dbc (Id, Category, Attributes, CastingTimeIndex, procFlags, procChance,
    baseLevel, spellLevel, DurationIndex, rangeIndex, EquippedItemClass, Effect1,
    EffectDieSides1, EffectBaseDice1, EffectImplicitTargetA1, EffectApplyAuraName1,
    EffectTriggerSpell1, SpellName1, DmgClass, PreventionType, SchoolMask)
    VALUES(150001, 65, 0x40050, 1, 20, 20,
    14, 14, 21, 1, -1, 6,
    1, 1, 1, 42,
    36974, "Wound Poison Aura", 2, 2, 1);
INSERT INTO spell_dbc (Id, Category, Attributes, CastingTimeIndex, procFlags, procChance,
    baseLevel, spellLevel, DurationIndex, rangeIndex, EquippedItemClass, Effect1,
    EffectDieSides1, EffectBaseDice1, EffectImplicitTargetA1, EffectApplyAuraName1,
    EffectTriggerSpell1, SpellName1, DmgClass, PreventionType, SchoolMask)
    VALUES(150002, 65, 0x40050, 1, 20, 20,
    14, 14, 21, 1, -1, 6,
    1, 1, 1, 42,
    30981, "Crippling Poison Aura", 2, 2, 1);

-- Make Prayer of Healing (Acolyte) have actual range so we can cast on nearby targets + Also make it AoE rather than target party
UPDATE spell_dbc SET rangeIndex=4 WHERE Id=15585 or Id=35943;

-- Make Sleep (visual) cancel when we move
UPDATE spell_dbc SET AuraInterruptFlags = AuraInterruptFlags | 0x8 WHERE Id=25148;

-- Copy PvP Trinket (with a new Id to avoid animation) for usage on hounds
INSERT INTO `spell_dbc` (`Id`, `Category`, `Dispel`, `Mechanic`, `Attributes`, `AttributesEx`, `AttributesEx2`, `AttributesEx3`, `AttributesEx4`, `AttributesEx5`, `AttributesEx6`, `Stances`, `StancesNot`, `Targets`, `TargetCreatureType`, `RequiresSpellFocus`, `FacingCasterFlags`, `CasterAuraState`, `TargetAuraState`, `CasterAuraStateNot`, `TargetAuraStateNot`, `CastingTimeIndex`, `RecoveryTime`, `CategoryRecoveryTime`, `InterruptFlags`, `AuraInterruptFlags`, `ChannelInterruptFlags`, `procFlags`, `procChance`, `procCharges`, `maxLevel`, `baseLevel`, `spellLevel`, `DurationIndex`, `powerType`, `manaCost`, `manaCostPerlevel`, `manaPerSecond`, `manaPerSecondPerLevel`, `rangeIndex`, `speed`, `StackAmount`, `Totem1`, `Totem2`, `Reagent1`, `Reagent2`, `Reagent3`, `Reagent4`, `Reagent5`, `Reagent6`, `Reagent7`, `Reagent8`, `ReagentCount1`, `ReagentCount2`, `ReagentCount3`, `ReagentCount4`, `ReagentCount5`, `ReagentCount6`, `ReagentCount7`, `ReagentCount8`, `EquippedItemClass`, `EquippedItemSubClassMask`, `EquippedItemInventoryTypeMask`, `Effect1`, `Effect2`, `Effect3`, `EffectDieSides1`, `EffectDieSides2`, `EffectDieSides3`, `EffectBaseDice1`, `EffectBaseDice2`, `EffectBaseDice3`, `EffectDicePerLevel1`, `EffectDicePerLevel2`, `EffectDicePerLevel3`, `EffectRealPointsPerLevel1`, `EffectRealPointsPerLevel2`, `EffectRealPointsPerLevel3`, `EffectBasePoints1`, `EffectBasePoints2`, `EffectBasePoints3`, `EffectMechanic1`, `EffectMechanic2`, `EffectMechanic3`, `EffectImplicitTargetA1`, `EffectImplicitTargetA2`, `EffectImplicitTargetA3`, `EffectImplicitTargetB1`, `EffectImplicitTargetB2`, `EffectImplicitTargetB3`, `EffectRadiusIndex1`, `EffectRadiusIndex2`, `EffectRadiusIndex3`, `EffectApplyAuraName1`, `EffectApplyAuraName2`, `EffectApplyAuraName3`, `EffectAmplitude1`, `EffectAmplitude2`, `EffectAmplitude3`, `EffectMultipleValue1`, `EffectMultipleValue2`, `EffectMultipleValue3`, `EffectChainTarget1`, `EffectChainTarget2`, `EffectChainTarget3`, `EffectItemType1`, `EffectItemType2`, `EffectItemType3`, `EffectMiscValue1`, `EffectMiscValue2`, `EffectMiscValue3`, `EffectMiscValueB1`, `EffectMiscValueB2`, `EffectMiscValueB3`, `EffectTriggerSpell1`, `EffectTriggerSpell2`, `EffectTriggerSpell3`, `EffectPointsPerComboPoint1`, `EffectPointsPerComboPoint2`, `EffectPointsPerComboPoint3`, `SpellVisual`, `SpellIconID`, `activeIconID`, `SpellName1`, `SpellName2`, `SpellName3`, `SpellName4`, `SpellName5`, `SpellName6`, `SpellName7`, `SpellName8`, `SpellName9`, `SpellName10`, `SpellName11`, `SpellName12`, `SpellName13`, `SpellName14`, `SpellName15`, `SpellName16`, `Rank1`, `Rank2`, `Rank3`, `Rank4`, `Rank5`, `Rank6`, `Rank7`, `Rank8`, `Rank9`, `Rank10`, `Rank11`, `Rank12`, `Rank13`, `Rank14`, `Rank15`, `Rank16`, `ManaCostPercentage`, `StartRecoveryCategory`, `StartRecoveryTime`, `MaxTargetLevel`, `SpellFamilyName`, `SpellFamilyFlags`, `MaxAffectedTargets`, `DmgClass`, `PreventionType`, `DmgMultiplier1`, `DmgMultiplier2`, `DmgMultiplier3`, `TotemCategory1`, `TotemCategory2`, `AreaId`, `SchoolMask`) values('150003','0','0','0','128','32768','0','0','0','393224','0','0','0','0','0','0','0','0','0','0','0','1','0','0','0','0','0','0','101','0','0','0','0','407','0','0','0','0','0','1','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','-1','0','0','6','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','1','0','0','0','0','0','0','0','0','77','0','0','0','0','0','0','0','0','0','0','0','0','0','0','1','0','0','0','0','0','0','0','0','0','0','0','86','74','0','Warhound Pvp Trinket','','','','','','','','','','','','','','','','','','','','','','','','','','','','','','','','0','0','0','0','0','0','0','0','0','1','1','1','0','0','0','1');

-- Make Blaze not do any threat (liquid fire's spell)
UPDATE spell_dbc SET AttributesEx = AttributesEx | 0x400, AttributesEx3 = AttributesEx3 | 0x20000 WHERE Id=23972 OR Id=32492;

-- Make Executioner debuffs negative
UPDATE spell_dbc SET AttributesEx = AttributesEx | 0x80 WHERE Id=39288 OR Id=39289 OR Id=39290;

-- Remove reflectability of some boss spells
UPDATE spell_dbc SET AttributesEx2 = AttributesEx2 | 0x4
    WHERE Id=30500 OR Id=30600 OR Id=30633 OR Id=30584;
