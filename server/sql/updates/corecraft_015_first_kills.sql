/* Run this in the characters database */
CREATE TABLE first_kills
(
  guild_id int UNSIGNED NOT NULL,
  guild_name varchar(255) NOT NULL,
  guild_faction varchar(8) NOT NULL,
  instance_name varchar(63) DEFAULT NULL,
  order_in_instance tinyint UNSIGNED DEFAULT 0,
  boss_name varchar(63) NOT NULL,
  boss_entry int UNSIGNED DEFAULT 0,
  kill_unix_timestamp int(11) UNSIGNED NOT NULL
)