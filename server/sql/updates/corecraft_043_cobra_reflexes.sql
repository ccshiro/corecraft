-- Add 15% damage reduction to cobra reflexes. Note misc 127 affects spell casting too, should it perhaps only be melee damage?
UPDATE spell_dbc SET Effect2=6, EffectDieSides2=1, EffectBaseDice2=1, EffectBasePoints2=-15,
    EffectImplicitTargetA2=1, EffectApplyAuraName2=79, EffectMiscValue2=127 WHERE Id=25076;
