DROP TABLE IF EXISTS spell_los_ignore;
CREATE TABLE spell_los_ignore
(
  id INT(11) UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY(id)
);

-- Copy the data of mangosd.conf.dist.in as it was when this patch was created
INSERT INTO spell_los_ignore (id) VALUES
  (7720),
  (10909),
  (29511),
  (30128),
  (30282),
  (30522),
  (30834),
  (30967),
  (30977),
  (32264),
  (33654),
  (33671),
  (33684),
  (35059),
  (37098),
  (38194),
  (38203),
  (38523),
  (40424);
