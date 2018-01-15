-- Seal Fate
-- The past mask is completely useless, we start from afres and add a mask that give the following spells:
-- Ambush 0x800200
-- Backstab 0x800004
-- Ghostly Strike 0x44000000
-- Gouge 0x8
-- Hemorrhage 0x2800000
-- Mutilate (MH & OH) 0x400000000 0x200000000
-- Shiv 0x20000000
-- Sinister Strike 0x800002
-- Note: The addition of cooldown=1 means that if both mutilate procs only one adds a combo point, but it's less than GCD so it shouldn't interfere with anything
UPDATE spell_proc_event SET SpellFamilyMask0 = 0x200 | 0x4 | 0x4000000 | 0x2000000 | 0x400000000 | 0x200000000 | 0x20000000 | 0x2,
    SpellFamilyMask1 = 0x200 | 0x4 | 0x4000000 | 0x2000000 | 0x400000000 | 0x200000000 | 0x20000000 | 0x2,
    SpellFamilyMask2 = 0x200 | 0x4 | 0x4000000 | 0x2000000 | 0x400000000 | 0x200000000 | 0x20000000 | 0x2, cooldown=1
    WHERE entry=14186; -- Only rank 1 is needed in this table
