-- mangosd

-- Immunity spells
-- Spell haste immunity
INSERT INTO spell_dbc (Id, EquippedItemClass, DurationIndex, Effect1, EffectApplyAuraName1,
    EffectImplicitTargetA1, EffectMiscValue1, SpellName1)
VALUES(150015, -1, 21, 6, 38, 1, 216, "Spell Aura Haste Spells IMMUNITY");
-- Poison immunity
INSERT INTO spell_dbc (Id, EquippedItemClass, DurationIndex, Effect1, EffectApplyAuraName1,
    EffectImplicitTargetA1, EffectMiscValue1, SpellName1)
VALUES(150016, -1, 21, 6, 41, 1, 4, "Poison IMMUNITY");
-- Mana drain & Mana burn Immunity
INSERT INTO spell_dbc (Id, EquippedItemClass, DurationIndex, Effect1, EffectApplyAuraName1,
    EffectImplicitTargetA1, EffectMiscValue1, Effect2, EffectApplyAuraName2, EffectImplicitTargetA2, EffectMiscValue2, SpellName1)
VALUES(150017, -1, 21, 6, 38, 1, 64, 6, 37, 1, 62, "Mana drain & Mana burn IMMUNITY");

--
-- COLDMIST WIDOW
--
-- Coldmist Widow dummy effect. This spell gets procd by the following one
INSERT INTO spell_dbc (Id, EquippedItemClass, Effect1, EffectImplicitTargetA1, SpellName1)
    VALUES(150011, -1, 3, 1, "Coldmist Widow Aura Proc");
-- Coldmist Widow dummy proc aura with 10% chance to trigger on taken melee or ranged attack (same proc flags as shield block)
INSERT INTO spell_dbc (Id, EquippedItemClass, DurationIndex, procFlags, procChance, Effect1, EffectApplyAuraName1,
    EffectImplicitTargetA1, EffectTriggerSpell1, SpellName1)
    VALUES(150012, -1, 21, 0x2A8, 10, 6, 42, 1, 150011, "Coldmist Widow Dummy Aura");

--
-- Skeletal Usher
--
-- Make a dummy spell that usher can cast to do all his magnetic pull logic
INSERT INTO spell_dbc (Id, EquippedItemClass, RangeIndex, Effect1, EffectImplicitTargetA1, SpellName1)
    VALUES(150013, -1, 6, 3, 6, "Skeletal Usher Magnetic Combat Sequence");

--
-- Spectral Stable Hand
--
UPDATE spell_dbc SET EffectImplicitTargetA1=45 WHERE Id=29339;
UPDATE spell_dbc SET EffectImplicitTargetA1=21 WHERE Id=29340;
DELETE FROM spell_script_target WHERE entry=29339 OR entry=29340;

--
-- Ghastly Haunt
--
-- Create an aura to proc Ethereal Curse on melee attack
INSERT INTO spell_dbc (Id, EquippedItemClass, DurationIndex, procFlags, procChance, Effect1, EffectApplyAuraName1,
    EffectImplicitTargetA1, EffectTriggerSpell1, SpellName1)
    VALUES(150014, -1, 21, 0x14, 25, 6, 42, 1, 29716, "Ghastly Haunt Ethereal Curse Aura");

--
-- Skeletal Waiter
--
-- Make Brittle Bones be a periodic dummy aura
UPDATE spell_dbc SET EffectApplyAuraName1=226, EffectAmplitude1=1000 WHERE Id=32441;

--
-- Ghostly Philanthropist
--
-- Add exhaust to Incite Rage
UPDATE spell_dbc SET Effect2=6, EffectApplyAuraName2=23, EffectImplicitTargetA2=38, 
EffectAmplitude2=6000, EffectTriggerSpell2=29650 WHERE Id=29612;

--
-- Ethereal Spellfilcher
--
-- Make Transference generic
UPDATE spell_dbc SET SpellFamilyName=0 WHERE Id=30039;

--
-- Arcane Watchman
--
-- Overload
INSERT INTO spell_dbc (Id, EquippedItemClass, Effect1, EffectImplicitTargetA1, SpellName1)
    VALUES(29767, -1, 3, 1, "Overload");


--
-- Arcane Anomaly
--
-- Blink, make it generic family
UPDATE spell_dbc SET SpellFamilyName=0 WHERE Id=29883;

--
-- Moroes
--
-- Garrote should not be target self or have 0 in range: (Giving it 20 yards even though it's technically a melee spell)
UPDATE spell_dbc SET RangeIndex=3, EffectImplicitTargetA1=6 WHERE Id=37066;

--
-- Maiden of Virtue
--
-- Make repentance not have the stop attack target attribute
UPDATE spell_dbc SET Attributes = Attributes & ~0x100000 WHERE Id=29511;

--
-- Romulo & Julianne
--
-- Undying love: Set target to single friendly and remove negative attribute
UPDATE spell_dbc SET EffectImplicitTargetA1=21, AttributesEx = AttributesEx & ~0x80 WHERE Id=30951;
-- Add a dummy effect to poison thrust (to duplicate the aura)
UPDATE spell_dbc SET Effect3=3, EffectImplicitTargetA3=6 WHERE Id=30822;

--
-- Wizard of Oz
--
-- Make cyclone not generate threat
UPDATE spell_dbc SET AttributesEx=AttributesEx | 0x400, AttributesEx3=AttributesEx3 | 0x20000 WHERE Id=29538;

--
-- Netherspite
--
-- Remove initial aggro and threat from nether burn aura
UPDATE spell_dbc SET AttributesEx3=AttributesEx3 | 0x20000, AttributesEx=AttributesEx | 0x400 WHERE Id=30522 OR Id=30523;

--
-- Malchezaar
--
-- Make Enfeeble single target (makes it easier to use in the script)
UPDATE spell_dbc SET RangeIndex=13, EffectImplicitTargetA1=6, EffectImplicitTargetB1=0, EffectRadiusIndex1=0,
    EffectImplicitTargetA2=6, EffectImplicitTargetB2=0, EffectRadiusIndex2=0 WHERE Id=30843;
-- Make Summon Axes be a dummy only (having them summoned gives more trouble)
UPDATE spell_dbc SET Effect1=3, EffectMiscValue1=0, EffectMiscValueB1=0 WHERE Id=30891;

--
-- Nightbane
--
-- Make the test distracting ash into a dummy -- the real one makes a flight animation since they removed it from
-- the ground phase; we cast the fake version and apply the real one as triggered
UPDATE spell_dbc SET DurationIndex=0, Effect1=3, EffectBasePoints1=0, EffectApplyAuraName1=0, EffectMiscValue1=0 WHERE Id=30280;

--
-- Misc
--
-- Torment of the Worgen lower chance and stuff
UPDATE spell_dbc SET procChance=30 WHERE Id=30564 or Id=30567;
-- Wrathe of the Titans: Make them generic
UPDATE spell_dbc SET SpellFamilyName=0 WHERE Id=30554 OR Id=30610;
-- Medivh's Journal. Make dummy instead
UPDATE spell_dbc SET Effect1=3, EffectMiscValue1=0 WHERE Id=31114;
-- Remove Can't Reflect flag from Fireball
UPDATE spell_dbc SET AttributesEx2=0, EffectImplicitTargetA1=6 WHERE Id=30967;
-- Make Reflection last a tad bit longer
UPDATE spell_dbc SET DurationIndex=8 WHERE Id=30969;

--
-- NERFS START
--
-- Moroes
-- Blind was physical in 2.0.3
UPDATE spell_dbc SET SchoolMask=0 WHERE Id=34694;
-- Holy Wrath did more damage in 2.0.3
UPDATE spell_dbc SET EffectBasePoints1=2199 WHERE Id=32445;

-- Romulo and Julianne
-- Blinding Passing did more damage in 2.0.3
UPDATE spell_dbc SET EffectBasePoints1=1999, EffectBasePoints2=999 WHERE Id=30890;
-- Daring was 50% increase in 2.0.3
UPDATE spell_dbc SET EffectBasePoints1=49, EffectBasePoints2=49 WHERE Id=30841;
-- Deadly Swathe has been changed. Before it did +300 weapon damage cleave. Now it does
-- a chain effect target. But cleaves before did 360 degrees (?), so the chain target gives a similar effect?
-- Ignoring change

-- Shade of Aran
-- Dragon's Breath ticked every second with 1000 damage instead of every 2nd with 2000, and it
-- also procd 29965
UPDATE spell_dbc SET EffectBasePoints1=999, EffectAmplitude1=1000, Effect3=6,
    EffectApplyAuraName3=23, EffectAmplitude3=1000, EffectTriggerSpell3=29965 WHERE Id=29964;

-- Nightbane
-- Smoking blast did A LOT more damage in 2.0.3
UPDATE spell_dbc SET EffectBasePoints1=464, EffectDieSides1=1 WHERE Id=30127;
UPDATE spell_dbc SET EffectBasePoints1=4254, EffectDieSides1=691 WHERE Id=30128;
--
-- NERFS END
--
	