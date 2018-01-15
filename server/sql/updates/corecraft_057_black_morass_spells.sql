-- Create a new spell to add as triggered into Corrupt Medivh
INSERT INTO spell_dbc (Id, EquippedItemClass, rangeIndex, Effect1, SpellName1)
VALUES(150006, -1, 13, 3, "Medivh's Corruption Tick");
-- Make medivh's corruption tick this spell
UPDATE spell_dbc SET EffectTriggerSpell1=150006 WHERE Id=31326 OR Id=37853;
-- As well as be a seperate stacks spell
UPDATE spell_dbc SET AttributesEx3=AttributesEx3 | 0x80 WHERE Id=31326 OR Id=37853;

-- Make Mortal Wound stack to 10 as it did pre-nerf
UPDATE spell_dbc SET StackAmount=10 WHERE Id=31464;

-- Memory Wipe
UPDATE spell_dbc SET EffectImplicitTargetA1=21, EffectImplicitTargetB1=0, EffectRadiusIndex1=0, RangeIndex=35 WHERE Id=33336;
