-- Add Bloodthirst next to Mortal Strike for 'Bloodthirst and Mortal Strike Discount'
UPDATE spell_affect SET SpellFamilyMask=0x40002000000 WHERE entry=37535;
-- Add Eviscerate and Envenom to 'Eviscerate and Envenom Discount'
UPDATE spell_affect SET SpellFamilyMask=0x800020000 WHERE entry=37166;
