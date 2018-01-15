ALTER TABLE `creature_template_addon`
ADD COLUMN `quest_visibility` INT(11) UNSIGNED NOT NULL DEFAULT 0 AFTER `moveflags`;
ALTER TABLE `creature_template_addon`
ADD COLUMN `quest_vis_flags` INT(11) UNSIGNED NOT NULL DEFAULT 0 AFTER `quest_visibility`;

ALTER TABLE `creature_addon`
ADD COLUMN `quest_visibility` INT(11) UNSIGNED NOT NULL DEFAULT 0 AFTER `moveflags`;
ALTER TABLE `creature_addon`
ADD COLUMN `quest_vis_flags` INT(11) UNSIGNED NOT NULL DEFAULT 0 AFTER `quest_visibility`;
