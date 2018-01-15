-- Leader of the pack shouldn't be removable by right clicking on it
UPDATE spell_dbc SET Attributes = Attributes | 0x80000000 WHERE Id=24932;

-- Tree of Life's aura shouldn't be removable by right clicking on it
UPDATE spell_dbc SET Attributes = Attributes | 0x80000000 WHERE Id=34123;

-- Add Moonfire, Nature's Swiftness & (Hibernate & Soothe Animal) to Subtlety's resist dispel affected mask
UPDATE spell_affect SET SpellFamilyMask = SpellFamilyMask | 0x2 | 0x8000000000000 | 0x2000000000000
    WHERE effectId = 1 AND (entry=17118 OR entry=17119 OR entry=17120 OR entry=17121 OR entry=17122);
