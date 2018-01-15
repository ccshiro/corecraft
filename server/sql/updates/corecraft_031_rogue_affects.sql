-- Improved Poison (rank 1 to 5) now also affects Anesthetic Poison and Wound poison
UPDATE spell_affect SET SpellFamilyMask = SpellFamilyMask | 0x1000000000 | 0x10000000 WHERE entry=14113 AND effectId=0;
UPDATE spell_affect SET SpellFamilyMask = SpellFamilyMask | 0x1000000000 | 0x10000000 WHERE entry=14114 AND effectId=0;
UPDATE spell_affect SET SpellFamilyMask = SpellFamilyMask | 0x1000000000 | 0x10000000 WHERE entry=14115 AND effectId=0;
UPDATE spell_affect SET SpellFamilyMask = SpellFamilyMask | 0x1000000000 | 0x10000000 WHERE entry=14116 AND effectId=0;
UPDATE spell_affect SET SpellFamilyMask = SpellFamilyMask | 0x1000000000 | 0x10000000 WHERE entry=14117 AND effectId=0;

-- Puncturing Wounds (rank 1 to 3) also affects your off-hand mutilate attack
UPDATE spell_affect SET SpellFamilyMask = SpellFamilyMask | 0x200000000 WHERE entry=13733 AND effectId=1;
UPDATE spell_affect SET SpellFamilyMask = SpellFamilyMask | 0x200000000 WHERE entry=13865 AND effectId=1;
UPDATE spell_affect SET SpellFamilyMask = SpellFamilyMask | 0x200000000 WHERE entry=13866 AND effectId=1;

-- Remorseless Attacks (rank 1 and 2) also affected by Hemorrhage and Mutilate (MH and OH)
UPDATE spell_affect SET SpellFamilymask = SpellFamilyMask | 0x2000000 | 0x400000000 | 0x200000000 WHERE entry=14143 AND effectId=0;
UPDATE spell_affect SET SpellFamilymask = SpellFamilyMask | 0x2000000 | 0x400000000 | 0x200000000 WHERE entry=14149 AND effectId=0;
