-- Set the SPELL_ATTR_EX5_START_PERIODIC_AT_APPLY flag to make totems pulse instantly
UPDATE spell_dbc SET AttributesEx5 = AttributesEx5 | 0x200 WHERE id = 6474;     -- Earthbind totem passive
UPDATE spell_dbc SET AttributesEx5 = AttributesEx5 | 0x200 WHERE id = 8145;     -- Tremor totem passive
UPDATE spell_dbc SET AttributesEx5 = AttributesEx5 | 0x200 WHERE id = 8167;     -- Poison cleansing totem passive
UPDATE spell_dbc SET AttributesEx5 = AttributesEx5 | 0x200 WHERE id = 8172;     -- Disease cleansing totem passive
UPDATE spell_dbc SET AttributesEx5 = AttributesEx5 | 0x200 WHERE id = 5728;     -- Stoneclaw totem passive (Rank 1)
UPDATE spell_dbc SET AttributesEx5 = AttributesEx5 | 0x200 WHERE id = 6397;     -- Stoneclaw totem passive (Rank 2)
UPDATE spell_dbc SET AttributesEx5 = AttributesEx5 | 0x200 WHERE id = 6398;     -- Stoneclaw totem passive (Rank 3)
UPDATE spell_dbc SET AttributesEx5 = AttributesEx5 | 0x200 WHERE id = 6399;     -- Stoneclaw totem passive (Rank 4)
UPDATE spell_dbc SET AttributesEx5 = AttributesEx5 | 0x200 WHERE id = 10425;    -- Stoneclaw totem passive (Rank 5)
UPDATE spell_dbc SET AttributesEx5 = AttributesEx5 | 0x200 WHERE id = 10426;    -- Stoneclaw totem passive (Rank 6)
UPDATE spell_dbc SET AttributesEx5 = AttributesEx5 | 0x200 WHERE id = 25513;    -- Stoneclaw totem passive (Rank 7)
UPDATE spell_dbc SET AttributesEx5 = AttributesEx5 | 0x200 WHERE id = 8229;     -- Flametongue Totem Passive (Rank 1)
UPDATE spell_dbc SET AttributesEx5 = AttributesEx5 | 0x200 WHERE id = 8251;     -- Flametongue totem passive (Rank 2)
UPDATE spell_dbc SET AttributesEx5 = AttributesEx5 | 0x200 WHERE id = 10524;    -- Flametongue totem passive (Rank 3)
UPDATE spell_dbc SET AttributesEx5 = AttributesEx5 | 0x200 WHERE id = 16388;    -- Flametongue totem passive (Rank 4)
UPDATE spell_dbc SET AttributesEx5 = AttributesEx5 | 0x200 WHERE id = 25556;    -- Flametongue totem passive (Rank 5)
UPDATE spell_dbc SET AttributesEx5 = AttributesEx5 | 0x200 WHERE id = 8515;     -- Windfury totem passive (Rank 1)
UPDATE spell_dbc SET AttributesEx5 = AttributesEx5 | 0x200 WHERE id = 10609;    -- Windfury totem passive (Rank 2)
UPDATE spell_dbc SET AttributesEx5 = AttributesEx5 | 0x200 WHERE id = 10612;    -- Windfury totem passive (Rank 3)
UPDATE spell_dbc SET AttributesEx5 = AttributesEx5 | 0x200 WHERE id = 25581;    -- Windfury totem passive (Rank 4)
UPDATE spell_dbc SET AttributesEx5 = AttributesEx5 | 0x200 WHERE id = 25582;    -- Windfury totem passive (Rank 5)

