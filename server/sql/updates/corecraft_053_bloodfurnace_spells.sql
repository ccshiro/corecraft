-- Flags just copied from a similar spell that does work
UPDATE spell_dbc SET AuraInterruptFlags=0x103F, ChannelInterruptFlags=0x101B WHERE Id=35766;

UPDATE spell_dbc SET AuraInterruptFlags = AuraInterruptFlags | 0x20 WHERE Id=8734 OR Id=31059;
