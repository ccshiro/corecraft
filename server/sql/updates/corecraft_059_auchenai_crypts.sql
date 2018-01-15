-- Inhibit magic. We chose to make the script trigger the spell on intended targets rather than mod the core.
UPDATE spell_dbc SET EffectImplicitTargetA1=6, EffectImplicitTargetB1=0 WHERE Id=32264;

-- Possess Main Spell, make a full effect spell apply the hidden charm component
UPDATE spell_dbc SET EffectImplicitTargetA3=6, EffectBasePoints3=1, Effect3=6,
    EffectApplyAuraName3=23, EffectAmplitude3=6000, EffectTriggerSpell3=32830 WHERE Id=33401;
-- Add a dummy effect to the hidden component so we can add the visible component as well as a proc on damage aura
UPDATE spell_dbc SET Effect3=6, EffectApplyAuraName3=4, EffectImplicitTargetA3=6 WHERE Id=32830;
-- Create a new dummy spell that procs on damage so when we are below 50% we can remove the 2 charm spells
INSERT INTO spell_dbc(Id, EquippedItemClass, RangeIndex, DurationIndex, Effect1, EffectApplyAuraName1,
                      EffectImplicitTargetA1, EffectBasePoints1, procChance, procFlags, SpellName1)
    VALUES(150007, -1, 134, 6, 6, 4, 1, 1, 101, 0x222A8, "Auchenai Crypt Possess Dummy");
-- Possesed should be negative
UPDATE spell_dbc SET AttributesEx = AttributesEx | 0x80 WHERE Id=32831;

-- Soul Scream
UPDATE spell_dbc SET AuraInterruptFlags=0x02 where Id=32421;

-- Infuriate
UPDATE spell_dbc SET procFlags=0x222A8 WHERE Id=32885;
