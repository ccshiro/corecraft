-- characters

DROP TABLE character_ticket;

-- This table should arguably be in realmd, especially if you intend multiple
-- realms for the same account (which we don't)
CREATE TABLE player_note
(
account_id INT UNSIGNED NOT NULL DEFAULT 0,
note VARCHAR(240) NOT NULL DEFAULT ''
);

CREATE TABLE ticket_history
(
resolve_time BIGINT UNSIGNED NOT NULL DEFAULT 0,
ticket_text TEXT,
player_acc INT UNSIGNED NOT NULL DEFAULT 0,
resolve_acc INT UNSIGNED NOT NULL DEFAULT '0' COMMENT 'Account Id of GM that resolved ticket',
backlog TEXT,
resolve_mail TEXT
);
