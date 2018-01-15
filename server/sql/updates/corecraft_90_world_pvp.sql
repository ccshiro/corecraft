INSERT INTO mangos_string VALUES
(1600,'|cffffff00Northpass Tower has been taken by the Horde!|r',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL),
(1601,'|cffffff00Northpass Tower has been taken by the Alliance!|r',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL),
(1602,'|cffffff00Crown Guard Tower has been taken by the Horde!|r',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL),
(1603,'|cffffff00Crown Guard Tower has been taken by the Alliance!|r',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL),
(1604,'|cffffff00Eastwall Tower has been taken by the Horde!|r',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL),
(1605,'|cffffff00Eastwall Tower has been taken by the Alliance!|r',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL),
(1606,'|cffffff00The Plaguewood Tower has been taken by the Horde!|r',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL),
(1607,'|cffffff00The Plaguewood Tower has been taken by the Alliance!|r',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL),
(1608,'|cffffff00The Overlook has been taken by the Horde!|r',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL),
(1609,'|cffffff00The Overlook has been taken by the Alliance!|r',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL),
(1610,'|cffffff00The Stadium has been taken by the Horde!|r',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL),
(1611,'|cffffff00The Stadium has been taken by the Alliance!|r',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL),
(1612,'|cffffff00Broken Hill has been taken by the Horde!|r',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL),
(1613,'|cffffff00Broken Hill has been taken by the Alliance!|r',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL),
(1614,'|cffffff00The Horde has taken control of the East Beacon!|r',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL),
(1615,'|cffffff00The Alliance has taken control of the East Beacon!|r',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL),
(1616,'|cffffff00The Horde has taken control of the West Beacon!|r',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL),
(1617,'|cffffff00The Alliance has taken control of the West Beacon!|r',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL),
(1618,'|cffffff00The Horde has taken control of both beacons!|r',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL),
(1619,'|cffffff00The Alliance has taken control of both beacons!|r',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL),
(1620,'|cffffff00The Horde Field Scout is now issuing battle standards.|r',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL),
(1621,'|cffffff00The Alliance Field Scout is now issuing battle standards.|r',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL),
(1622,'|cffffff00The Horde has taken control of Twin Spire Ruins!|r',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL),
(1623,'|cffffff00The Alliance has taken control of Twin Spire Ruins!|r',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL),
(1624,'|cffffff00The Horde has taken control of a Spirit Tower!|r',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL),
(1625,'|cffffff00The Alliance has taken control of a Spirit Tower!|r',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL),
(1626,'|cffffff00The Horde has lost control of a Spirit Tower!|r',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL),
(1627,'|cffffff00The Alliance has lost control of a Spirit Tower!|r',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL),
(1628,'|cffffff00The Horde has taken control of The Bone Wastes!|r',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL),
(1629,'|cffffff00The Alliance has taken control of The Bone Wastes!|r',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL),
(1630,'|cffffff00The Horde is gaining control of Halaa!|r',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL),
(1631,'|cffffff00The Alliance is gaining control of Halaa!|r',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL),
(1632,'|cffffff00The Horde has taken control of Halaa!|r',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL),
(1633,'|cffffff00The Alliance has taken control of Halaa!|r',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL),
(1634,'|cffffff00Halaa is defenseless!|r',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL),
(1635,'|cffffff00The Horde has collected 200 silithyst!|r',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL),
(1636,'|cffffff00The Alliance has collected 200 silithyst!|r',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL);

-- Halaa SQL DATA

-- Halaa NPC spawn data(reserved guids 1002755 - 1002800 used 1002755 - 1002794)

-- Shiro: Barroth made his queries UPDATE queries due to a miss-comunication between
-- him and nim, so we need to actually create rows for each creature
DELIMITER ;;
CREATE PROCEDURE temp_7865979461()
BEGIN
  DECLARE i INT DEFAULT 1002755;
  WHILE i <= 1002794 DO
    INSERT INTO creature (guid) VALUES(i);
    SET i = i + 1;
  END WHILE;
END;;
DELIMITER ;
CALL temp_7865979461();
DROP PROCEDURE temp_7865979461;

-- Horde Guards
UPDATE creature SET `id` = 18192, `map` = 530, `spawnMask` = 0, `modelid` = 0, `equipment_id` = 0, `position_x` = -1654.06, `position_y` = 8000.46, `position_z` = -26.59, `orientation` = 3.37, `spawntimesecs` = 300, `spawndist` = 0, `currentwaypoint` = 0, `curhealth` = 1182800, `curmana` = 0, `DeathState` = 0, `MovementType` = 0 WHERE(guid = 1002755);
UPDATE creature SET `id` = 18192, `map` = 530, `spawnMask` = 0, `modelid` = 0, `equipment_id` = 0, `position_x` = -1487.18, `position_y` = 7899.1, `position_z` = -19.53, `orientation` = 0.954, `spawntimesecs` = 300, `spawndist` = 0, `currentwaypoint` = 0, `curhealth` = 1182800, `curmana` = 0, `DeathState` = 0, `MovementType` = 0 WHERE(guid = 1002756);
UPDATE creature SET `id` = 18192, `map` = 530, `spawnMask` = 0, `modelid` = 0, `equipment_id` = 0, `position_x` = -1480.88, `position_y` = 7908.79, `position_z` = -19.19, `orientation` = 4.485, `spawntimesecs` = 300, `spawndist` = 0, `currentwaypoint` = 0, `curhealth` = 1182800, `curmana` = 0, `DeathState` = 0, `MovementType` = 0 WHERE(guid = 1002757);
UPDATE creature SET `id` = 18192, `map` = 530, `spawnMask` = 0, `modelid` = 0, `equipment_id` = 0, `position_x` = -1540.56, `position_y` = 7995.44, `position_z` = -20.45, `orientation` = 0.947, `spawntimesecs` = 300, `spawndist` = 0, `currentwaypoint` = 0, `curhealth` = 1182800, `curmana` = 0, `DeathState` = 0, `MovementType` = 0 WHERE(guid = 1002758);
UPDATE creature SET `id` = 18192, `map` = 530, `spawnMask` = 0, `modelid` = 0, `equipment_id` = 0, `position_x` = -1546.95, `position_y` = 8000.85, `position_z` = -20.72, `orientation` = 6.035, `spawntimesecs` = 300, `spawndist` = 0, `currentwaypoint` = 0, `curhealth` = 1182800, `curmana` = 0, `DeathState` = 0, `MovementType` = 0 WHERE(guid = 1002759);
UPDATE creature SET `id` = 18192, `map` = 530, `spawnMask` = 0, `modelid` = 0, `equipment_id` = 0, `position_x` = -1595.31, `position_y` = 7860.53, `position_z` = -21.51, `orientation` = 3.747, `spawntimesecs` = 300, `spawndist` = 0, `currentwaypoint` = 0, `curhealth` = 1182800, `curmana` = 0, `DeathState` = 0, `MovementType` = 0 WHERE(guid = 1002760);
UPDATE creature SET `id` = 18192, `map` = 530, `spawnMask` = 0, `modelid` = 0, `equipment_id` = 0, `position_x` = -1642.31, `position_y` = 7995.59, `position_z` = -25.8, `orientation` = 3.317, `spawntimesecs` = 300, `spawndist` = 0, `currentwaypoint` = 0, `curhealth` = 1182800, `curmana` = 0, `DeathState` = 0, `MovementType` = 0 WHERE(guid = 1002761);
UPDATE creature SET `id` = 18192, `map` = 530, `spawnMask` = 0, `modelid` = 0, `equipment_id` = 0, `position_x` = -1545.46, `position_y` = 7995.35, `position_z` = -20.63, `orientation` = 1.094, `spawntimesecs` = 300, `spawndist` = 0, `currentwaypoint` = 0, `curhealth` = 1182800, `curmana` = 0, `DeathState` = 0, `MovementType` = 0 WHERE(guid = 1002762);
UPDATE creature SET `id` = 18192, `map` = 530, `spawnMask` = 0, `modelid` = 0, `equipment_id` = 0, `position_x` = -1487.58, `position_y` = 7907.99, `position_z` = -19.27, `orientation` = 5.567, `spawntimesecs` = 300, `spawndist` = 0, `currentwaypoint` = 0, `curhealth` = 1182800, `curmana` = 0, `DeathState` = 0, `MovementType` = 0 WHERE(guid = 1002763);
UPDATE creature SET `id` = 18192, `map` = 530, `spawnMask` = 0, `modelid` = 0, `equipment_id` = 0, `position_x` = -1651.54, `position_y` = 7988.56, `position_z` = -26.5289, `orientation` = 2.98451, `spawntimesecs` = 300, `spawndist` = 0, `currentwaypoint` = 0, `curhealth` = 1182800, `curmana` = 0, `DeathState` = 0, `MovementType` = 0 WHERE(guid = 1002764);
UPDATE creature SET `id` = 18192, `map` = 530, `spawnMask` = 0, `modelid` = 0, `equipment_id` = 0, `position_x` = -1602.46, `position_y` = 7866.43, `position_z` = -22.1177, `orientation` = 4.74729, `spawntimesecs` = 300, `spawndist` = 0, `currentwaypoint` = 0, `curhealth` = 1182800, `curmana` = 0, `DeathState` = 0, `MovementType` = 0 WHERE(guid = 1002765);
UPDATE creature SET `id` = 18192, `map` = 530, `spawnMask` = 0, `modelid` = 0, `equipment_id` = 0, `position_x` = -1591.22, `position_y` = 7875.29, `position_z` = -22.3536, `orientation` = 4.34587, `spawntimesecs` = 300, `spawndist` = 0, `currentwaypoint` = 0, `curhealth` = 1182800, `curmana` = 0, `DeathState` = 0, `MovementType` = 0 WHERE(guid = 1002766);
UPDATE creature SET `id` = 18192, `map` = 530, `spawnMask` = 0, `modelid` = 0, `equipment_id` = 0, `position_x` = -1550.6, `position_y` = 7944.45, `position_z` = -21.63, `orientation` = 3.559, `spawntimesecs` = 300, `spawndist` = 0, `currentwaypoint` = 0, `curhealth` = 1182800, `curmana` = 0, `DeathState` = 0, `MovementType` = 0 WHERE(guid = 1002767);
UPDATE creature SET `id` = 18192, `map` = 530, `spawnMask` = 0, `modelid` = 0, `equipment_id` = 0, `position_x` = -1545.57, `position_y` = 7935.83, `position_z` = -21.13, `orientation` = 3.448, `spawntimesecs` = 300, `spawndist` = 0, `currentwaypoint` = 0, `curhealth` = 1182800, `curmana` = 0, `DeathState` = 0, `MovementType` = 0 WHERE(guid = 1002768);
UPDATE creature SET `id` = 18192, `map` = 530, `spawnMask` = 0, `modelid` = 0, `equipment_id` = 0, `position_x` = -1550.86, `position_y` = 7937.56, `position_z` = -21.7, `orientation` = 3.801, `spawntimesecs` = 300, `spawndist` = 0, `currentwaypoint` = 0, `curhealth` = 1182800, `curmana` = 0, `DeathState` = 0, `MovementType` = 0 WHERE(guid = 1002769);

-- Alliance Guards
UPDATE creature SET `id` = 18256, `map` = 530, `spawnMask` = 0, `modelid` = 0, `equipment_id` = 0, `position_x` = -1654.06, `position_y` = 8000.46, `position_z` = -26.59, `orientation` = 3.37, `spawntimesecs` = 300, `spawndist` = 0, `currentwaypoint` = 0, `curhealth` = 1182800, `curmana` = 0, `DeathState` = 0, `MovementType` = 0 WHERE(guid = 1002770);
UPDATE creature SET `id` = 18256, `map` = 530, `spawnMask` = 0, `modelid` = 0, `equipment_id` = 0, `position_x` = -1487.18, `position_y` = 7899.1, `position_z` = -19.53, `orientation` = 0.954, `spawntimesecs` = 300, `spawndist` = 0, `currentwaypoint` = 0, `curhealth` = 1182800, `curmana` = 0, `DeathState` = 0, `MovementType` = 0 WHERE(guid = 1002771);
UPDATE creature SET `id` = 18256, `map` = 530, `spawnMask` = 0, `modelid` = 0, `equipment_id` = 0, `position_x` = -1480.88, `position_y` = 7908.79, `position_z` = -19.19, `orientation` = 4.485, `spawntimesecs` = 300, `spawndist` = 0, `currentwaypoint` = 0, `curhealth` = 1182800, `curmana` = 0, `DeathState` = 0, `MovementType` = 0 WHERE(guid = 1002772);
UPDATE creature SET `id` = 18256, `map` = 530, `spawnMask` = 0, `modelid` = 0, `equipment_id` = 0, `position_x` = -1540.56, `position_y` = 7995.44, `position_z` = -20.45, `orientation` = 0.947, `spawntimesecs` = 300, `spawndist` = 0, `currentwaypoint` = 0, `curhealth` = 1182800, `curmana` = 0, `DeathState` = 0, `MovementType` = 0 WHERE(guid = 1002773);
UPDATE creature SET `id` = 18256, `map` = 530, `spawnMask` = 0, `modelid` = 0, `equipment_id` = 0, `position_x` = -1546.95, `position_y` = 8000.85, `position_z` = -20.72, `orientation` = 6.035, `spawntimesecs` = 300, `spawndist` = 0, `currentwaypoint` = 0, `curhealth` = 1182800, `curmana` = 0, `DeathState` = 0, `MovementType` = 0 WHERE(guid = 1002774);
UPDATE creature SET `id` = 18256, `map` = 530, `spawnMask` = 0, `modelid` = 0, `equipment_id` = 0, `position_x` = -1595.31, `position_y` = 7860.53, `position_z` = -21.51, `orientation` = 3.747, `spawntimesecs` = 300, `spawndist` = 0, `currentwaypoint` = 0, `curhealth` = 1182800, `curmana` = 0, `DeathState` = 0, `MovementType` = 0 WHERE(guid = 1002775);
UPDATE creature SET `id` = 18256, `map` = 530, `spawnMask` = 0, `modelid` = 0, `equipment_id` = 0, `position_x` = -1642.31, `position_y` = 7995.59, `position_z` = -25.8, `orientation` = 3.317, `spawntimesecs` = 300, `spawndist` = 0, `currentwaypoint` = 0, `curhealth` = 1182800, `curmana` = 0, `DeathState` = 0, `MovementType` = 0 WHERE(guid = 1002776);
UPDATE creature SET `id` = 18256, `map` = 530, `spawnMask` = 0, `modelid` = 0, `equipment_id` = 0, `position_x` = -1545.46, `position_y` = 7995.35, `position_z` = -20.63, `orientation` = 1.094, `spawntimesecs` = 300, `spawndist` = 0, `currentwaypoint` = 0, `curhealth` = 1182800, `curmana` = 0, `DeathState` = 0, `MovementType` = 0 WHERE(guid = 1002777);
UPDATE creature SET `id` = 18256, `map` = 530, `spawnMask` = 0, `modelid` = 0, `equipment_id` = 0, `position_x` = -1487.58, `position_y` = 7907.99, `position_z` = -19.27, `orientation` = 5.567, `spawntimesecs` = 300, `spawndist` = 0, `currentwaypoint` = 0, `curhealth` = 1182800, `curmana` = 0, `DeathState` = 0, `MovementType` = 0 WHERE(guid = 1002778);
UPDATE creature SET `id` = 18256, `map` = 530, `spawnMask` = 0, `modelid` = 0, `equipment_id` = 0, `position_x` = -1651.54, `position_y` = 7988.56, `position_z` = -26.5289, `orientation` = 2.98451, `spawntimesecs` = 300, `spawndist` = 0, `currentwaypoint` = 0, `curhealth` = 1182800, `curmana` = 0, `DeathState` = 0, `MovementType` = 0 WHERE(guid = 1002779);
UPDATE creature SET `id` = 18256, `map` = 530, `spawnMask` = 0, `modelid` = 0, `equipment_id` = 0, `position_x` = -1602.46, `position_y` = 7866.43, `position_z` = -22.1177, `orientation` = 4.74729, `spawntimesecs` = 300, `spawndist` = 0, `currentwaypoint` = 0, `curhealth` = 1182800, `curmana` = 0, `DeathState` = 0, `MovementType` = 0 WHERE(guid = 1002780);
UPDATE creature SET `id` = 18256, `map` = 530, `spawnMask` = 0, `modelid` = 0, `equipment_id` = 0, `position_x` = -1591.22, `position_y` = 7875.29, `position_z` = -22.3536, `orientation` = 4.34587, `spawntimesecs` = 300, `spawndist` = 0, `currentwaypoint` = 0, `curhealth` = 1182800, `curmana` = 0, `DeathState` = 0, `MovementType` = 0 WHERE(guid = 1002781);
UPDATE creature SET `id` = 18256, `map` = 530, `spawnMask` = 0, `modelid` = 0, `equipment_id` = 0, `position_x` = -1550.6, `position_y` = 7944.45, `position_z` = -21.63, `orientation` = 3.559, `spawntimesecs` = 300, `spawndist` = 0, `currentwaypoint` = 0, `curhealth` = 1182800, `curmana` = 0, `DeathState` = 0, `MovementType` = 0 WHERE(guid = 1002782);
UPDATE creature SET `id` = 18256, `map` = 530, `spawnMask` = 0, `modelid` = 0, `equipment_id` = 0, `position_x` = -1545.57, `position_y` = 7935.83, `position_z` = -21.13, `orientation` = 3.448, `spawntimesecs` = 300, `spawndist` = 0, `currentwaypoint` = 0, `curhealth` = 1182800, `curmana` = 0, `DeathState` = 0, `MovementType` = 0 WHERE(guid = 1002783);
UPDATE creature SET `id` = 18256, `map` = 530, `spawnMask` = 0, `modelid` = 0, `equipment_id` = 0, `position_x` = -1550.86, `position_y` = 7937.56, `position_z` = -21.7, `orientation` = 3.801, `spawntimesecs` = 300, `spawndist` = 0, `currentwaypoint` = 0, `curhealth` = 1182800, `curmana` = 0, `DeathState` = 0, `MovementType` = 0 WHERE(guid = 1002784);

-- Horde Vendors
UPDATE creature SET `id` = 18816, `map` = 530, `spawnMask` = 0, `modelid` = 0, `equipment_id` = 0, `position_x` = -1523.92, `position_y` = 7951.76, `position_z` = -17.6942, `orientation` = 3.51172, `spawntimesecs` = 300, `spawndist` = 0, `currentwaypoint` = 0, `curhealth` = 1182800, `curmana` = 0, `DeathState` = 0, `MovementType` = 0 WHERE(guid = 1002785);
UPDATE creature SET `id` = 18821, `map` = 530, `spawnMask` = 0, `modelid` = 0, `equipment_id` = 0, `position_x` = -1527.75, `position_y` = 7952.46, `position_z` = -17.6948, `orientation` = 3.99317, `spawntimesecs` = 300, `spawndist` = 0, `currentwaypoint` = 0, `curhealth` = 1182800, `curmana` = 0, `DeathState` = 0, `MovementType` = 0 WHERE(guid = 1002786);
UPDATE creature SET `id` = 21474, `map` = 530, `spawnMask` = 0, `modelid` = 0, `equipment_id` = 0, `position_x` = -1520.14, `position_y` = 7927.11, `position_z` = -20.2527, `orientation` = 3.39389, `spawntimesecs` = 300, `spawndist` = 0, `currentwaypoint` = 0, `curhealth` = 1182800, `curmana` = 0, `DeathState` = 0, `MovementType` = 0 WHERE(guid = 1002787);
UPDATE creature SET `id` = 21484, `map` = 530, `spawnMask` = 0, `modelid` = 0, `equipment_id` = 0, `position_x` = -1524.84, `position_y` = 7930.34, `position_z` = -20.182, `orientation` = 3.6405, `spawntimesecs` = 300, `spawndist` = 0, `currentwaypoint` = 0, `curhealth` = 1182800, `curmana` = 0, `DeathState` = 0, `MovementType` = 0 WHERE(guid = 1002788);
UPDATE creature SET `id` = 21483, `map` = 530, `spawnMask` = 0, `modelid` = 0, `equipment_id` = 0, `position_x` = -1570.01, `position_y` = 7993.8, `position_z` = -22.4505, `orientation` = 5.02655, `spawntimesecs` = 300, `spawndist` = 0, `currentwaypoint` = 0, `curhealth` = 1182800, `curmana` = 0, `DeathState` = 0, `MovementType` = 0 WHERE(guid = 1002789);

-- Alliance Vendors
UPDATE creature SET `id` = 18817, `map` = 530, `spawnMask` = 0, `modelid` = 0, `equipment_id` = 0, `position_x` = -1591.18, `position_y` = 8020.39, `position_z` = -22.2042, `orientation` = 4.59022, `spawntimesecs` = 300, `spawndist` = 0, `currentwaypoint` = 0, `curhealth` = 1182800, `curmana` = 0, `DeathState` = 0, `MovementType` = 0 WHERE(guid = 1002790);
UPDATE creature SET `id` = 18822, `map` = 530, `spawnMask` = 0, `modelid` = 0, `equipment_id` = 0, `position_x` = -1588.0, `position_y` = 8019.0, `position_z` = -22.2042, `orientation` = 4.06662, `spawntimesecs` = 300, `spawndist` = 0, `currentwaypoint` = 0, `curhealth` = 1182800, `curmana` = 0, `DeathState` = 0, `MovementType` = 0 WHERE(guid = 1002791);
UPDATE creature SET `id` = 21485, `map` = 530, `spawnMask` = 0, `modelid` = 0, `equipment_id` = 0, `position_x` = -1521.93, `position_y` = 7927.37, `position_z` = -20.2299, `orientation` = 3.24631, `spawntimesecs` = 300, `spawndist` = 0, `currentwaypoint` = 0, `curhealth` = 1182800, `curmana` = 0, `DeathState` = 0, `MovementType` = 0 WHERE(guid = 1002792);
UPDATE creature SET `id` = 21487, `map` = 530, `spawnMask` = 0, `modelid` = 0, `equipment_id` = 0, `position_x` = -1540.33, `position_y` = 7971.95, `position_z` = -20.7186, `orientation` = 3.07178, `spawntimesecs` = 300, `spawndist` = 0, `currentwaypoint` = 0, `curhealth` = 1182800, `curmana` = 0, `DeathState` = 0, `MovementType` = 0 WHERE(guid = 1002793);
UPDATE creature SET `id` = 21488, `map` = 530, `spawnMask` = 0, `modelid` = 0, `equipment_id` = 0, `position_x` = -1570.01, `position_y` = 7993.8, `position_z` = -22.4505, `orientation` = 5.02655, `spawntimesecs` = 300, `spawndist` = 0, `currentwaypoint` = 0, `curhealth` = 1182800, `curmana` = 0, `DeathState` = 0, `MovementType` = 0 WHERE(guid = 1002794);

-- Halaa GameObject spawn data(guid range 67880 - 67999 used 67880 - 67891)

-- GO_WYVERN_ROOST_ALLIANCE
UPDATE gameobject  SET `id` = 182281, `map` = 530, `spawnMask` = 0, `position_x`= -1384.52, `position_y` = 7779.33, `position_z` = -11.1663, `orientation` = -0.575959, `rotation0` = 0, `rotation1` = 0, `rotation2` = 0.284015, `rotation3` = -0.95882, `spawntimesecs` = 180, `animprogress` = 255, `state`= 1 WHERE(guid = 67880);
UPDATE gameobject  SET `id` = 182282, `map` = 530, `spawnMask` = 0, `position_x`= -1650.11, `position_y` = 7732.56, `position_z` = -15.4505, `orientation` = -2.80998, `rotation0` = 0, `rotation1` = 0, `rotation2` = 0.986286, `rotation3` = -0.165048, `spawntimesecs` = 180, `animprogress` = 255, `state`= 1 WHERE(guid = 67881);

-- GO_WYVERN_ROOST_HORDE
UPDATE gameobject  SET `id` = 182273, `map` = 530, `spawnMask` = 0, `position_x`= -1377.95, `position_y` = 7773.44, `position_z` = -10.31, `orientation` = -0.575959, `rotation0` = 0, `rotation1` = 0, `rotation2` = 0.284015, `rotation3` = -0.95882, `spawntimesecs` = 180, `animprogress` = 255, `state`= 1 WHERE(guid = 67882);
UPDATE gameobject  SET `id` = 182274, `map` = 530, `spawnMask` = 0, `position_x`= -1659.87, `position_y` = 7733.15, `position_z` = -15.75, `orientation` = -2.80998, `rotation0` = 0, `rotation1` = 0, `rotation2` = 0.986286, `rotation3` = -0.165048, `spawntimesecs` = 180, `animprogress` = 255, `state`= 1 WHERE(guid = 67883);

-- GO_DESTROYED_ROOST_ALLIANCE
UPDATE gameobject  SET `id` = 182266, `map` = 530, `spawnMask` = 0, `position_x`= -1815.8, `position_y` = 8036.51, `position_z` = -26.2354, `orientation` = -2.89725, `rotation0` = 0, `rotation1` = 0, `rotation2` = 0.992546, `rotation3` = -0.121869, `spawntimesecs` = 180, `animprogress` = 255, `state`= 1 WHERE(guid = 67884);
UPDATE gameobject  SET `id` = 182275, `map` = 530, `spawnMask` = 0, `position_x`= -1507.95, `position_y` = 8132.1, `position_z` = -19.5585, `orientation` = -1.3439, `rotation0` = 0, `rotation1` = 0, `rotation2` = 0.622515, `rotation3` = -0.782608, `spawntimesecs` = 180, `animprogress` = 255, `state`= 1 WHERE(guid = 67885);
UPDATE gameobject  SET `id` = 182276, `map` = 530, `spawnMask` = 0, `position_x`= -1384.52, `position_y` = 7779.33, `position_z` = -11.1663, `orientation` = -0.575959, `rotation0` = 0, `rotation1` = 0, `rotation2` = 0.284015, `rotation3` = -0.95882, `spawntimesecs` = 180, `animprogress` = 255, `state`= 1 WHERE(guid = 67886);
UPDATE gameobject  SET `id` = 182277, `map` = 530, `spawnMask` = 0, `position_x`= -1650.11, `position_y` = 7732.56, `position_z` = -15.4505, `orientation` = -2.80998, `rotation0` = 0, `rotation1` = 0, `rotation2` = 0.986286, `rotation3` = -0.165048, `spawntimesecs` = 180, `animprogress` = 255, `state`= 1 WHERE(guid = 67887);

-- GO_DESTROYED_ROOST_HORDE
UPDATE gameobject  SET `id` = 182297, `map` = 530, `spawnMask` = 0, `position_x`= -1815.8, `position_y` = 8036.51, `position_z` = -26.2354, `orientation` = -2.89725, `rotation0` = 0, `rotation1` = 0, `rotation2` = 0.992546, `rotation3` = -0.121869, `spawntimesecs` = 180, `animprogress` = 255, `state`= 1 WHERE(guid = 67888);
UPDATE gameobject  SET `id` = 182298, `map` = 530, `spawnMask` = 0, `position_x`= -1507.95, `position_y` = 8132.1, `position_z` = -19.5585, `orientation` = -1.3439, `rotation0` = 0, `rotation1` = 0, `rotation2` = 0.622515, `rotation3` = -0.782608, `spawntimesecs` = 180, `animprogress` = 255, `state`= 1 WHERE(guid = 67889);
UPDATE gameobject  SET `id` = 182299, `map` = 530, `spawnMask` = 0, `position_x`= -1384.52, `position_y` = 7779.33, `position_z` = -11.1663, `orientation` = -0.575959, `rotation0` = 0, `rotation1` = 0, `rotation2` = 0.284015, `rotation3` = -0.95882, `spawntimesecs` = 180, `animprogress` = 255, `state`= 1 WHERE(guid = 67890);
UPDATE gameobject  SET `id` = 182300, `map` = 530, `spawnMask` = 0, `position_x`= -1650.11, `position_y` = 7732.56, `position_z` = -15.4505, `orientation` = -2.80998, `rotation0` = 0, `rotation1` = 0, `rotation2` = 0.986286, `rotation3` = -0.165048, `spawntimesecs` = 180, `animprogress` = 255, `state`= 1 WHERE(guid = 67891);

-- Set respawntime to -1 so gameobjects are spawned hidden
UPDATE gameobject SET `spawntimesecs` = -1 WHERE (id=182267 OR id=182280 OR id=182281 OR id= 182282 OR id=182222 OR id=182272 OR id= 182273 OR id= 182274 OR id=182266 OR id=182275 OR id=182276 OR id =182277 OR id =182301 OR id =182302 OR id = 182303 OR id = 182304 OR id = 182305 OR id = 182306 OR id = 182307 OR id =182308 OR id =182297 OR id =182298 OR id =182299 OR id =182300);

-- spell updates for the Bombing spell( 31961)
UPDATE spell_dbc SET Effect2 = 3 WHERE(Id = 31958);
UPDATE spell_dbc SET EffectImplicitTargetA1 = 16, EffectImplicitTargetB1 = 0, EffectImplicitTargetA2 = 16, EffectImplicitTargetB2 = 0, Effect3 = 0, Effect2 = 27 WHERE(Id = 31961);
