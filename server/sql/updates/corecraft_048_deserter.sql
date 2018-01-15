-- Deserter debuff spell should be a normal spell, not an arcane spell
-- This fixes divine shield, iceblock and other immunity effects preventing
-- the application of the deserter debuff
UPDATE `spell_dbc` SET SchoolMask = 0 WHERE id = 26013;
