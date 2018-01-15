-- add required 'dead' column
ALTER TABLE character_pet ADD COLUMN dead BOOL NOT NULL DEFAULT false;
