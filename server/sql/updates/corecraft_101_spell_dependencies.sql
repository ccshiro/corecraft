-- world

CREATE TABLE `spell_dependencies`
(
`dependency_id` INT(11) UNSIGNED NOT NULL DEFAULT 0,
`spell_id` INT(11) UNSIGNED NOT NULL DEFAULT 0
);

-- The spells that prompted this table to be added (more might exist)
INSERT INTO spell_dependencies VALUES
(20217, 25898), -- Blessing of Kings
(20914, 25899), -- Blessing of Sanctuary (Greater r1)
(27168, 27169), -- Blessing of Sanctuary (Greater r2)
(37116, 16958), -- Primal Fury r1; Primal Fury
(37116, 16952), -- Primal Fury r1; Blood Frenzy
(37117, 16961), -- Primal Fury r2; Primal Fury
(37117, 16954); -- Primal Fury r2; Blood Frenzy
