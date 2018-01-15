-- Crow's (?) Cyclone
UPDATE spell_dbc SET DurationIndex=39 WHERE Id=29538;
-- (Quest) A Spirit Guide's "Call Ancestral Spirit Wolf"
UPDATE spell_dbc SET RequiresSpellFocus=0 WHERE Id=29731;
-- Shade of Aran's Arcane Missiles
UPDATE spell_dbc SET Attributes=0x10000 WHERE Id=29956;
-- Shade of Aran's Summon Blizzard
UPDATE spell_dbc SET EffectImplicitTargetA1=6, EffectImplicitTargetB1=0 WHERE Id=29969;
-- Shade of Aran's Pyroblast
UPDATE spell_dbc SET Attributes=0x10000 WHERE Id=29978;
-- Netherspite's Red Portal's Player Buff
UPDATE spell_dbc SET EffectApplyAuraName3=230 WHERE Id=30421;
-- Omor the Unscarred's Orbital Strike
UPDATE spell_dbc SET CastingTimeIndex=1 WHERE Id=30637;
-- Bleeding Hollow Scryer's Fel Infusion
UPDATE spell_dbc SET EffectImplicitTargetA1=0, EffectImplicitTargetA2=0, EffectImplicitTargetA3=0, EffectImplicitTargetB1=0, EffectImplicitTargetB2=0, EffectImplicitTargetB3=0 WHERE Id=30659;
-- Omor the Unscarred's Treacherous Aura
UPDATE spell_dbc SET CastingTimeIndex=1 WHERE Id=30695;
-- Malchezaar's Enfeeble
UPDATE spell_dbc SET DurationIndex=31 WHERE Id=30843;
-- Malchezaar's Shadow Nova
UPDATE spell_dbc SET AttributesEx2=0x4 WHERE Id=30852;
-- Muselek's Hunter's Mark
UPDATE spell_dbc SET EffectImplicitTargetA1=25, EffectImplicitTargetA2=6, EffectImplicitTargetA3=6, EffectImplicitTargetB1=0, EffectImplicitTargetB2=0, EffectRadiusIndex1=0, EffectRadiusIndex2=0 WHERE Id=31615;
-- Black Stalker's Levitate
UPDATE spell_dbc SET EffectImplicitTargetA1=0, EffectImplicitTargetA1=0, EffectImplicitTargetB1=0, EffectImplicitTargetB2=0 WHERE Id=31704;
-- Black Stalker's Chain Lightning
UPDATE spell_dbc SET AttributesEx=0x4 WHERE Id=31717;
-- Museleke's Throw Freezing Trap
UPDATE spell_dbc SET EffectImplicitTargetA1=0, EffectImplicitTargetB1=0, EffectRadiusIndex1=0 WHERE Id=31946;
-- Murmur's Touch
UPDATE spell_dbc SET EffectImplicitTargetA1=6, EffectImplicitTargetB1=0 WHERE Id=33711;
-- Void Traveler's (Shadow Lab) Empowering Shadows
UPDATE spell_dbc SET EffectImplicitTargetA1=0, EffectImplicitTargetA2=0 WHERE Id=33783;
-- Nethermancer Sepethrea's Raging Flame's Inferno
UPDATE spell_dbc SET EffectTriggerSpell1=35283 WHERE Id=35268;
-- Nightbane's Bellowing Roar
UPDATE spell_dbc SET CastingTimeIndex=16 WHERE Id=36922;
-- Moroe's Garrote
UPDATE spell_dbc SET EffectImplicitTargetA1=6 WHERE Id=37066;
-- Chess: Human Conjurer's Elemental Blast
UPDATE spell_dbc SET EffectImplicitTargetA1=6 WHERE Id=37462;
-- Chess: Orc Warlock's Fireball
UPDATE spell_dbc SET EffectImplicitTargetA1=6 WHERE Id=37463;
-- Chess: Human Conjurer's Rain of Fire
UPDATE spell_dbc SET EffectImplicitTargetA1=25 WHERE Id=37465;
-- Omor the Unscarred's Bane of Treachery
UPDATE spell_dbc SET CastingTimeIndex=1 WHERE Id=37566;
-- Murmur's Touch
UPDATE spell_dbc SET EffectImplicitTargetA1=6, EffectImplicitTargetA2=6, EffectImplicitTargetB1=0, EffectImplicitTargetB2=0 WHERE Id=38794;
-- Malchezaar's Amplify Damage
UPDATE spell_dbc SET EffectImplicitTargetA1=6, EffectImplicitTargetB1=0 WHERE Id=39095;
-- Chess: Game In Session
UPDATE spell_dbc SET Attributes=0x80000000, DurationIndex=21, Effect2=6, EffectBaseDice2=0x1, EffectBasePoints2=-1, EffectImplicitTargetA2=1, EffectApplyAuraName2=39 WHERE Id=39331;
-- Nexus Stalker's Phantom strike NOTE: This is probably a miss-write and was supposed to be part of game in session, but since it works without... What was it supposed to do oO?
-- UPDATE spell_dbc SET EffectMiscValue2=126 WHERE Id=39332;
-- Void Traveler's Empowering Shadows
UPDATE spell_dbc SET EffectImplicitTargetA1=0, EffectImplicitTargetA2=0 WHERE Id=39364;