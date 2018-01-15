
/* Run on mangos' realmd database */

DELIMITER //

/* Creates account. Returns -1 if name is taken, -2 if unknown error happened and a > 0 id on success. */
CREATE PROCEDURE CreateServerAccount(IN _username varchar(16), IN _password varchar(16), IN _email TEXT, IN _expansion TINYINT)
BEGIN
  DECLARE res_id INT;
  DECLARE upper_username VARCHAR(16);
  SELECT UPPER(_username) INTO upper_username;
  SELECT id INTO res_id FROM account WHERE username=upper_username;
  IF res_id > 0 THEN
    SELECT -1;
  ELSE
    BEGIN
      DECLARE _pass_hash VARCHAR(40);
      SELECT SHA1(CONCAT(upper_username, ':', UPPER(_password))) INTO _pass_hash;
      INSERT INTO account (username, sha_pass_hash, gmlevel, email, expansion)
        VALUES(upper_username, _pass_hash, 0, _email, _expansion);
      IF ROW_COUNT() != 1 THEN
        SELECT -2;
      ELSE
        SELECT LAST_INSERT_ID();
      END IF;
    END;
  END IF;
END//

/* Update account password. Returns -1 if the account's username does not match provided username, 1 if success */
CREATE PROCEDURE UpdateServerAccountPassword(IN _acc_id INT, IN _username varchar(16), IN _newpassword varchar(16), IN _email TEXT)
BEGIN
  DECLARE _name VARCHAR(16);
  SELECT username INTO _name FROM account WHERE id=_acc_id;
  IF STRCMP(_name, UPPER(_username)) != 0 THEN
    SELECT -1;
  ELSE
    BEGIN
      DECLARE _pass_hash VARCHAR(40);
      SELECT SHA1(CONCAT(_name, ':', UPPER(_newpassword))) INTO _pass_hash;
      UPDATE account SET sha_pass_hash=_pass_hash, v=NULL, s=NULL, email=_email WHERE id=_acc_id;
      SELECT 1;
    END;
  END IF;
END//

CREATE PROCEDURE GetOnlinePlayers(IN _realm_name VARCHAR(32))
BEGIN
  SELECT alliance_online, horde_online FROM realmlist WHERE name=_realm_name;
END//

DELIMITER ;