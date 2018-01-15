-- Note, we changed from 32727, the correct arena spell, to a unused one (32728) in a previous commit
-- Rather than changing to the right one (it would require modifying the right one), I decided to use
-- the incorrect one and modify that, since it isn't used and I'd rather modify it and keep the original
-- in case we realize at some point we'd like to embrace the invisibility of the original (doubt it)
UPDATE spell_dbc SET Effect1=6, Effect2=6, EffectApplyAuraName2=215 WHERE Id=32728;