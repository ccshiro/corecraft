-- Jackhammer bug fixing:
-- Remove Interrupt Flags
UPDATE spell_dbc SET InterruptFlags = 0, AuraInterruptFlags = 0 WHERE Id=39194 OR Id=35327;
-- Remove cooldown and category of the procced spell
UPDATE spell_dbc SET Category=0, CategoryRecoveryTime=0 WHERE Id=39195 OR Id=35330;
