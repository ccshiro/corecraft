-- mangosd

-- Containment Beam: no aggro
UPDATE spell_dbc SET AttributesEx = AttributesEx | 0x400, AttributesEx3 = AttributesEx | 0x20000 WHERE Id=36220;

-- Hellmaw's banish, add stun effect
UPDATE spell_dbc SET Effect2=6, EffectApplyAuraName2=12, EffectImplicitTargetA1=1 WHERE Id=30231;

-- Shadow Nova. Add stun effect
UPDATE spell_dbc SET DurationIndex=1, Effect2=6, EffectApplyAuraName2=12, EffectImplicitTargetA2=1 WHERE Id=33846;

-- Remove SpellIconID of teleport spell to stop it overriding Empowering Shadows (Was 217 before)
UPDATE spell_dbc SET SpellIconId=0 WHERE Id=33563;

-- Make Sonic Boom's Damage Component AoE as well
UPDATE spell_dbc SET EffectImplicitTargetA1=22, EffectImplicitTargetB1=15, EffectRadiusIndex1=39,
    EffectImplicitTargetA2=22, EffectImplicitTargetB2=15, EffectRadiusIndex2=39
	WHERE Id=36841 OR Id=38897;

-- Shockwave. Add a knock-up effect.
UPDATE spell_dbc SET Effect3=98, EffectBasePoints3=250, EffectImplicitTargetA3=1, EffectMiscValue3=10 WHERE Id=33686;

-- Murmur's Touch. Change aura to dummy
UPDATE spell_dbc SET EffectApplyAuraName1=4, EffectImplicitTargetA1=6, EffectImplicitTargetB1=0,
    EffectRadiusIndex1=0 WHERE Id=33711 OR Id=38794;

-- Thundering Storm. Make it single target; let the script decide targets.
UPDATE spell_dbc SET EffectImplicitTargetA1=6, EffectImplicitTargetB1=0 WHERE Id=39365;

-- Suppression Blast. Targeting
UPDATE spell_dbc SET EffectImplicitTargetA1=20, EffectImplicitTargetB1=0 WHERE Id=33332;

-- Empowering Shadows. We touched this spell long ago, and did some bad edits. Here I just reset it.
UPDATE spell_dbc SET EffectImplicitTargetA1=38, EffectImplicitTargetA2=38 WHERE Id=33783 OR Id=39364;

-- Mark of Malice should be shadow damage
UPDATE spell_dbc SET SchoolMask=32 WHERE Id=33494;

-- Remove Icon Id from Shape of the Beast dummy
UPDATE spell_dbc SET SpellIconId=0 WHERE Id=33949;

-- Magnetic Pull. Make it non-negative and AoE.
UPDATE spell_dbc SET AttributesEx = AttributesEx & ~0x80, EffectImplicitTargetA1=20 WHERE Id=33689;
