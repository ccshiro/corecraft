-- run on realmd database
ALTER TABLE `account` ADD COLUMN `tokenkey` CHAR(16) AFTER `sha_pass_hash`;
