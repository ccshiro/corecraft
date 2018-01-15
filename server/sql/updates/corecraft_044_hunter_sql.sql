-- Wyvern Sting r1-3, 4 already works (target: Duel vs Player -> chain damage)
UPDATE spell_dbc SET EffectImplicitTargetA1=6 WHERE Id=24131 or Id=24134 or Id=24135;

-- Entrapment for Snake
UPDATE spell_dbc SET EffectImplicitTargetA1=53, EffectImplicitTargetA2=53 WHERE Id=45145;

-- 18350 was used for leader of the pack too, here we just create a new spell and change what frost trap "points" to
-- Entrapment for Frost Trap
INSERT INTO spell_dbc (Id, Dispel, CastingTimeIndex, procChance, maxLevel, baseLevel, spellLevel, DurationIndex,
rangeIndex, EquippedItemClass, Effect1, SpellName1, Rank1, SpellFamilyName, SpellFamilyFlags, DmgClass, PreventionType,
DmgMultiplier1, DmgMultiplier2, DmgMultiplier3, SchoolMask)
VALUES(53100, 1, 1, 101, 62, 56, 56, 8, 7, -1, 3, "Frost Trap Effect Tick", "", 9, 4, 1, 1, 1, 1, 1, 4);
-- Update triggered spell of frost trap
UPDATE spell_dbc SET EffectTriggerSpell2=53100 WHERE Id=13810;

-- Fix the snakes of snake trap
UPDATE creature_template SET spell1=30981, spell2=25810, spell3=34655, minhealth=101, maxhealth=101, mindmg=16, maxdmg=28 WHERE entry=19921;
UPDATE creature_template SET minhealth=101, maxhealth=101, mindmg=54, maxdmg=82 WHERE entry=19833;
-- Cooldown on poisons
UPDATE spell_dbc SET Category=1650, RecoveryTime=4000, CategoryRecoveryTime=4000 WHERE Id=30981 OR Id=25810 OR Id=34655;
