-- Add Mind Flay to Blackout's proc event
UPDATE spell_proc_event SET SpellFamilyMask0 = SpellFamilyMask0 | 0x800000,
    SpellFamilyMask1 = SpellFamilyMask1 | 0x800000, SpellFamilyMask2 = SpellFamilyMask2 | 0x800000 WHERE entry=15268;