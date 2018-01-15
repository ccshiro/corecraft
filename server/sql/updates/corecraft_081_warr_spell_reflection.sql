-- world

-- Warrior's Spell Reflection.
-- These proc flags caused incorrect behavior. So we remove them and let
-- the core remove the spell for us (when the reflect actually happens).
UPDATE spell_dbc SET procFlags=0, procCharges=0 WHERE Id=23920;
