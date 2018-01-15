DROP TABLE IF EXISTS character_recent_dungeons;
CREATE TABLE character_recent_dungeons (
    id INTEGER NOT NULL AUTO_INCREMENT,
    guid INTEGER NOT NULL,
    map INTEGER NOT NULL,
    instance INTEGER NOT NULL,
    timestamp INTEGER NOT NULL,
    PRIMARY KEY(id)
);
