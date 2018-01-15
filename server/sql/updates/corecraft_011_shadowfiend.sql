-- Shadowfiend (estimated by: previoushp/3)
UPDATE pet_levelstats SET hp=2500 WHERE creature_entry=19668 AND LEVEL=66;
UPDATE pet_levelstats SET hp=2666 WHERE creature_entry=19668 AND LEVEL=67;
UPDATE pet_levelstats SET hp=2833 WHERE creature_entry=19668 AND LEVEL=68;
UPDATE pet_levelstats SET hp=3000 WHERE creature_entry=19668 AND LEVEL=69;
UPDATE pet_levelstats SET hp=3210 WHERE creature_entry=19668 AND LEVEL=70;

-- Shadowfiend's Shadow Armor. Adding 90% melee and ranged attacker's miss chance auras
UPDATE spell_dbc SET Effect2=6, Effect3=6, EffectDieSides2=1, EffectDieSides3=1, EffectBaseDice2=1, EffectBaseDice3=1, EffectBasePoints2=-90, EffectBasePoints3=-90, EffectApplyAuraName2=184, EffectApplyAuraName3=185, DmgMultiplier2=1, DmgMultiplier3=1 WHERE Id=34424;