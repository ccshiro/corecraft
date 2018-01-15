/* Add alliance_online and horde_online to realmd.realmlist */
ALTER TABLE realmlist
ADD COLUMN alliance_online INT DEFAULT 0,
ADD COLUMN horde_online INT DEFAULT 0;