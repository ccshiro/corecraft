ALTER TABLE `gameobject_template`
ADD `AIName` VARCHAR(64) DEFAULT '' NOT NULL
AFTER `maxgold`;

-- Revert an old patch (Turns out radius is actually diameter). Also, traps should have a 2 seconds armning time.
UPDATE gameobject_template SET data2=5, data7=2 WHERE entry=2561 OR entry=164876 OR entry=164877 OR entry=164639 OR entry=164638 OR
    entry=164872 OR entry=164873 OR entry=164874 OR entry=164875 OR entry=181030 OR entry=164839 OR entry=164879 OR entry=164780 OR entry=181031 OR entry=183957;
