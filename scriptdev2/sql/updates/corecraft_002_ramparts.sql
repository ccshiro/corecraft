UPDATE mangos.creature_template SET `AIName`="", `ScriptName`="mob_add_fiendish_hound" WHERE `entry`=17540;
DELETE FROM mangos.creature_ai_scripts WHERE `creature_id`=17540;