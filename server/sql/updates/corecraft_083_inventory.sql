-- characters
ALTER TABLE `character_inventory` CHANGE `slot` `index` TINYINT(3) UNSIGNED NOT NULL;
ALTER TABLE `character_inventory` MODIFY `bag` TINYINT(3) UNSIGNED NOT NULL;

-- guild_member
ALTER TABLE `guild_member` CHANGE `BankRemMoney` `bank_withdrawn_money` INT(11)
  UNSIGNED NOT NULL DEFAULT '0';
ALTER TABLE `guild_member` CHANGE `BankRemSlotsTab0` `bank_withdrawals_tab0`
  INT(11) UNSIGNED NOT NULL DEFAULT '0';
  
ALTER TABLE `guild_member` CHANGE `BankRemSlotsTab1` `bank_withdrawals_tab1`
  INT(11) UNSIGNED NOT NULL DEFAULT '0';

ALTER TABLE `guild_member` CHANGE `BankRemSlotsTab2` `bank_withdrawals_tab2`
  INT(11) UNSIGNED NOT NULL DEFAULT '0';

ALTER TABLE `guild_member` CHANGE `BankRemSlotsTab3` `bank_withdrawals_tab3`
  INT(11) UNSIGNED NOT NULL DEFAULT '0';

ALTER TABLE `guild_member` CHANGE `BankRemSlotsTab4` `bank_withdrawals_tab4`
  INT(11) UNSIGNED NOT NULL DEFAULT '0';

ALTER TABLE `guild_member` CHANGE `BankRemSlotsTab5` `bank_withdrawals_tab5`
  INT(11) UNSIGNED NOT NULL DEFAULT '0';

ALTER TABLE `guild_member` DROP COLUMN `BankResetTimeMoney`;
ALTER TABLE `guild_member` DROP COLUMN `BankResetTimeTab0`;
ALTER TABLE `guild_member` DROP COLUMN `BankResetTimeTab1`;
ALTER TABLE `guild_member` DROP COLUMN `BankResetTimeTab2`;
ALTER TABLE `guild_member` DROP COLUMN `BankResetTimeTab3`;
ALTER TABLE `guild_member` DROP COLUMN `BankResetTimeTab4`;
ALTER TABLE `guild_member` DROP COLUMN `BankResetTimeTab5`;

ALTER TABLE `guild_bank_eventlog` DROP COLUMN `LogGuid`;
ALTER TABLE `guild_bank_eventlog` DROP PRIMARY KEY;
