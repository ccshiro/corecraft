-- Omen of Clarity spell_affect Changes:

-- Removes Faerie Fire and Faerie Fire (Feral)
UPDATE `spell_affect` SET `SpellFamilyMask`= `SpellFamilyMask` & ~0x400 WHERE `entry`=16870 AND `effectId`=0;
-- Removes Prowl
UPDATE `spell_affect` SET `SpellFamilyMask`= `SpellFamilyMask` & ~0x4000 WHERE `entry`=16870 AND `effectId`=0;