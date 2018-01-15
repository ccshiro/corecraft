-- world
CREATE TABLE spell_proc_exception
(
sid INT UNSIGNED NOT NULL,
list TEXT NOT NULL,
white TINYINT NOT NULL DEFAULT 0,
PRIMARY KEY(sid)
);
