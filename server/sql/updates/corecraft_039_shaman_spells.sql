-- Gear for shamans (mainly totems) by Barroth

-- Totem of the Thunderhead (Item Id: 24413)
UPDATE spell_affect SET SpellFamilyMask = 0x2000000000 WHERE entry=34318 AND effectId=0;
-- Tidefury Raiment Fix(Itemset Id: 630)
UPDATE spell_affect SET SpellFamilyMask = 0x2000000000 WHERE entry=37209 AND effectId=0;

-- Totem of Life (Item Id: 22396)
INSERT INTO spell_affect (entry, effectId, SpellFamilyMask) VALUES (27855, 0, 0x80);
-- Totem of Sustaining (Item Id: 23200)
INSERT INTO spell_affect (entry, effectId, SpellFamilyMask) VALUES (28856, 0, 0x80);
-- Totem of the Plains (Item Id: 25645)
INSERT INTO spell_affect (entry, effectId, SpellFamilyMask) VALUES (32401, 0, 0x80);

-- (Earth Shock: 0x100000, Flame Shock: 0x10000000, Frost Shock: 0x80000000)
-- Totem of Rage (Item Id: 22395)
INSERT INTO spell_affect (entry, effectId, SpellFamilyMask) VALUES (27859, 0, 0x90100000);
-- Totem of Impact (Id 27947 (horde version) & 27984 (alliance version))
INSERT INTO spell_affect (entry, effectId, SpellFamilyMask) VALUES (33556, 0, 0x90100000);

-- (Lightning Bolt: 0x1 Chain Lightning: 0x2)
-- Totem of the Storm (Item Id: 23199)
INSERT INTO spell_affect (entry, effectId, SpellFamilyMask) VALUES (28857, 0, 0x3);
-- Totem of the Void (Item Id: 28248)
INSERT INTO spell_affect (entry, effectId, SpellFamilyMask) VALUES (34230, 0, 0x3);
-- Totem of Ancestral Guidance
INSERT INTO spell_affect (entry, effectId, SpellFamilyMask) VALUES (41040, 0, 0x3);

-- Skyshatter Harness (Itemset Id: 682)
INSERT INTO spell_affect (entry, effectId, SpellFamilyMask) VALUES (38432, 0, 0x1000000000);

-- Spells & Talents

-- Add Chain Heal to the spells affected by Tidal Focus (Talent)
UPDATE spell_affect SET SpellFamilyMask = SpellFamilyMask | 0x100 WHERE entry=16179 AND effectId=0;
UPDATE spell_affect SET SpellFamilyMask = SpellFamilyMask | 0x100 WHERE entry=16214 AND effectId=0;
UPDATE spell_affect SET SpellFamilyMask = SpellFamilyMask | 0x100 WHERE entry=16215 AND effectId=0;
UPDATE spell_affect SET SpellFamilyMask = SpellFamilyMask | 0x100 WHERE entry=16216 AND effectId=0;
UPDATE spell_affect SET SpellFamilyMask = SpellFamilyMask | 0x100 WHERE entry=16217 AND effectId=0;

-- Healing Grace (Talent). Spell mask completely broken, start new
-- 0x400 Lightning Shield
-- 0x10000000 Flame Shock
-- 0x20000000 Earthbind Totem (and other totems too although it logically makes no sense, since they cannot be dispelled)
-- 0x80000000 Frost Shock
-- 0x1000000000 Stormstrike
-- 0x2000000000 Water Shield
-- 0x4000000000 Heroism & Bloodlust
-- 0x8000000000 Nature's Switfness
-- 0x40000000000 Earth Shield
-- (Elemental Mastery, Healing Way & Ancestral Fortitude & other talents have no spellfamilyflags so they cannot be affected, is this how blizzard had it too or is the client data perhaps just missing the flags?)
UPDATE spell_affect SET SpellFamilyMask = SpellFamilyMask | (0x400 | 0x10000000 | 0x20000000 | 0x80000000 | 0x1000000000 | 0x2000000000 | 0x4000000000 | 0x8000000000 | 0x40000000000) WHERE entry=29187 AND effectId=1;
UPDATE spell_affect SET SpellFamilyMask = SpellFamilyMask | (0x400 | 0x10000000 | 0x20000000 | 0x80000000 | 0x1000000000 | 0x2000000000 | 0x4000000000 | 0x8000000000 | 0x40000000000) WHERE entry=29189 AND effectId=1;
UPDATE spell_affect SET SpellFamilyMask = SpellFamilyMask | (0x400 | 0x10000000 | 0x20000000 | 0x80000000 | 0x1000000000 | 0x2000000000 | 0x4000000000 | 0x8000000000 | 0x40000000000) WHERE entry=29191 AND effectId=1;
