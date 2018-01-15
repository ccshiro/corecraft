-- world

-- remove old quest vis
ALTER TABLE creature_addon DROP COLUMN quest_visibility;
ALTER TABLE creature_addon DROP COLUMN quest_vis_flags;
ALTER TABLE creature_template_addon DROP COLUMN quest_visibility;
ALTER TABLE creature_template_addon DROP COLUMN quest_vis_flags;

-- add new field
ALTER TABLE creature_addon ADD COLUMN quest_vis TEXT AFTER moveflags;
ALTER TABLE creature_template_addon ADD COLUMN quest_vis TEXT AFTER moveflags;
