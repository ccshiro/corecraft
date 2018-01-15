-- Shaman Talent Elemental Weapons now affects Flametongue Weapon
UPDATE spell_affect SET SpellFamilyMask=SpellFamilyMask | 0x200000 WHERE entry = '29080' and effectId=1;
UPDATE spell_affect SET SpellFamilyMask=SpellFamilyMask | 0x200000 WHERE entry = '29079' and effectId=1;
UPDATE spell_affect SET SpellFamilyMask=SpellFamilyMask | 0x200000 WHERE entry = '16266' and effectId=1;
