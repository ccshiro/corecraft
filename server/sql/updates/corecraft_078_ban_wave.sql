-- characters
DROP TABLE IF EXISTS ban_wave;
CREATE TABLE ban_wave
(
account_id INT UNSIGNED NOT NULL,
char_name VARCHAR(16) NOT NULL,
reason VARCHAR(255) NOT NULL DEFAULT '',
banned_by VARCHAR(63) NOT NULL DEFAULT '',
removed_by VARCHAR(63) NOT NULL DEFAULT ''
);
-- Note: Cannot have a primary key as SQL transaction in ban_wave::save_to_db() will not work then
