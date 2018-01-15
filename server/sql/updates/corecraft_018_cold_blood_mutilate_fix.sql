-- Cold Blood. Fix Affected Spells.

-- Reset the spell mask
UPDATE `spell_affect` SET `SpellFamilyMask`=0 WHERE `entry`=14177 AND `effectId`=0;

-- Add spells that should be affected:

-- Backstab
UPDATE `spell_affect` SET `SpellFamilyMask`=`SpellFamilyMask`|0x800004 WHERE `entry`=14177 AND `effectId`=0;
-- Sinister Strike
UPDATE `spell_affect` SET `SpellFamilyMask`=`SpellFamilyMask`|0x800002 WHERE `entry`=14177 AND `effectId`=0;
-- Eviscerate
UPDATE `spell_affect` SET `SpellFamilyMask`=`SpellFamilyMask`|0x820000 WHERE `entry`=14177 AND `effectId`=0;
-- Shiv
UPDATE `spell_affect` SET `SpellFamilyMask`=`SpellFamilyMask`|0x20000000 WHERE `entry`=14177 AND `effectId`=0;
-- Ambush
UPDATE `spell_affect` SET `SpellFamilyMask`=`SpellFamilyMask`|0x800200 WHERE `entry`=14177 AND `effectId`=0;
-- Riposte
UPDATE `spell_affect` SET `SpellFamilyMask`=`SpellFamilyMask`|0x10000000000 WHERE `entry`=14177 AND `effectId`=0;
-- Ghostly Strike
UPDATE `spell_affect` SET `SpellFamilyMask`=`SpellFamilyMask`|0x44000000 WHERE `entry`=14177 AND `effectId`=0;
-- Hemorrhage
UPDATE `spell_affect` SET `SpellFamilyMask`=`SpellFamilyMask`|0x2800000 WHERE `entry`=14177 AND `effectId`=0;
-- Mutilate Main Hand
UPDATE `spell_affect` SET `SpellFamilyMask`=`SpellFamilyMask`|0x200000000 WHERE `entry`=14177 AND `effectId`=0;
-- Mutilate Off Hand
UPDATE `spell_affect` SET `SpellFamilyMask`=`SpellFamilyMask`|0x400000000 WHERE `entry`=14177 AND `effectId`=0;
-- Envenom
UPDATE `spell_affect` SET `SpellFamilyMask`=`SpellFamilyMask`|0x800800000 WHERE `entry`=14177 AND `effectId`=0;
-- Deadly Throw
UPDATE `spell_affect` SET `SpellFamilyMask`=`SpellFamilyMask`|0x100800000 WHERE `entry`=14177 AND `effectId`=0;