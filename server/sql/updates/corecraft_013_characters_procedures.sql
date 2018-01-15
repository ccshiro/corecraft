
/* Run on mangos' character database */

DELIMITER //

/* Select all characters from account with given id */
CREATE PROCEDURE GetCharactersFromAccountId(IN acc_id INT)
BEGIN
  SELECT guid, name, race, class, gender, level FROM characters WHERE account=acc_id;
END//

/* Get GuildId and Rank of Character by id */
CREATE PROCEDURE GetGuildInfoOfCharacter(IN char_id INT)
BEGIN
  SELECT guildid, rank FROM guild_member WHERE guid=char_id;
END//

/* Get Guild Name And Leader Guid from Id */
CREATE PROCEDURE GetGuildNameAndLeaderGuidFromId(IN guild_id INT)
BEGIN
  SELECT name, leaderguid FROM guild WHERE guildid=guild_id;
END//

/* Create Pending Market Item */
CREATE PROCEDURE CreatePendingMarketItem(IN _char_guid INT, IN _item_id INT, IN _quantity INT)
BEGIN
  INSERT INTO pending_market_items (char_guid, item_id, quantity) VALUES(_char_guid, _item_id, _quantity);
END//

/* Select all First Kills of Instance */
CREATE PROCEDURE SelectFirstKillsFromInstanceName(IN _instance_name varchar(63))
BEGIN
  SELECT guild_id, guild_name, guild_faction, boss_name, kill_unix_timestamp FROM first_kills WHERE instance_name=_instance_name;
END//

DELIMITER ;